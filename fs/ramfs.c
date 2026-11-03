#include <fs/ramfs.h>
#include <lib/memory.h>

#define RAMFS_FILE_COUNT 16U
#define RAMFS_DIRECTORY_COUNT 64U

struct ramfs_file {
  char path[RAMFS_PATH_MAX + 1U];
  size_t size;
  bool used;
};

struct ramfs_directory {
  char path[RAMFS_PATH_MAX + 1U];
  bool read_only;
  bool used;
};

static struct ramfs_file files[RAMFS_FILE_COUNT];
static uint8_t file_data[RAMFS_FILE_COUNT][RAMFS_FILE_CAPACITY];
static struct ramfs_directory directories[RAMFS_DIRECTORY_COUNT];
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

static bool parse_path(
    const char *input,
    char path[RAMFS_PATH_MAX + 1U],
    bool allow_root
)
{
  if (input == 0 || path == 0 || input[0] != '/')
  {
    return false;
  }

  memset(path, 0, RAMFS_PATH_MAX + 1U);
  size_t length = 0;
  while (input[length] != '\0')
  {
    unsigned char character = (unsigned char)input[length];
    if (length >= RAMFS_PATH_MAX ||
        (character != '/' && (character < 0x21U || character > 0x7EU)))
    {
      return false;
    }
    path[length] = (char)character;
    length++;
  }
  path[length] = '\0';

  if (length == 1U)
  {
    return allow_root;
  }

  if (path[length - 1U] == '/')
  {
    return false;
  }

  size_t segment_start = 1U;
  for (size_t index = 1U; index <= length; index++)
  {
    if (index != length && path[index] != '/')
    {
      continue;
    }
    size_t segment_length = index - segment_start;
    if (segment_length == 0 || segment_length > 31U ||
        (segment_length == 1U && path[segment_start] == '.') ||
        (segment_length == 2U && path[segment_start] == '.' &&
            path[segment_start + 1U] == '.'))
    {
      return false;
    }
    segment_start = index + 1U;
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

static size_t find_file(const char *path)
{
  for (size_t index = 0; index < RAMFS_FILE_COUNT; index++)
  {
    if (files[index].used && names_equal(files[index].path, path))
    {
      return index;
    }
  }
  return RAMFS_FILE_COUNT;
}

static size_t find_directory(const char *path)
{
  for (size_t index = 0; index < RAMFS_DIRECTORY_COUNT; index++)
  {
    if (directories[index].used &&
        names_equal(directories[index].path, path))
    {
      return index;
    }
  }
  return RAMFS_DIRECTORY_COUNT;
}

static bool parent_path(
    const char *path,
    char parent[RAMFS_PATH_MAX + 1U]
)
{
  size_t length = 0;
  size_t separator = 0;
  while (path[length] != '\0')
  {
    if (path[length] == '/')
    {
      separator = length;
    }
    length++;
  }
  if (length == 0)
  {
    return false;
  }
  if (separator == 0)
  {
    parent[0] = '/';
    parent[1] = '\0';
    return true;
  }
  memcpy(parent, path, separator);
  parent[separator] = '\0';
  return true;
}

static bool parent_is_writable(const char *path)
{
  char parent[RAMFS_PATH_MAX + 1U];
  if (!parent_path(path, parent))
  {
    return false;
  }
  size_t directory = find_directory(parent);
  return directory != RAMFS_DIRECTORY_COUNT &&
      !directories[directory].read_only;
}

static bool add_directory(const char *path, bool read_only)
{
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!parse_path(path, parsed_path, false) ||
      find_directory(parsed_path) != RAMFS_DIRECTORY_COUNT ||
      find_file(parsed_path) != RAMFS_FILE_COUNT ||
      !parent_is_writable(parsed_path))
  {
    return false;
  }

  size_t slot = 0;
  while (slot < RAMFS_DIRECTORY_COUNT && directories[slot].used)
  {
    slot++;
  }
  if (slot == RAMFS_DIRECTORY_COUNT)
  {
    return false;
  }
  memcpy(directories[slot].path, parsed_path, sizeof(parsed_path));
  directories[slot].read_only = read_only;
  directories[slot].used = true;
  return true;
}

bool ramfs_init(void)
{
  static const char *const layout[] = {
    "/bin",
    "/sbin",
    "/boot",
    "/boot/limine",
    "/lib",
    "/lib/kernel",
    "/usr",
    "/usr/bin",
    "/usr/lib",
    "/home",
    "/home/ethos",
    "/home/ethos/Desktop",
    "/home/ethos/Documents",
    "/home/ethos/Downloads",
    "/home/ethos/Media",
    "/home/ethos/Media/Music",
    "/home/ethos/Media/Pictures",
    "/home/ethos/Media/Videos",
    "/home/ethos/Projects",
    "/home/ethos/Public",
    "/home/ethos/Templates",
    "/home/ethos/.cache",
    "/home/ethos/.config",
    "/home/ethos/.local",
    "/home/ethos/.local/bin",
    "/home/ethos/.local/lib",
    "/home/ethos/.local/share",
    "/home/ethos/Temporary"
  };
  static const char *const protected_paths[] = {
    "/bin",
    "/sbin",
    "/boot",
    "/boot/limine",
    "/lib",
    "/lib/kernel",
    "/usr",
    "/usr/bin",
    "/usr/lib",
    "/home"
  };

  uint64_t flags = interrupt_disable();
  if (initialized)
  {
    interrupt_restore(flags);
    return false;
  }
  memset(files, 0, sizeof(files));
  memset(file_data, 0, sizeof(file_data));
  memset(directories, 0, sizeof(directories));
  initialized = true;
  directories[0].path[0] = '/';
  directories[0].path[1] = '\0';
  directories[0].used = true;
  for (size_t index = 0; index < sizeof(layout) / sizeof(layout[0]); index++)
  {
    if (!add_directory(layout[index], false))
    {
      memset(directories, 0, sizeof(directories));
      initialized = false;
      interrupt_restore(flags);
      return false;
    }
  }
  for (size_t index = 0;
      index < sizeof(protected_paths) / sizeof(protected_paths[0]); index++)
  {
    size_t directory = find_directory(protected_paths[index]);
    if (directory == RAMFS_DIRECTORY_COUNT)
    {
      memset(directories, 0, sizeof(directories));
      initialized = false;
      interrupt_restore(flags);
      return false;
    }
    directories[directory].read_only = true;
  }
  interrupt_restore(flags);
  return true;
}

bool ramfs_is_initialized(void)
{
  return initialized;
}

bool ramfs_mkdir(const char *path)
{
  if (!initialized)
  {
    return false;
  }
  uint64_t flags = interrupt_disable();
  bool created = add_directory(path, false);
  interrupt_restore(flags);
  return created;
}

bool ramfs_is_directory(const char *path)
{
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || !parse_path(path, parsed_path, true))
  {
    return false;
  }
  uint64_t flags = interrupt_disable();
  bool exists = find_directory(parsed_path) != RAMFS_DIRECTORY_COUNT;
  interrupt_restore(flags);
  return exists;
}

size_t ramfs_directory_count(void)
{
  uint64_t flags = interrupt_disable();
  size_t count = 0;
  for (size_t index = 0; index < RAMFS_DIRECTORY_COUNT; index++)
  {
    if (directories[index].used)
    {
      count++;
    }
  }
  interrupt_restore(flags);
  return count;
}

bool ramfs_directory_get_at(size_t index, struct ramfs_directory_info *info)
{
  if (!initialized || info == 0)
  {
    return false;
  }
  uint64_t flags = interrupt_disable();
  size_t active_index = 0;
  for (size_t slot = 0; slot < RAMFS_DIRECTORY_COUNT; slot++)
  {
    if (!directories[slot].used)
    {
      continue;
    }
    if (active_index++ == index)
    {
      memcpy(info->path, directories[slot].path, sizeof(info->path));
      info->read_only = directories[slot].read_only;
      interrupt_restore(flags);
      return true;
    }
  }
  interrupt_restore(flags);
  return false;
}

bool ramfs_create(const char *path)
{
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || !parse_path(path, parsed_path, false))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  if (find_file(parsed_path) != RAMFS_FILE_COUNT ||
      find_directory(parsed_path) != RAMFS_DIRECTORY_COUNT ||
      !parent_is_writable(parsed_path))
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

  memcpy(files[slot].path, parsed_path, sizeof(parsed_path));
  files[slot].size = 0;
  files[slot].used = true;
  interrupt_restore(flags);
  return true;
}

bool ramfs_remove(const char *path)
{
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || !parse_path(path, parsed_path, false))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(parsed_path);
  if (slot == RAMFS_FILE_COUNT || !parent_is_writable(parsed_path))
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
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || !parse_path(path, parsed_path, false) ||
      (size != 0 && data == 0) || offset > RAMFS_FILE_CAPACITY ||
      size > RAMFS_FILE_CAPACITY - offset)
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(parsed_path);
  if (slot == RAMFS_FILE_COUNT || !parent_is_writable(parsed_path))
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
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || !parse_path(path, parsed_path, false) ||
      (capacity != 0 && buffer == 0))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(parsed_path);
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
  char parsed_path[RAMFS_PATH_MAX + 1U];
  if (!initialized || info == 0 || !parse_path(path, parsed_path, false))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t slot = find_file(parsed_path);
  if (slot == RAMFS_FILE_COUNT)
  {
    interrupt_restore(flags);
    return false;
  }
  memcpy(info->path, files[slot].path, sizeof(info->path));
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
    memcpy(info->path, files[slot].path, sizeof(info->path));
    info->size = files[slot].size;
    interrupt_restore(flags);
    return true;
  }
  interrupt_restore(flags);
  return false;
}