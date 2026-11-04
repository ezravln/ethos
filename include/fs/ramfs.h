#ifndef ETHOS_FS_RAMFS_H
#define ETHOS_FS_RAMFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAMFS_PATH_MAX 127U
#define RAMFS_NAME_MAX RAMFS_PATH_MAX
#define RAMFS_FILE_CAPACITY 4096U

struct ramfs_file_info {
  char path[RAMFS_PATH_MAX + 1U];
  size_t size;
};

struct ramfs_directory_info {
  char path[RAMFS_PATH_MAX + 1U];
  bool read_only;
};

bool ramfs_init(void);
bool ramfs_is_initialized(void);
bool ramfs_mkdir(const char *path);
bool ramfs_is_directory(const char *path);
size_t ramfs_directory_count(void);
bool ramfs_directory_get_at(size_t index, struct ramfs_directory_info *info);
bool ramfs_create(const char *path);
bool ramfs_remove(const char *path);
bool ramfs_truncate(const char *path, size_t size);
bool ramfs_write(const char *path, size_t offset, const void *data, size_t size);
bool ramfs_read(
    const char *path,
    size_t offset,
    void *buffer,
    size_t capacity,
    size_t *bytes_read
);
bool ramfs_stat(const char *path, struct ramfs_file_info *info);
size_t ramfs_count(void);
bool ramfs_get_at(size_t index, struct ramfs_file_info *info);

#endif