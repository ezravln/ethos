#ifndef ETHOS_PROC_APX_H
#define ETHOS_PROC_APX_H

#include <stdint.h>

#define ETHOS_APX_MAGIC {'E', 'T', 'H', 'O', 'S', 'A', 'P', 'X'}
#define ETHOS_APX_MAGIC_SIZE 8U
#define ETHOS_APX_VERSION 1U
#define ETHOS_APX_ARCH_X86_64 1U

struct __attribute__((packed)) apx_header {
  uint8_t magic[ETHOS_APX_MAGIC_SIZE];
  uint16_t version;
  uint16_t header_size;
  uint16_t architecture;
  uint16_t flags;
};

#endif