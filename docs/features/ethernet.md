# Ethernet

Ethos can initialize one Intel E1000-compatible PCI adapter and send or poll-receive Ethernet frames. The driver lives in `net/e1000.c`; its kernel API is `include/net/ethernet.h`. It currently supports the Intel 82540EM device ID `8086:100E`, the model QEMU exposes as `e1000`.

## QEMU setup

The repository's `scripts/run.sh` attaches an E1000 NIC to QEMU's user-mode network backend:

```sh
./scripts/build.sh
./scripts/run.sh
```

A successful boot log includes a line like:

```text
SUCCESS >> E1000 eth0 MAC=52:54:00:12:34:56 link=up
```

The MAC depends on QEMU's generated NIC address. In the shell, `net` or `net status` prints the adapter state and MAC. If Ethos finds no supported adapter, the kernel continues booting and the command reports that no adapter is available.

## Driver behavior

The driver scans PCI configuration space, maps the E1000 MMIO BAR, enables PCI memory access and bus mastering, then allocates receive and transmit descriptor rings from page-backed DMA memory. It polls descriptors rather than using a NIC interrupt. The rings contain 16 descriptors each, and receive buffers hold up to 2048 bytes.

Kernel code can query the MAC and link state with `ethernet_get_info()`, send a complete Ethernet frame with `ethernet_send_frame()`, or poll for a received frame with `ethernet_receive_frame()`. These are kernel interfaces; user programs can't call them directly.

## Ping and current protocol support

Run `ping 10.0.2.2` to test the QEMU gateway, or `ping google.com` to exercise DHCP, DNS, ARP, and ICMP through QEMU's user-mode network backend. Ethos requests a DHCP lease on the first ping, resolves hostnames with a DNS A query, then sends four ICMP echo requests. The shell blocks until the requests finish.

The E1000 driver still polls frames; there is no background network task, TCP, socket API, HTTP client, IPv6, or configurable interface. Internet reachability depends on QEMU's user network backend and the host's network access. Other networks may use different DHCP, DNS, and gateway settings.

The [ping guide](ping.md) describes the command and its limits.

Ethos can also identify the Intel AX201 PCI device and optionally stage its firmware, but it does not initialize the radio or connect to an access point. See [Wi-Fi: Intel AX201](wifi.md) for the exact status. E1000 remains the only supported network device.
