#include <net/wifi.h>
#include <arch/x86_64/pci.h>
#include <boot/boot.h>

#define INTEL_VENDOR_ID 0x8086U
#define INTEL_AX201_DEVICE_ID 0xA0F0U

static bool module_name_matches(const char *left, const char *right)
{
  if (left == 0 || right == 0)
  {
    return false;
  }
  size_t index = 0;
  while (left[index] != '\0' && right[index] != '\0')
  {
    if (left[index] != right[index])
    {
      return false;
    }
    index++;
  }
  return left[index] == right[index];
}

bool iwl_ax201_get_firmware(const uint8_t **image, size_t *size)
{
  if (image == 0 || size == 0)
  {
    return false;
  }
  *image = 0;
  *size = 0;

  struct limine_module_response *modules = boot_modules();
  if (modules == 0 || modules->modules == 0)
  {
    return false;
  }
  for (uint64_t index = 0; index < modules->module_count; index++)
  {
    struct limine_file *module = modules->modules[index];
    if (module != 0 && module->address != 0 && module->size != 0 &&
        module_name_matches(module->string, ETHOS_AX201_FIRMWARE_MODULE))
    {
      *image = module->address;
      *size = (size_t)module->size;
      return true;
    }
  }
  return false;
}

bool iwl_ax201_probe(struct wifi_device_info *info)
{
  if (info == 0)
  {
    return false;
  }
  *info = (struct wifi_device_info) {
    .vendor_id = INTEL_VENDOR_ID,
    .device_id = INTEL_AX201_DEVICE_ID
  };

  const uint8_t *firmware_image;
  size_t firmware_size;
  if (iwl_ax201_get_firmware(&firmware_image, &firmware_size))
  {
    (void)firmware_image;
    info->firmware_available = true;
    info->firmware_size = firmware_size;
  }

  struct pci_device device;
  info->detected = pci_find_device(INTEL_VENDOR_ID,
      INTEL_AX201_DEVICE_ID, &device);
  return true;
}
