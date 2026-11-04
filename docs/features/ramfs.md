# RAMFS

RAMFS keeps files in kernel memory. It starts with a directory tree, not with the files staged inside the ISO; boot modules supply executable commands separately. Reboot and the files disappear.

## Capacity

The limits live in `fs/ramfs.c` and `include/fs/ramfs.h`:

| Resource | Limit |
| --- | ---: |
| File slots | 16 |
| Bytes in one file | 4096 |
| Directory slots, including `/` | 64 |
| Path length | 127 characters |
| One path segment | 31 characters |

RAMFS doesn't use a disk, track per-file permissions, or support links and mount points.

## Directory tree

Ethos creates `/bin`, `/sbin`, `/boot`, `/lib`, `/usr`, and their system subdirectories at startup. It also creates `/home/ethos` with `Desktop`, `Documents`, `Downloads`, `Media`, `Projects`, `Public`, `Templates`, `Temporary`, and `.local` directories. Media and `.local` each have their own child directories.

Write checks protect system paths such as `/boot`, `/bin`, `/sbin`, `/lib`, `/usr`, and `/home`. The user's home subdirectories remain writable; for example, `/home/ethos/Documents` can hold editor files. `/usr/bin` exists in RAMFS but begins empty, because Limine loads command modules from the ISO independently.

## Try it in the shell

```text
mkdir /home/ethos/Documents/Notes
touch /home/ethos/Documents/Notes/hello.txt
typer /home/ethos/Documents/Notes/hello.txt
cat /home/ethos/Documents/Notes/hello.txt
```

The shell resolves `hello.txt` to `/hello.txt`; it doesn't track a working directory, and it rejects `..`. Typer can write only when the parent directory already exists and allows writes. It truncates before saving, so a shorter replacement doesn't retain old bytes at the end.