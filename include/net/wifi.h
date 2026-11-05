#ifndef ETHOS_NET_WIFI_H
#define ETHOS_NET_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ETHOS_AX201_FIRMWARE_MODULE "iwlwifi-QuZ-a0-jf-b0-77.ucode"

struct wifi_device_info {
  uint16_t vendor_id;
  uint16_t device_id;
  bool detected;
  bool firmware_available;
  size_t firmware_size;
  bool radio_ready;
};

bool iwl_ax201_probe(struct wifi_device_info *info);
bool iwl_ax201_get_firmware(const uint8_t **image, size_t *size);

#endif
