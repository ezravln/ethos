#!/bin/bash

qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cdrom build/ethos.iso \
  -nic user,model=e1000 \
  -boot d \
  -serial stdio \
  -no-reboot \
  -no-shutdown
