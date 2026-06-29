# Lab 3: Kernel Module Networking

Lab nay bam theo Linux Kernel Labs - Networking va mo rong thanh mot
edge firewall nho chay trong kernel space.

## Cau truc

```text
03_kernel_module_networking/
|-- 1-2-netfilter/
|-- 3-4-tcp-sock/
|-- 5-udp-sock/
|-- 6-tcp-udp-steg/
|-- edge_firewall/
`-- packet_capture/
```

- `1-2-netfilter/packet_logger`: `packet_logger.ko`, log TCP SYN outbound.
- `1-2-netfilter/dst_filter`: `dst_filter.ko` + `filterctl`, log packet theo
  destination IP cau hinh qua ioctl.
- `3-4-tcp-sock/tcp_listen`: `tcp_listen.ko`, tao TCP listen socket port
  `60000`.
- `3-4-tcp-sock/tcp_accept`: `tcp_accept.ko`, accept TCP connection bang
  kernel thread.
- `5-udp-sock/udp_sender`: `udp_sender.ko`, gui UDP datagram toi
  `127.0.0.1:60001`.
- `6-tcp-udp-steg`: `tcp_udp_steg.ko` + `stegctl`, demo giau 1 bit moi
  packet TCP/UDP bang bit thap nhat cua IPv4 Identification.
- `edge_firewall/`: phan mo rong thiet bi bien, tach thanh `local_fw.ko` va
  `forward_fw.ko`.
- `packet_capture/`: `packet_capture.ko` + `capturectl`, bat packet TCP/UDP
  realtime bang netfilter va luu ring buffer co gioi han.

## Mo rong: Edge Mini Firewall

`edge_firewall` co hai module rieng:

- `local_fw.ko`: dang ky `NF_INET_LOCAL_OUT`, xu ly packet sinh ra tu gateway.
- `forward_fw.ko`: dang ky `NF_INET_FORWARD`, xu ly packet di xuyen gateway.

Hai module khong tu route packet. Routing that van do Linux kernel xu ly. De
packet di vao hook `FORWARD`, VM gateway can bat `net.ipv4.ip_forward=1` va
co route hop le giua LAN/WAN.

## Cach build nhanh tren VM

```bash
cd 03_kernel_module_networking/edge_firewall
make
sudo insmod local_fw.ko
sudo insmod forward_fw.ko
sudo ./fwctl status
sudo rmmod forward_fw
sudo rmmod local_fw
```

Doc them trong `edge_firewall/README.md` de xem cac lenh test local va
forwarding.

## Mo rong: Packet Capture

`packet_capture` la Wireshark mini cho dashboard Qt:

- Hook `NF_INET_LOCAL_IN` va `NF_INET_LOCAL_OUT`.
- Chi quan sat IPv4 ICMP/TCP/UDP va luon tra `NF_ACCEPT`.
- Loc theo protocol, source IP va direction `in/out/both`.
- Luu 1024 packet summary gan nhat trong ring buffer de tranh tran bo nho.
- Moi record co snapshot co dinh 128 byte dau tien de xem noi dung packet bang
  `capturectl detail <seq>` hoac panel detail trong tab `Packet Capture`.

Build nhanh:

```bash
cd 03_kernel_module_networking/packet_capture
make
sudo insmod packet_capture.ko
sudo ./capturectl set any any both
sudo ./capturectl stream --since 0 --interval-ms 100
sudo ./capturectl detail 1
sudo rmmod packet_capture
```
