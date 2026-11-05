#include <net/ethernet.h>
#include <arch/x86_64/pci.h>
#include <boot/boot.h>
#include <debug/serial.h>
#include <mm/paging.h>
#include <lib/memory.h>

#define PCI_COMMAND_OFFSET 0x04U
#define PCI_BAR0_OFFSET 0x10U
#define PCI_VENDOR_INTEL 0x8086U
#define PCI_DEVICE_82540EM 0x100EU
#define E1000_MMIO_SIZE 0x20000U
#define E1000_MMIO_VIRTUAL_BASE 0xFFFFFE0000000000ULL
#define E1000_RING_COUNT 16U
#define E1000_FRAME_CAPACITY 2048U

#define E1000_REG_CTRL 0x0000U
#define E1000_REG_STATUS 0x0008U
#define E1000_REG_EERD 0x0014U
#define E1000_REG_IMC 0x00D8U
#define E1000_REG_RCTL 0x0100U
#define E1000_REG_TCTL 0x0400U
#define E1000_REG_TIPG 0x0410U
#define E1000_REG_RAL 0x5400U
#define E1000_REG_RAH 0x5404U
#define E1000_REG_RDBAL 0x2800U
#define E1000_REG_RDBAH 0x2804U
#define E1000_REG_RDLEN 0x2808U
#define E1000_REG_RDH 0x2810U
#define E1000_REG_RDT 0x2818U
#define E1000_REG_TDBAL 0x3800U
#define E1000_REG_TDBAH 0x3804U
#define E1000_REG_TDLEN 0x3808U
#define E1000_REG_TDH 0x3810U
#define E1000_REG_TDT 0x3818U

#define E1000_CTRL_RESET (1U << 26)
#define E1000_STATUS_LINK_UP (1U << 1)
#define E1000_RCTL_ENABLE (1U << 1)
#define E1000_RCTL_BROADCAST_ACCEPT (1U << 15)
#define E1000_RCTL_STRIP_CRC (1U << 26)
#define E1000_TCTL_ENABLE (1U << 1)
#define E1000_TCTL_PAD_SHORT_PACKETS (1U << 3)
#define E1000_TX_CMD_EOP (1U << 0)
#define E1000_TX_CMD_INSERT_FCS (1U << 1)
#define E1000_TX_CMD_REPORT_STATUS (1U << 3)
#define E1000_DESC_DONE (1U << 0)

struct __attribute__((packed)) e1000_rx_descriptor {
  uint64_t buffer_address;
  uint16_t length;
  uint16_t checksum;
  uint8_t status;
  uint8_t errors;
  uint16_t special;
};

struct __attribute__((packed)) e1000_tx_descriptor {
  uint64_t buffer_address;
  uint16_t length;
  uint8_t checksum_offset;
  uint8_t command;
  uint8_t status;
  uint8_t checksum_start;
  uint16_t special;
};

_Static_assert(sizeof(struct e1000_rx_descriptor) == 16,
    "E1000 receive descriptor must be 16 bytes");
_Static_assert(sizeof(struct e1000_tx_descriptor) == 16,
    "E1000 transmit descriptor must be 16 bytes");

static volatile uint32_t *registers;
static volatile struct e1000_rx_descriptor *receive_ring;
static volatile struct e1000_tx_descriptor *transmit_ring;
static void *receive_buffers[E1000_RING_COUNT];
static void *transmit_buffer;
static uint8_t device_mac[6];
static size_t receive_index;
static size_t transmit_index;
static uint64_t hhdm_offset;
static bool initialized;

static void serial_write_hex_byte(uint8_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  serial_write_char(digits[value >> 4]);
  serial_write_char(digits[value & 0x0FU]);
}

static bool find_e1000(struct pci_device *device_out, uint64_t *bar_out)
{
  if (!pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_82540EM, device_out))
  {
    return false;
  }

  uint32_t bar_low = pci_read_config32(device_out, PCI_BAR0_OFFSET);
  if ((bar_low & 1U) != 0)
  {
    return false;
  }
  uint64_t bar = bar_low & ~0xFULL;
  if ((bar_low & 0x6U) == 0x4U)
  {
    bar |= (uint64_t)pci_read_config32(device_out,
        PCI_BAR0_OFFSET + 4U) << 32;
  }
  if (bar == 0)
  {
    return false;
  }
  *bar_out = bar;
  return true;
}

static uint64_t dma_physical_address(const volatile void *buffer)
{
  uintptr_t address = (uintptr_t)buffer;
  return address >= hhdm_offset ? address - hhdm_offset : 0;
}

static bool map_mmio(uint64_t physical_base)
{
  uint64_t page_base = physical_base & ~(uint64_t)(PAGING_PAGE_SIZE - 1U);
  uint64_t offset = physical_base - page_base;
  uint64_t map_size = E1000_MMIO_SIZE + offset;
  size_t page_count = (size_t)((map_size + PAGING_PAGE_SIZE - 1U) /
      PAGING_PAGE_SIZE);
  uintptr_t virtual_base = (uintptr_t)E1000_MMIO_VIRTUAL_BASE;
  size_t mapped = 0;

  for (; mapped < page_count; mapped++)
  {
    uintptr_t virtual_page = virtual_base + mapped * PAGING_PAGE_SIZE;
    if (!paging_map_page(paging_current_address_space(), virtual_page,
        page_base + mapped * PAGING_PAGE_SIZE,
        PAGING_FLAG_WRITE | PAGING_FLAG_WRITE_THROUGH |
            PAGING_FLAG_CACHE_DISABLE))
    {
      while (mapped != 0)
      {
        mapped--;
        (void)paging_unmap_page(paging_current_address_space(),
            virtual_base + mapped * PAGING_PAGE_SIZE);
      }
      return false;
    }
  }
  registers = (volatile uint32_t *)(virtual_base + offset);
  return true;
}

static uint32_t read_register(uint32_t offset)
{
  return registers[offset / sizeof(uint32_t)];
}

static void write_register(uint32_t offset, uint32_t value)
{
  registers[offset / sizeof(uint32_t)] = value;
}

static bool read_eeprom_word(uint8_t address, uint16_t *value)
{
  write_register(E1000_REG_EERD, 1U | ((uint32_t)address << 8));
  for (uint32_t attempt = 0; attempt < 1000000U; attempt++)
  {
    uint32_t result = read_register(E1000_REG_EERD);
    if ((result & (1U << 4)) != 0)
    {
      *value = (uint16_t)(result >> 16);
      return true;
    }
  }
  return false;
}

static bool read_mac_address(void)
{
  uint16_t word;
  for (uint8_t index = 0; index < 3U; index++)
  {
    if (!read_eeprom_word(index, &word))
    {
      uint32_t low = read_register(E1000_REG_RAL);
      uint32_t high = read_register(E1000_REG_RAH);
      device_mac[0] = (uint8_t)low;
      device_mac[1] = (uint8_t)(low >> 8);
      device_mac[2] = (uint8_t)(low >> 16);
      device_mac[3] = (uint8_t)(low >> 24);
      device_mac[4] = (uint8_t)high;
      device_mac[5] = (uint8_t)(high >> 8);
      return true;
    }
    device_mac[index * 2U] = (uint8_t)word;
    device_mac[index * 2U + 1U] = (uint8_t)(word >> 8);
  }
  return true;
}

static bool allocate_dma_resources(void)
{
  receive_ring = paging_alloc_page();
  transmit_ring = paging_alloc_page();
  transmit_buffer = paging_alloc_page();
  if (receive_ring == 0 || transmit_ring == 0 || transmit_buffer == 0)
  {
    return false;
  }
  for (size_t index = 0; index < E1000_RING_COUNT; index++)
  {
    receive_buffers[index] = paging_alloc_page();
    if (receive_buffers[index] == 0)
    {
      return false;
    }
    receive_ring[index].buffer_address = dma_physical_address(receive_buffers[index]);
    receive_ring[index].status = 0;
  }
  for (size_t index = 0; index < E1000_RING_COUNT; index++)
  {
    transmit_ring[index].status = E1000_DESC_DONE;
  }
  return true;
}

static void setup_receive_ring(void)
{
  uint64_t ring_address = dma_physical_address(receive_ring);
  write_register(E1000_REG_RDBAL, (uint32_t)ring_address);
  write_register(E1000_REG_RDBAH, (uint32_t)(ring_address >> 32));
  write_register(E1000_REG_RDLEN,
      E1000_RING_COUNT * sizeof(struct e1000_rx_descriptor));
  write_register(E1000_REG_RDH, 0);
  write_register(E1000_REG_RDT, E1000_RING_COUNT - 1U);
  write_register(E1000_REG_RCTL, E1000_RCTL_ENABLE |
      E1000_RCTL_BROADCAST_ACCEPT | E1000_RCTL_STRIP_CRC);
}

static void setup_transmit_ring(void)
{
  uint64_t ring_address = dma_physical_address(transmit_ring);
  write_register(E1000_REG_TDBAL, (uint32_t)ring_address);
  write_register(E1000_REG_TDBAH, (uint32_t)(ring_address >> 32));
  write_register(E1000_REG_TDLEN,
      E1000_RING_COUNT * sizeof(struct e1000_tx_descriptor));
  write_register(E1000_REG_TDH, 0);
  write_register(E1000_REG_TDT, 0);
  write_register(E1000_REG_TIPG, 0x0060200AU);
  write_register(E1000_REG_TCTL, E1000_TCTL_ENABLE |
      E1000_TCTL_PAD_SHORT_PACKETS | 0x00040100U);
}

bool ethernet_init(void)
{
  if (initialized)
  {
    return false;
  }
  hhdm_offset = boot_hhdm_offset();
  if (hhdm_offset == 0)
  {
    return false;
  }

  struct pci_device pci_device;
  uint64_t bar;
  if (!find_e1000(&pci_device, &bar) || !map_mmio(bar))
  {
    return false;
  }

  uint32_t command = pci_read_config32(&pci_device, PCI_COMMAND_OFFSET);
  pci_write_config32(&pci_device, PCI_COMMAND_OFFSET,
      (command & 0xFFFFU) | 0x0006U);

  write_register(E1000_REG_IMC, 0xFFFFFFFFU);
  (void)read_register(E1000_REG_STATUS);
  write_register(E1000_REG_CTRL,
      read_register(E1000_REG_CTRL) | E1000_CTRL_RESET);
  for (volatile uint32_t delay = 0; delay < 100000U; delay++)
  {
    __asm__ volatile ("pause");
  }
  write_register(E1000_REG_IMC, 0xFFFFFFFFU);
  (void)read_register(E1000_REG_STATUS);

  if (!read_mac_address() || !allocate_dma_resources())
  {
    return false;
  }
  setup_receive_ring();
  setup_transmit_ring();
  initialized = true;
  serial_write("SUCCESS >> E1000 eth0 MAC=");
  for (size_t index = 0; index < sizeof(device_mac); index++)
  {
    if (index != 0)
    {
      serial_write_char(':');
    }
    serial_write_hex_byte(device_mac[index]);
  }
  serial_write((read_register(E1000_REG_STATUS) & E1000_STATUS_LINK_UP) != 0
      ? " link=up\n" : " link=down\n");
  return true;
}

bool ethernet_get_info(struct ethernet_device_info *info)
{
  if (!initialized || info == 0)
  {
    return false;
  }
  memcpy(info->mac_address, device_mac, sizeof(device_mac));
  info->link_up = (read_register(E1000_REG_STATUS) & E1000_STATUS_LINK_UP) != 0;
  return true;
}

bool ethernet_send_frame(const void *frame, size_t length)
{
  if (!initialized || frame == 0 || length < 14U ||
      length > E1000_FRAME_CAPACITY)
  {
    return false;
  }

  volatile struct e1000_tx_descriptor *descriptor =
      &transmit_ring[transmit_index];
  for (uint32_t attempt = 0; attempt < 1000000U &&
      (descriptor->status & E1000_DESC_DONE) == 0; attempt++)
  {
    __asm__ volatile ("pause");
  }
  if ((descriptor->status & E1000_DESC_DONE) == 0)
  {
    return false;
  }

  memcpy(transmit_buffer, frame, length);
  descriptor->buffer_address = dma_physical_address(transmit_buffer);
  descriptor->length = (uint16_t)length;
  descriptor->checksum_offset = 0;
  descriptor->command = E1000_TX_CMD_EOP | E1000_TX_CMD_INSERT_FCS |
      E1000_TX_CMD_REPORT_STATUS;
  descriptor->status = 0;
  descriptor->checksum_start = 0;
  descriptor->special = 0;
  __asm__ volatile ("" ::: "memory");

  size_t next = (transmit_index + 1U) % E1000_RING_COUNT;
  write_register(E1000_REG_TDT, (uint32_t)next);
  for (uint32_t attempt = 0; attempt < 1000000U &&
      (descriptor->status & E1000_DESC_DONE) == 0; attempt++)
  {
    __asm__ volatile ("pause");
  }
  if ((descriptor->status & E1000_DESC_DONE) == 0)
  {
    return false;
  }
  transmit_index = next;
  return true;
}

bool ethernet_receive_frame(void *buffer, size_t capacity, size_t *length)
{
  if (!initialized || buffer == 0 || length == 0)
  {
    return false;
  }

  volatile struct e1000_rx_descriptor *descriptor =
      &receive_ring[receive_index];
  if ((descriptor->status & E1000_DESC_DONE) == 0)
  {
    return false;
  }
  __asm__ volatile ("" ::: "memory");
  size_t frame_length = descriptor->length;
  bool valid = descriptor->errors == 0 && frame_length <= capacity;
  if (valid)
  {
    memcpy(buffer, receive_buffers[receive_index], frame_length);
    *length = frame_length;
  }
  descriptor->status = 0;
  write_register(E1000_REG_RDT, (uint32_t)receive_index);
  receive_index = (receive_index + 1U) % E1000_RING_COUNT;
  return valid;
}
