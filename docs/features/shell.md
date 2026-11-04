# Shell

Ethos starts the shell in kernel context. It reads events from the keyboard driver and writes through the framebuffer console; it doesn't launch a separate shell process.

## Commands

| Command | What it does |
| --- | --- |
| `help` | Prints command names and usage. |
| `echo [text...]` | Writes the arguments. |
| `ls [path]` | Lists one directory, or prints a file's name. |
| `cat path...` | Writes file contents. |
| `pwd` | Prints `/`. |
| `clear` | Clears the display. |
| `mkdir path` | Creates one directory. |
| `touch path` | Creates an empty file. |
| `rm path` | Removes a file. |
| `typer path` | Opens a file in Typer. |

Use an absolute path or a path relative to `/`. `typer hello.txt` therefore opens `/hello.txt`; there is no `cd`, and `..` components fail validation.

## Parsing

The input buffer holds 127 characters, and the parser can split a line into up to 64 arguments. It recognizes single and double quotes and backslash escapes. After a built-in check, the shell searches Limine's module list for a command name, loads the matching APX, and passes the arguments to the user program.

Pipes and redirection aren't part of this shell. It also has no variables, scripts, globbing, command chaining, environment, or job control. See [APX executables](apx-executables.md) to follow external command loading.