#include <shell/shell.h>
#include <console/vga.h>
#include <input/keyboard.h>
#include <proc/process.h>
#include <stddef.h>

#define SHELL_LINE_MAX 127U

static char command_line[SHELL_LINE_MAX + 1U];
static size_t command_length;
static bool initialized;

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
  size_t first = 0;
  while (command_line[first] == ' ' || command_line[first] == '\t')
  {
    first++;
  }
  size_t end = first;
  while (command_line[end] != '\0' && command_line[end] != ' ' &&
      command_line[end] != '\t')
  {
    end++;
  }

  if (end > first)
  {
    char saved = command_line[end];
    command_line[end] = '\0';
    if (!process_exec_command(&command_line[first]))
    {
      vga_write("command not found or invalid binary\n");
    }
    command_line[end] = saved;
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

  vga_write("Type a command name to run its binary.\n");
  vga_write("ethos> ");
  for (;;)
  {
    char character = keyboard_poll_char();
    if (character == 0)
    {
      __asm__ volatile ("pause");
      continue;
    }

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
