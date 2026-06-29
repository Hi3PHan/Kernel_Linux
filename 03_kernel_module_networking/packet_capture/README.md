# Packet Capture

`packet_capture.ko` is a small IPv4 ICMP/TCP/UDP packet capture module for the Qt
dashboard. It uses Netfilter only to observe packets and always returns
`NF_ACCEPT`.

## Build

```bash
make
```

## Load and Configure

```bash
sudo insmod packet_capture.ko
sudo ./capturectl set any any both
sudo ./capturectl status
```

Filter syntax:

```text
capturectl set <any|icmp|tcp|udp> <src-ip|any> <in|out|both>
```

## Read Packets

One-shot read:

```bash
sudo ./capturectl read --since 0 --limit 50
```

Realtime stream:

```bash
sudo ./capturectl stream --since 0 --interval-ms 100
```

Output format:

```text
SEQ TIME_NS DIR PROTO SRC_IP SRC_PORT DST_IP DST_PORT LEN
```

Packet detail for one captured seq:

```bash
sudo ./capturectl detail 12
```

`detail` prints the packet summary, decoded IPv4 + TCP/UDP/ICMP fields, payload
preview, and a hex/ASCII dump of the first 128 bytes captured from the IPv4
packet.

## Memory Limits

The kernel module stores only the most recent 1024 packet summaries in a ring
buffer. Each record includes a fixed 128-byte packet snapshot, so memory usage
does not grow with traffic. When traffic is heavier than the reader can consume,
old records are overwritten and the `overwritten_records` counter increases.

Clear the buffer and counters:

```bash
sudo ./capturectl clear
```

Unload:

```bash
sudo rmmod packet_capture
```
