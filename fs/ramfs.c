#include <fs/ramfs.h>
#include <lib/memory.h>

#define RAMFS_FILE_COUNT 16U

struct ramfs_file {
  char name[RAMFS_NAME_MAX + 1U];
  size_t size;
  bool used;
};

static struct ramfs_file files[RAMFS_FILE_COUNT];
static uint8_t file_data[RAMFS_FILE_COUNT][RAMFS_FILE_CAPACITY];
static bool initialized;

static uint64_t interrupt_disable(void)
{
  uint64_t flags;
  __asm__ volatile (
    "pushfq\n"
    "popq %0\n"
    "cli"
    : "=r"(flags)
    :
    : "memory"
  );
  return flags;
}

static void interrupt_restore(uint64_t flags)
{
  if ((flags & (1ULL << 9)) != 0)
  {
    __asm__ volatile ("sti" ::: "memory");
  }
}

static bool parse_path(const char *path, char name[RAMFS_NAME_MAX + 1U])
{
  if (path == 0 || name == 0 || path[0] != '/' || path[1] == '\0')
  {
    return false;
  }

  size_t length = 0;
  while (path[length + 1U] != '\0')
  {
    unsigned char character = (unsigned char)path[length + 1U];
    if (length >= RAMFS_NAME_MAX || character < 0x21U || character > 0x7EU ||
        character == '/')
    {
      return false;
    }
    name[length] = (char)character;
    length++;
  }
  name[length] = '\0';

  if ((length == 1U && name[0] == '.') ||
      (length == 2U && name[0] == '.' && name[1] == '.'))
  {
    return false;
  }
  return true;
}

static bool names_equal(const char *left, const char *right)
{
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

static size_t find_file(const char *name)
{
  for (size_t index = 0; index < RAMFS_FILE_COUNT; index++)
  {
    if (files[index].used && names_equal(files[index].name, name))
    {
      return index;
    }
  }
  return RAMFS_FILE_COUNT;
}

bool ramfs_init(void)
{
  uint64_t flags = interrupt_disable();
  if (initialized)
  {
    interrupt_restore(flags);
    return false;
  }
  memset(files, 0, sizeof(files));
  memset(file_data, 0, sizeof(file_data));
  initialized = true;
  interrupt_restore(flags);
  return true;
}

bool ramfs_is_initialized(void)
{
  return initialized;
}

bool ramfs_create(const char *path)
{
  char name[RAMFS_NAME_MAX + 1U];
  if (!initialized || !parse_path(path, name))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  if (find_file(name) != RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }

  size_t slot = 0;
  while (slot < RAMFS_FILE_COUNT && files[slot].used)
  {
    slot++;
  }
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }

  memcpy(files[slot].name, name, sizeof(name));
  files[slot].size = 0;
  files[slot].used = true;
  interrupt_restore(flags);
  return true;
}

bool ramfs_remove(const char *path)
{
  char name[RAMFS_NAME_MAX + 1U];
  if (!initialized || !parse_path(path, name))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(name);
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }

  memset(&files[slot], 0, sizeof(files[slot]));
  memset(file_data[slot], 0, sizeof(file_data[slot]));
  interrupt_restore(flags);
  return true;
}

bool ramfs_write(const char *path, size_t offset, const void *data, size_t size)
{
  char name[RAMFS_NAME_MAX + 1U];
  if (!initialized || !parse_path(path, name) ||
      (size != 0 && data == 0) || offset > RAMFS_FILE_CAPACITY ||
      size > RAMFS_FILE_CAPACITY - offset)
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(name);
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }

  if (offset > files[slot].size)
  {
    memset(file_data[slot] + files[slot].size, 0,
        offset - files[slot].size);
  }
  if (size != 0)
  {
    memcpy(file_data[slot] + offset, data, size);
  }
  size_t end = offset + size;
  if (end > files[slot].size)
  {
    files[slot].size = end;
  }
  interrupt_restore(flags);
  return true;
}

bool ramfs_read(
    const char *path,
    size_t offset,
    void *buffer,
    size_t capacity,
    size_t *bytes_read
)
{
  char name[RAMFS_NAME_MAX + 1U];
  if (!initialized || !parse_path(path, name) ||
      (capacity != 0 && buffer == 0))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(name);
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }

  size_t count = 0;
  if (offset < files[slot].size)
  {
    count = files[slot].size - offset;
    if (count > capacity)
    {
      count = capacity;
    }
    if (count != 0)
    {
      memcpy(buffer, file_data[slot] + offset, count);
    }
  }
  if (bytes_read != 0)
  {
    *bytes_read = count;
  }
  interrupt_restore(flags);
  return true;
}

bool ramfs_stat(const char *path, struct ramfs_file_info *info)
{
  char name[RAMFS_NAME_MAX + 1U];
  if (!initialized || info == 0 || !parse_path(path, name))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(name);
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }
  memcpy(info->name, files[slot].name, sizeof(info->name));
  info->size = files[slot].size;
  interrupt_restore(flags);
  return true;
}

size_t ramfs_count(void)
{
  uint64_t flags = interrupt_disable();
  size_t count = 0;
  for (size_t index = 0; index < RAMFS_FILE_COUNT; index++)
  {
    if (files[index].used)
    {
      count++;
    }
  }
  interrupt_restore(flags);
  return count;
}

bool ramfs_get_at(size_t index, struct ramfs_file_info *info)
{
  if (!initialized || info == 0)
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t active_index = 0;
  for (size_t slot = 0; slot < RAMFS_FILE_COUNT; slot++)
  {
    if (!files[slot].used)
    {
      continue;
    }
    if (active_index++ != index)
    {
      continue;
    }
    memcpy(info->name, files[slot].name, sizeof(info->name));
    info->size = files[slot].size;
    interrupt_restore(flags);
    return true;
  }
  interrupt_restore(flags);
  return false;
}