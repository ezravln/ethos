# Wi-Fi: Intel AX201

Ethos recognizes the Intel AX201 PCI ID `8086:a0f0`. The host that identified this target reports firmware `iwlwifi-QuZ-a0-jf-b0-77.ucode`, so Ethos looks for a Limine module with that exact name.

This is device detection and firmware staging, not a working Wi-Fi driver. Ethos does not yet start the AX201 radio, scan for access points, authenticate, associate, or exchange 802.11 frames.

## Optional firmware module

The build script never copies firmware from the host by default. To stage a firmware file you have obtained and are allowed to use, set its path while building:

```sh
ETHOS_IWLWIFI_FIRMWARE=/lib/firmware/iwlwifi-QuZ-a0-jf-b0-77.ucode.zst ./scripts/build.sh
```

The script copies it into the generated ISO as `/boot/firmware/iwlwifi-QuZ-a0-jf-b0-77.ucode` and registers that file as a Limine module. `iwl_ax201_get_firmware()` returns the module's address and size to kernel code, and the probe reports whether it found both the PCI device and named module. Staging firmware does not upload it to the device yet.

## Why the Linux module won't load here

`iwlwifi` is a Linux kernel driver, not a portable firmware blob. Its PCI transport, DMA, interrupt handling, firmware protocol, and connection-management code depend on Linux kernel facilities. Ethos needs its own driver path; copying `iwlwifi.ko` into the ISO cannot provide one.

The AX201 also needs more than a PCI probe. A usable driver must initialize its transport, load and start firmware, handle interrupts and command queues, then implement the 802.11 connection flow. Ethos currently has none of those Wi-Fi pieces. Its E1000 Ethernet support is separate; see [Ethernet](ethernet.md).
