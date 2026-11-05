#include <arch/x86_64/pci.h>

#define PCI_CONFIG_ADDRESS 0xCF8U
#define PCI_CONFIG_DATA 0xCFCU

static inline void outl(uint16_t port, uint32_t value)
{
  __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
  uint32_t value;
  __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

static uint32_t pci_read_location(uint8_t bus, uint8_t device,
    uint8_t function, uint8_t offset)
{
  uint32_t address = 0x80000000U | ((uint32_t)bus << 16) |
      ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFCU);
  outl(PCI_CONFIG_ADDRESS, address);
  return inl(PCI_CONFIG_DATA);
}

uint32_t pci_read_config32(const struct pci_device *device, uint8_t offset)
{
  if (device == 0)
  {
    return 0xFFFFFFFFU;
  }
  return pci_read_location(device->bus, device->device, device->function,
      offset);
}

void pci_write_config32(const struct pci_device *device, uint8_t offset,
    uint32_t value)
{
  if (device == 0)
  {
    return;
  }
  uint32_t address = 0x80000000U | ((uint32_t)device->bus << 16) |
      ((uint32_t)device->device << 11) |
      ((uint32_t)device->function << 8) | (offset & 0xFCU);
  outl(PCI_CONFIG_ADDRESS, address);
  outl(PCI_CONFIG_DATA, value);
}

bool pci_find_device(uint16_t vendor_id, uint16_t device_id,
    struct pci_device *result)
{
  if (result == 0)
  {
    return false;
  }
  for (uint32_t bus = 0; bus < 256U; bus++)
  {
    for (uint32_t device = 0; device < 32U; device++)
    {
      for (uint32_t function = 0; function < 8U; function++)
      {
        uint32_t identity = pci_read_location((uint8_t)bus,
            (uint8_t)device, (uint8_t)function, 0);
        uint16_t found_vendor = (uint16_t)identity;
        uint16_t found_device = (uint16_t)(identity >> 16);
        if (found_vendor == 0xFFFFU)
        {
          if (function == 0)
          {
            break;
          }
          continue;
        }
        if (found_vendor == vendor_id && found_device == device_id)
        {
          *result = (struct pci_device) {
            .bus = (uint8_t)bus,
            .device = (uint8_t)device,
            .function = (uint8_t)function,
            .vendor_id = found_vendor,
            .device_id = found_device
          };
          return true;
        }
      }
    }
  }
  return false;
}
