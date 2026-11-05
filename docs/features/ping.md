# Ping

The shell's `ping` built-in sends four IPv4 ICMP echo requests through the QEMU E1000 adapter. It accepts a numeric IPv4 address or a hostname:

```text
ping 10.0.2.2
ping google.com
```

With QEMU's user-mode network backend, Ethos requests an address, subnet mask, router, and DNS server through DHCP. For a hostname, it asks the assigned DNS server for an A record. It then resolves the destination or gateway MAC address with ARP and sends ICMP echo requests.

A successful QEMU test looked like this:

```text
ethos> ping google.com
PING google.com (74.125.200.101) 32 bytes
32 bytes from 74.125.200.101: icmp_seq=1 ttl=249 time=30 ms
32 bytes from 74.125.200.101: icmp_seq=2 ttl=249 time=20 ms
32 bytes from 74.125.200.101: icmp_seq=3 ttl=249 time=20 ms
32 bytes from 74.125.200.101: icmp_seq=4 ttl=249 time=30 ms
--- ping statistics: 4 sent, 4 received ---
```

DNS answers and round-trip times vary. The PIT runs at 100 Hz, so the displayed RTT has 10 ms resolution; replies in the same tick appear as `<10 ms`.

The command runs synchronously and has no count or timeout options yet. E1000 polling continues while it waits, but the shell can't accept another command during the test. Ethos has no TCP, socket, IPv6, or general-purpose DNS cache yet; the current DHCP and network path targets QEMU's user-mode backend. See [Ethernet](ethernet.md) for the device and protocol limits.
