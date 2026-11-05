#include <shell/shell.h>
#include <apps/typer.h>
#include <console/vga.h>
#include <fs/ramfs.h>
#include <input/keyboard.h>
#include <proc/process.h>
#include <sched/scheduler.h>
#include <net/ethernet.h>
#include <net/ping.h>
#include <net/wifi.h>
#include <display/server.h>
#include <stddef.h>

#define SHELL_LINE_MAX 127U
#define SHELL_ARGUMENT_MAX 64U

static char command_line[SHELL_LINE_MAX + 1U];
static size_t command_length;
static bool initialized;

static bool strings_equal(const char *left, const char *right)
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

static void shell_write_hex_byte(uint8_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  vga_write_char(digits[value >> 4]);
  vga_write_char(digits[value & 0x0FU]);
}

static void display_demo(void)
{
  if (!display_server_is_initialized())
  {
    vga_write("display: framebuffer server unavailable\n");
    return;
  }

  struct display_rect bounds = { .x = 12U, .y = 4U, .width = 56U,
    .height = 16U };
  uint32_t window_id;
  if (!display_window_create("Ethos Display Server", bounds, &window_id))
  {
    vga_write("display: couldn't create window\n");
    return;
  }
  (void)display_window_fill(window_id,
      (struct display_rect){ .x = 0U, .y = 0U, .width = 54U, .height = 14U },
      ' ', 7U, 0U);
  (void)display_window_draw_text(window_id, 2U, 1U,
      "Composited text-cell window", 15U, 0U);
  (void)display_window_draw_text(window_id, 2U, 3U,
      "Window buffers, z-order, focus and keyboard routing are active.",
      7U, 0U);
  (void)display_window_draw_text(window_id, 2U, 6U,
      "Press Q to close this window and return to the shell.", 14U, 0U);

  for (;;)
  {
    struct keyboard_event event;
    if (!display_server_poll_event(window_id, &event))
    {
      __asm__ volatile ("pause");
      continue;
    }
    if (event.key == KEYBOARD_KEY_CHARACTER &&
        (event.character == 'q' || event.character == 'Q'))
    {
      break;
    }
    if (event.key == KEYBOARD_KEY_CHARACTER && event.character >= 0x20 &&
        event.character <= 0x7E)
    {
      (void)display_window_fill(window_id,
          (struct display_rect){ .x = 2U, .y = 9U, .width = 50U,
            .height = 1U }, ' ', 7U, 0U);
      (void)display_window_draw_text(window_id, 2U, 9U,
          "Key event delivered to focused window.", 10U, 0U);
    }
  }
  (void)display_window_destroy(window_id);
  vga_clear();
}

static bool resolve_path(const char *input, char output[RAMFS_PATH_MAX + 1U])
{
  if (input == 0 || input[0] == '\0')
  {
    return false;
  }
  size_t output_length = 1U;
  size_t input_position = 0;
  output[0] = '/';

  while (input[input_position] != '\0')
  {
    while (input[input_position] == '/')
    {
      input_position++;
    }
    if (input[input_position] == '\0')
    {
      break;
    }

    size_t component_start = input_position;
    while (input[input_position] != '\0' && input[input_position] != '/')
    {
      input_position++;
    }
    size_t component_length = input_position - component_start;
    if (component_length == 1U && input[component_start] == '.')
    {
      continue;
    }
    if (component_length == 2U && input[component_start] == '.' &&
        input[component_start + 1U] == '.')
    {
      return false;
    }

    size_t separator = output_length > 1U ? 1U : 0U;
    if (output_length + separator > RAMFS_PATH_MAX ||
      component_length > RAMFS_PATH_MAX - output_length - separator)
    {
      return false;
    }
    if (separator != 0U)
    {
      output[output_length++] = '/';
    }
    for (size_t index = 0; index < component_length; index++)
    {
      output[output_length++] = input[component_start + index];
    }
  }
  output[output_length] = '\0';
  return true;
}

static bool path_parent_matches(const char *path, const char *directory,
    const char **name)
{
  size_t directory_length = 0;
  while (directory[directory_length] != '\0')
  {
    directory_length++;
  }
  size_t prefix_length = directory_length;
  if (directory_length == 1U && directory[0] == '/')
  {
    prefix_length = 0;
  }
  for (size_t index = 0; index < prefix_length; index++)
  {
    if (path[index] != directory[index])
    {
      return false;
    }
  }
  size_t name_start = prefix_length;
  if (prefix_length != 0)
  {
    if (path[name_start] != '/')
    {
      return false;
    }
    name_start++;
  }
  else if (path[0] == '/')
  {
    name_start = 1U;
  }
  if (path[name_start] == '\0')
  {
    return false;
  }
  for (size_t index = name_start; path[index] != '\0'; index++)
  {
    if (path[index] == '/')
    {
      return false;
    }
  }
  *name = &path[name_start];
  return true;
}

static void print_basename(const char *path)
{
  size_t length = 0;
  size_t start = 0;
  while (path[length] != '\0')
  {
    if (path[length] == '/')
    {
      start = length + 1U;
    }
    length++;
  }
  vga_write(&path[start]);
}

static bool shell_list_directory(const char *path)
{
  if (!ramfs_is_directory(path))
  {
    struct ramfs_file_info file;
    if (ramfs_stat(path, &file))
    {
      print_basename(path);
      vga_write_char('\n');
      return true;
    }
    vga_write("ls: no such file or directory\n");
    return false;
  }

  bool found = false;
  size_t count = ramfs_directory_count();
  for (size_t index = 0; index < count; index++)
  {
    struct ramfs_directory_info directory;
    const char *name;
    if (ramfs_directory_get_at(index, &directory) &&
        path_parent_matches(directory.path, path, &name))
    {
      vga_write(name);
      vga_write("/  ");
      found = true;
    }
  }
  count = ramfs_count();
  for (size_t index = 0; index < count; index++)
  {
    struct ramfs_file_info file;
    const char *name;
    if (ramfs_get_at(index, &file) &&
        path_parent_matches(file.path, path, &name))
    {
      vga_write(name);
      vga_write("  ");
      found = true;
    }
  }
  if (found)
  {
    vga_write_char('\n');
  }
  return true;
}

static bool shell_cat_file(const char *path)
{
  uint8_t contents[RAMFS_FILE_CAPACITY];
  size_t bytes_read;
  if (!ramfs_read(path, 0, contents, sizeof(contents), &bytes_read))
  {
    vga_write("cat: cannot read file\n");
    return false;
  }
  for (size_t index = 0; index < bytes_read; index++)
  {
    vga_write_char((char)contents[index]);
  }
  if (bytes_read != 0 && contents[bytes_read - 1U] != '\n')
  {
    vga_write_char('\n');
  }
  return true;
}

static bool execute_builtin(size_t argument_count, char *arguments[])
{
  const char *command = arguments[0];
  if (strings_equal(command, "net"))
  {
    if (argument_count > 2U ||
        (argument_count == 2U && !strings_equal(arguments[1], "status")))
    {
      vga_write("usage: net [status]\n");
      return true;
    }
    struct ethernet_device_info info;
    if (ethernet_get_info(&info))
    {
      vga_write("eth0: ");
      vga_write(info.link_up ? "link up  MAC " : "link down  MAC ");
      for (size_t index = 0; index < sizeof(info.mac_address); index++)
      {
        if (index != 0)
        {
          vga_write_char(':');
        }
        shell_write_hex_byte(info.mac_address[index]);
      }
      vga_write_char('\n');
    }
    else
    {
      vga_write("eth0: unavailable\n");
    }

    struct wifi_device_info wifi;
    if (!iwl_ax201_probe(&wifi) || !wifi.detected)
    {
      vga_write(wifi.firmware_available
          ? "wifi: AX201 not detected, firmware module present\n"
          : "wifi: AX201 not detected, firmware module missing\n");
    }
    else if (!wifi.firmware_available)
    {
      vga_write("wifi: AX201 detected, firmware missing, radio not initialized\n");
    }
    else
    {
      vga_write("wifi: AX201 detected, firmware present, radio not initialized\n");
    }
    return true;
  }
  if (strings_equal(command, "ping"))
  {
    if (argument_count != 2U)
    {
      vga_write("usage: ping <hostname|IPv4>\n");
    }
    else
    {
      (void)network_ping(arguments[1]);
    }
    return true;
  }
  if (strings_equal(command, "display"))
  {
    if (argument_count != 1U)
    {
      vga_write("usage: display\n");
    }
    else
    {
      display_demo();
    }
    return true;
  }
  if (strings_equal(command, "typer"))
  {
    if (argument_count != 2U)
    {
      vga_write("usage: typer [path]\n");
    }
    else
    {
      char path[RAMFS_PATH_MAX + 1U];
      if (!resolve_path(arguments[1], path))
      {
        vga_write("typer: invalid path\n");
      }
      else
      {
        typer_run(path);
      }
    }
    return true;
  }
  if (strings_equal(command, "echo"))
  {
    for (size_t index = 1; index < argument_count; index++)
    {
      if (index != 1U)
      {
        vga_write_char(' ');
      }
      vga_write(arguments[index]);
    }
    vga_write_char('\n');
    return true;
  }
  if (strings_equal(command, "help"))
  {
    vga_write("Built-ins: help echo ls cat pwd clear mkdir touch rm typer net ping display\n");
    vga_write("External APX command: hello\n");
    vga_write("Typer: typer [path]\n");
    vga_write("Ping: ping <hostname|IPv4>\n");
    vga_write("Display: display\n");
    return true;
  }
  if (strings_equal(command, "pwd"))
  {
    vga_write("/\n");
    return true;
  }
  if (strings_equal(command, "clear"))
  {
    vga_clear();
    return true;
  }
  if (strings_equal(command, "ls"))
  {
    if (argument_count > 2U)
    {
      vga_write("usage: ls [path]\n");
      return true;
    }
    char path[RAMFS_PATH_MAX + 1U];
    if (!resolve_path(argument_count == 2U ? arguments[1] : "/", path))
    {
      vga_write("ls: invalid path\n");
    }
    else
    {
      shell_list_directory(path);
    }
    return true;
  }
  if (strings_equal(command, "cat"))
  {
    if (argument_count < 2U)
    {
      vga_write("usage: cat path...\n");
      return true;
    }
    for (size_t index = 1; index < argument_count; index++)
    {
      char path[RAMFS_PATH_MAX + 1U];
      if (!resolve_path(arguments[index], path))
      {
        vga_write("cat: invalid path\n");
      }
      else
      {
        shell_cat_file(path);
      }
    }
    return true;
  }
  if (strings_equal(command, "mkdir") || strings_equal(command, "touch") ||
      strings_equal(command, "rm"))
  {
    if (argument_count != 2U)
    {
      vga_write("usage: mkdir|touch|rm path\n");
      return true;
    }
    char path[RAMFS_PATH_MAX + 1U];
    if (!resolve_path(arguments[1], path))
    {
      vga_write("error: invalid path\n");
    }
    else if (strings_equal(command, "mkdir"))
    {
      if (!ramfs_mkdir(path))
      {
        vga_write("mkdir: cannot create directory\n");
      }
    }
    else if (strings_equal(command, "touch"))
    {
      if (!ramfs_create(path))
      {
        vga_write("touch: cannot create file\n");
      }
    }
    else if (!ramfs_remove(path))
    {
      vga_write("rm: cannot remove file\n");
    }
    return true;
  }
  return false;
}

static bool parse_command_line(char *line, size_t *argument_count,
    char *arguments[SHELL_ARGUMENT_MAX])
{
  size_t read_position = 0;
  size_t write_position = 0;
  *argument_count = 0;

  while (line[read_position] != '\0')
  {
    while (line[read_position] == ' ' || line[read_position] == '\t')
    {
      read_position++;
    }
    if (line[read_position] == '\0')
    {
      break;
    }
    if (*argument_count == SHELL_ARGUMENT_MAX)
    {
      return false;
    }

    arguments[(*argument_count)++] = &line[write_position];
    char quote = '\0';
    while (line[read_position] != '\0')
    {
      char character = line[read_position++];
      if (quote != '\0')
      {
        if (character == quote)
        {
          quote = '\0';
          continue;
        }
        if (character == '\\' && quote == '"' &&
            line[read_position] != '\0')
        {
          character = line[read_position++];
        }
        line[write_position++] = character;
        continue;
      }
      if (character == '\'' || character == '"')
      {
        quote = character;
      }
      else if (character == '\\')
      {
        if (line[read_position] == '\0')
        {
          return false;
        }
        line[write_position++] = line[read_position++];
      }
      else if (character == ' ' || character == '\t')
      {
        break;
      }
      else
      {
        line[write_position++] = character;
      }
    }
    if (quote != '\0')
    {
      return false;
    }
    line[write_position++] = '\0';
  }
  return true;
}

bool shell_init(void)
{
  if (initialized || !keyboard_init())
  {
    return false;
  }
  command_length = 0;
  initialized = true;
  return true;
}

static void execute_command_line(void)
{
  command_line[command_length] = '\0';
  size_t argument_count;
  char *arguments[SHELL_ARGUMENT_MAX];
  if (!parse_command_line(command_line, &argument_count, arguments))
  {
    vga_write("syntax error: invalid quoting or too many arguments\n");
  }
  else if (argument_count != 0)
  {
    if (!execute_builtin(argument_count, arguments) &&
        !process_exec_command(argument_count, arguments))
    {
      vga_write("command not found or invalid binary\n");
    }
  }
  command_length = 0;
}

__attribute__((noreturn)) void shell_run(void)
{
  if (!initialized)
  {
    for (;;)
    {
      __asm__ volatile ("cli; hlt" ::: "memory");
    }
  }

  vga_write("Run a command; quote arguments with spaces.\n");
  vga_write("ethos> ");
  uint64_t cursor_last_tick = scheduler_ticks();
  bool cursor_visible = true;
  vga_set_cursor_visible(cursor_visible);
  for (;;)
  {
    uint64_t current_tick = scheduler_ticks();
    if (current_tick - cursor_last_tick >= 50U)
    {
      cursor_last_tick = current_tick;
      cursor_visible = !cursor_visible;
      vga_set_cursor_visible(cursor_visible);
    }

    char character = keyboard_poll_char();
    if (character == 0)
    {
      __asm__ volatile ("pause");
      continue;
    }

    cursor_last_tick = current_tick;
    cursor_visible = true;
    vga_set_cursor_visible(cursor_visible);
    if (character == '\n')
    {
      vga_write_char('\n');
      execute_command_line();
      vga_write("ethos> ");
      continue;
    }
    if (character == '\b')
    {
      if (command_length != 0)
      {
        command_length--;
        vga_write_char('\b');
      }
      continue;
    }
    if (character == '\t')
    {
      character = ' ';
    }
    if (character >= 0x20 && character <= 0x7E &&
        command_length < SHELL_LINE_MAX)
    {
      command_line[command_length++] = character;
      vga_write_char(character);
    }
  }
}
