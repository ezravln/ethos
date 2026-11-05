#ifndef ETHOS_NET_ETHERNET_H
#define ETHOS_NET_ETHERNET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ethernet_device_info {
  uint8_t mac_address[6];
  bool link_up;
};

bool ethernet_init(void);
bool ethernet_get_info(struct ethernet_device_info *info);
bool ethernet_send_frame(const void *frame, size_t length);
bool ethernet_receive_frame(void *buffer, size_t capacity, size_t *length);

#endif
