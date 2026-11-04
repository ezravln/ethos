#include <apps/typer.h>
#include <console/vga.h>
#include <fs/ramfs.h>
#include <input/keyboard.h>
#include <lib/memory.h>

static char text[RAMFS_FILE_CAPACITY];
static char clipboard[RAMFS_FILE_CAPACITY];
static size_t clipboard_length;

struct editor_cell {
  char character;
  uint8_t foreground;
  uint8_t background;
};

static struct editor_cell desired_screen[VGA_TEXT_ROWS][VGA_TEXT_COLUMNS];
static struct editor_cell previous_screen[VGA_TEXT_ROWS][VGA_TEXT_COLUMNS];
static bool screen_cache_valid;
static bool previous_cursor_visible;
static size_t previous_cursor_column;
static size_t previous_cursor_row;

#define EDITOR_COLUMNS 80U
#define EDITOR_TOP_ROW 3U
#define EDITOR_VISIBLE_ROWS 20U
#define EDITOR_STATUS_ROW 24U

struct editor_state {
  const char *path;
  size_t length;
  size_t cursor;   
  size_t anchor;
  size_t preferred_column;
  size_t first_row;
  bool selection_active;
  bool dirty;
  const char *message;
};

static size_t selection_start(const struct editor_state *editor)
{
  return editor->cursor < editor->anchor ? editor->cursor : editor->anchor;
}

static size_t selection_end(const struct editor_state *editor)
{
  return editor->cursor > editor->anchor ? editor->cursor : editor->anchor;
}

static bool has_selection(const struct editor_state *editor)
{
  return editor->selection_active && editor->cursor != editor->anchor;
}

static void clear_selection(struct editor_state *editor)
{
  editor->selection_active = false;
  editor->anchor = editor->cursor;
}

static void move_cursor(struct editor_state *editor, size_t position,
    bool extend_selection)
{
  if (position > editor->length)
  {
    position = editor->length;
  }
  if (extend_selection)
  {
    if (!editor->selection_active)
    {
      editor->anchor = editor->cursor;
      editor->selection_active = true;
    }
  }
  else
  {
    clear_selection(editor);
  }
  editor->cursor = position;
  editor->message = 0;
}

static void remove_range(struct editor_state *editor, size_t start, size_t end)
{
  if (start >= end || end > editor->length)
  {
    return;
  }
  memmove(&text[start], &text[end], editor->length - end);
  editor->length -= end - start;
  editor->cursor = start;
  clear_selection(editor);
  editor->dirty = true;
}

static void delete_selection(struct editor_state *editor)
{
  if (has_selection(editor))
  {
    remove_range(editor, selection_start(editor), selection_end(editor));
  }
}

static size_t line_start(size_t position)
{
  while (position != 0 && text[position - 1U] != '\n')
  {
    position--;
  }
  return position;
}

static size_t line_end(size_t position, size_t length)
{
  while (position < length && text[position] != '\n')
  {
    position++;
  }
  return position;
}

static size_t visual_row_for(size_t position)
{
  size_t row = 0;
  size_t column = 0;
  for (size_t index = 0; index < position; index++)
  {
    if (text[index] == '\n')
    {
      row++;
      column = 0;
    }
    else if (++column == EDITOR_COLUMNS)
    {
      row++;
      column = 0;
    }
  }
  return row;
}

static size_t visual_column_for(size_t position)
{
  size_t column = 0;
  size_t start = line_start(position);
  for (size_t index = start; index < position; index++)
  {
    if (text[index] == '\n')
    {
      column = 0;
    }
    else
    {
      column++;
      if (column == EDITOR_COLUMNS)
      {
        column = 0;
      }
    }
  }
  return column;
}

static size_t position_for_visual(size_t target_row, size_t target_column,
    size_t length)
{
  size_t row = 0;
  size_t column = 0;
  size_t best_position = 0;
  for (size_t index = 0; index <= length; index++)
  {
    if (row == target_row)
    {
      best_position = index;
      if (column >= target_column)
      {
        return index;
      }
    }
    if (index == length)
    {
      break;
    }
    if (text[index] == '\n')
    {
      if (row == target_row)
      {
        return best_position;
      }
      row++;
      column = 0;
    }
    else if (++column == EDITOR_COLUMNS)
    {
      row++;
      column = 0;
    }
  }
  return best_position;
}

static void update_view(struct editor_state *editor)
{
  size_t row = visual_row_for(editor->cursor);
  if (row < editor->first_row)
  {
    editor->first_row = row;
  }
  else if (row >= editor->first_row + EDITOR_VISIBLE_ROWS)
  {
    editor->first_row = row - EDITOR_VISIBLE_ROWS + 1U;
  }
}

static void screen_set_cell(size_t column, size_t row, char character,
    uint8_t foreground, uint8_t background)
{
  if (column >= VGA_TEXT_COLUMNS || row >= VGA_TEXT_ROWS)
  {
    return;
  }
  desired_screen[row][column] = (struct editor_cell){
    .character = character,
    .foreground = foreground,
    .background = background
  };
}

static void screen_write(size_t column, size_t row, const char *value,
    uint8_t foreground, uint8_t background)
{
  while (*value != '\0' && column < VGA_TEXT_COLUMNS)
  {
    screen_set_cell(column++, row, *value++, foreground, background);
  }
}

static void render_editor(struct editor_state *editor)
{
  update_view(editor);
  vga_set_cursor_visible(false);
  if (!screen_cache_valid)
  {
    vga_set_color(7U, 0U);
    vga_clear();
    for (size_t row = 0; row < VGA_TEXT_ROWS; row++)
    {
      for (size_t column = 0; column < VGA_TEXT_COLUMNS; column++)
      {
        previous_screen[row][column] = (struct editor_cell){ ' ', 7U, 0U };
      }
    }
    screen_cache_valid = true;
  }

  for (size_t row = 0; row < VGA_TEXT_ROWS; row++)
  {
    for (size_t column = 0; column < VGA_TEXT_COLUMNS; column++)
    {
      desired_screen[row][column] = (struct editor_cell){ ' ', 7U, 0U };
    }
  }
  screen_write(0, 0,
      "Typer | arrows move | Shift+arrows select | Ctrl+S save | Ctrl+Q exit",
      7U, 0U);
  screen_write(0, 1, editor->path, 7U, 0U);
  screen_write(0, 2,
      "--------------------------------------------------------------------------------", 7U, 0U);

  size_t row = 0;
  size_t column = 0;
  size_t selection_begin = selection_start(editor);
  size_t selection_finish = selection_end(editor);
  for (size_t index = 0; index < editor->length; index++)
  {
    if (text[index] == '\n')
    {
      row++;
      column = 0;
      continue;
    }
    if (row >= editor->first_row &&
        row < editor->first_row + EDITOR_VISIBLE_ROWS)
    {
      bool selected = has_selection(editor) && index >= selection_begin &&
          index < selection_finish;
      screen_set_cell(column, EDITOR_TOP_ROW + row - editor->first_row,
        text[index], selected ? 0U : 7U, selected ? 14U : 0U);
    }
    if (++column == EDITOR_COLUMNS)
    {
      row++;
      column = 0;
    }
  }

  const char *status;
  if (editor->message != 0)
  {
    status = editor->message;
  }
  else
  {
    status = editor->dirty ? "MODIFIED  Ctrl+C copy  Ctrl+X cut  Ctrl+V paste"
      : "SAVED     Ctrl+C copy  Ctrl+X cut  Ctrl+V paste";
  }
  screen_write(0, EDITOR_STATUS_ROW, status, 7U, 0U);

  for (size_t screen_row = 0; screen_row < VGA_TEXT_ROWS; screen_row++)
  {
    for (size_t screen_column = 0; screen_column < VGA_TEXT_COLUMNS;
        screen_column++)
    {
      struct editor_cell cell = desired_screen[screen_row][screen_column];
      bool restore_cursor = previous_cursor_visible &&
          screen_column == previous_cursor_column &&
          screen_row == previous_cursor_row;
      struct editor_cell previous = previous_screen[screen_row][screen_column];
      if (!restore_cursor && cell.character == previous.character &&
          cell.foreground == previous.foreground &&
          cell.background == previous.background)
      {
        continue;
      }
      vga_set_cursor_position(screen_column, screen_row);
      vga_set_color(cell.foreground, cell.background);
      vga_write_char(cell.character);
      previous_screen[screen_row][screen_column] = cell;
    }
  }

  size_t cursor_row = visual_row_for(editor->cursor);
  size_t cursor_column = visual_column_for(editor->cursor);
  previous_cursor_visible = false;
  vga_set_color(7U, 0U);
  if (cursor_row >= editor->first_row &&
      cursor_row < editor->first_row + EDITOR_VISIBLE_ROWS)
  {
    previous_cursor_column = cursor_column;
    previous_cursor_row = EDITOR_TOP_ROW + cursor_row - editor->first_row;
    vga_set_cursor_position(previous_cursor_column, previous_cursor_row);
    vga_set_cursor_visible(true);
    previous_cursor_visible = true;
  }
}

static bool save_file(struct editor_state *editor)
{
  struct ramfs_file_info info;
  if (!ramfs_stat(editor->path, &info) && !ramfs_create(editor->path))
  {
    return false;
  }
  if (!ramfs_truncate(editor->path, editor->length) ||
      !ramfs_write(editor->path, 0, text, editor->length))
  {
    return false;
  }
  editor->dirty = false;
  editor->message = "Saved";
  return true;
}

static void copy_selection(struct editor_state *editor, bool remove)
{
  size_t start = selection_start(editor);
  size_t end = selection_end(editor);
  if (!has_selection(editor))
  {
    editor->message = "Nothing selected";
    return;
  }
  clipboard_length = end - start;
  memcpy(clipboard, &text[start], clipboard_length);
  if (remove)
  {
    remove_range(editor, start, end);
  }
  else
  {
    clear_selection(editor);
  }
  editor->message = remove ? "Cut to clipboard" : "Copied to clipboard";
}

static void paste_clipboard(struct editor_state *editor)
{
  size_t start = editor->cursor;
  size_t removed = 0;
  if (has_selection(editor))
  {
    start = selection_start(editor);
    removed = selection_end(editor) - start;
  }
  if (clipboard_length > sizeof(text) - (editor->length - removed))
  {
    editor->message = "Paste exceeds 4 KiB file limit";
    return;
  }
  if (removed != 0)
  {
    memmove(&text[start], &text[start + removed], editor->length - start - removed);
    editor->length -= removed;
  }
  memmove(&text[start + clipboard_length], &text[start], editor->length - start);
  memcpy(&text[start], clipboard, clipboard_length);
  editor->length += clipboard_length;
  editor->cursor = start + clipboard_length;
  clear_selection(editor);
  editor->dirty = true;
  editor->message = "Pasted";
}

static void insert_character(struct editor_state *editor, char character)
{
  delete_selection(editor);
  if (editor->length == sizeof(text))
  {
    editor->message = "File is at 4 KiB limit";
    return;
  }
  memmove(&text[editor->cursor + 1U], &text[editor->cursor],
      editor->length - editor->cursor);
  text[editor->cursor++] = character;
  editor->length++;
  editor->dirty = true;
  editor->message = 0;
  editor->preferred_column = visual_column_for(editor->cursor);
}

static size_t move_word_left(size_t position)
{
  while (position != 0 && text[position - 1U] == ' ')
  {
    position--;
  }
  while (position != 0 && text[position - 1U] != ' ' &&
      text[position - 1U] != '\n')
  {
    position--;
  }
  return position;
}

static size_t move_word_right(size_t position, size_t length)
{
  while (position < length && text[position] != ' ' && text[position] != '\n')
  {
    position++;
  }
  while (position < length && text[position] == ' ')
  {
    position++;
  }
  return position;
}

static void move_vertical(struct editor_state *editor, int direction,
    bool extend_selection)
{
  if (editor->preferred_column == SIZE_MAX)
  {
    editor->preferred_column = visual_column_for(editor->cursor);
  }
  size_t row = visual_row_for(editor->cursor);
  if (direction < 0 && row != 0)
  {
    move_cursor(editor, position_for_visual(row - 1U,
        editor->preferred_column, editor->length), extend_selection);
  }
  else if (direction > 0)
  {
    size_t next_row = row + 1U;
    if (position_for_visual(next_row, 0, editor->length) < editor->length ||
        visual_row_for(editor->length) >= next_row)
    {
      move_cursor(editor, position_for_visual(next_row,
          editor->preferred_column, editor->length), extend_selection);
    }
  }
}

static bool handle_event(struct editor_state *editor,
    const struct keyboard_event *event)
{
  bool control = (event->modifiers & KEYBOARD_MOD_CONTROL) != 0;
  bool shift = (event->modifiers & KEYBOARD_MOD_SHIFT) != 0;
  if (event->key == KEYBOARD_KEY_CHARACTER && control)
  {
    switch (event->character)
    {
      case 19:
        if (!save_file(editor))
        {
          editor->message = "Save failed: path is not writable";
        }
        return false;
      case 17:
        if (editor->dirty)
        {
          editor->message = "Unsaved changes: press Ctrl+S first";
          return false;
        }
        return true;
      case 1:
        editor->anchor = 0;
        editor->cursor = editor->length;
        editor->selection_active = true;
        editor->message = "Selected all";
        return false;
      case 3:
        copy_selection(editor, false);
        return false;
      case 24:
        copy_selection(editor, true);
        return false;
      case 22:
        paste_clipboard(editor);
        return false;
      case 2:
        move_cursor(editor, 0, false);
        editor->preferred_column = 0;
        return false;
      case 5:
        move_cursor(editor, editor->length, false);
        editor->preferred_column = SIZE_MAX;
        return false;
      default:
        return false;
    }
  }

  if (event->key == KEYBOARD_KEY_LEFT || event->key == KEYBOARD_KEY_RIGHT ||
      event->key == KEYBOARD_KEY_UP || event->key == KEYBOARD_KEY_DOWN ||
      event->key == KEYBOARD_KEY_HOME || event->key == KEYBOARD_KEY_END)
  {
    if (!shift && has_selection(editor) &&
        (event->key == KEYBOARD_KEY_LEFT || event->key == KEYBOARD_KEY_RIGHT))
    {
      size_t target = event->key == KEYBOARD_KEY_LEFT
          ? selection_start(editor) : selection_end(editor);
      move_cursor(editor, target, false);
    }
    else if (event->key == KEYBOARD_KEY_LEFT)
    {
      size_t target = control ? move_word_left(editor->cursor)
          : (editor->cursor == 0 ? 0 : editor->cursor - 1U);
      move_cursor(editor, target, shift);
      editor->preferred_column = SIZE_MAX;
    }
    else if (event->key == KEYBOARD_KEY_RIGHT)
    {
      size_t target = control ? move_word_right(editor->cursor, editor->length)
          : (editor->cursor == editor->length ? editor->length : editor->cursor + 1U);
      move_cursor(editor, target, shift);
      editor->preferred_column = SIZE_MAX;
    }
    else if (event->key == KEYBOARD_KEY_UP || event->key == KEYBOARD_KEY_DOWN)
    {
      move_vertical(editor, event->key == KEYBOARD_KEY_UP ? -1 : 1, shift);
    }
    else
    {
      size_t target = event->key == KEYBOARD_KEY_HOME
          ? line_start(editor->cursor) : line_end(editor->cursor, editor->length);
      if (control)
      {
        target = event->key == KEYBOARD_KEY_HOME ? 0 : editor->length;
      }
      move_cursor(editor, target, shift);
      editor->preferred_column = SIZE_MAX;
    }
    return false;
  }

  if (event->key == KEYBOARD_KEY_DELETE ||
      (event->key == KEYBOARD_KEY_CHARACTER && event->character == '\b'))
  {
    if (has_selection(editor))
    {
      delete_selection(editor);
    }
    else if (event->key == KEYBOARD_KEY_DELETE && editor->cursor < editor->length)
    {
      remove_range(editor, editor->cursor, editor->cursor + 1U);
    }
    else if (event->key == KEYBOARD_KEY_CHARACTER && editor->cursor != 0)
    {
      remove_range(editor, editor->cursor - 1U, editor->cursor);
    }
    editor->message = 0;
    return false;
  }
  if (event->key == KEYBOARD_KEY_CHARACTER && !control &&
      (event->character == '\n' || event->character == '\t' ||
       (event->character >= 0x20 && event->character <= 0x7E)))
  {
    if (event->character == '\t')
    {
      for (size_t index = 0; index < 4U; index++)
      {
        insert_character(editor, ' ');
      }
    }
    else
    {
      insert_character(editor, event->character);
    }
  }
  return false;
}

void typer_run(const char *path)
{
  screen_cache_valid = false;
  previous_cursor_visible = false;
  struct ramfs_file_info info;
  struct editor_state editor = {
    .path = path,
    .preferred_column = SIZE_MAX
  };
  if (path == 0 || path[0] != '/' || path[1] == '\0')
  {
    vga_write("Usage: typer /absolute/path/to/file\n");
    return;
  }
  if (ramfs_stat(path, &info))
  {
    if (info.size > sizeof(text))
    {
      vga_write("typer: file exceeds RAMFS capacity\n");
      return;
    }
    if (!ramfs_read(path, 0, text, sizeof(text), &editor.length))
    {
      vga_write("typer: could not read file\n");
      return;
    }
  }

  render_editor(&editor);
  for (;;)
  {
    struct keyboard_event event;
    if (!keyboard_poll_event(&event))
    {
      __asm__ volatile ("pause");
      continue;
    }
    if (handle_event(&editor, &event))
    {
      vga_set_cursor_visible(false);
      previous_cursor_visible = false;
      screen_cache_valid = false;
      vga_set_color(7U, 0U);
      vga_clear();
      return;
    }
    render_editor(&editor);
  }
}