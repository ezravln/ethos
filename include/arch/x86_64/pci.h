#ifndef ETHOS_ARCH_X86_64_PCI_H
#define ETHOS_ARCH_X86_64_PCI_H

#include <stdbool.h>
#include <stdint.h>

struct pci_device {
  uint8_t bus;
  uint8_t device;
  uint8_t function;
  uint16_t vendor_id;
  uint16_t device_id;
};

bool pci_find_device(uint16_t vendor_id, uint16_t device_id,
    struct pci_device *result);
uint32_t pci_read_config32(const struct pci_device *device, uint8_t offset);
void pci_write_config32(const struct pci_device *device, uint8_t offset,
    uint32_t value);

#endif
