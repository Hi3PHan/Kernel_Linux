# Bai 5: UDP Sender Trong Kernel

Thu muc nay danh cho bai goc trong Linux Kernel Labs Networking, tach thanh
module rieng:

5. `udp_sender/`: tao UDP socket trong kernel va gui message toi
   `127.0.0.1:60001`. Payload gui di la `kernelsocket`, dung voi
   `MY_TEST_MESSAGE` cua de bai goc.

Phan edge firewall co the loc UDP theo source/destination port, nhung khong
tu gui UDP packet.

Build/test nhanh:

```bash
cd udp_sender && make
nc -u -l -p 60001
sudo insmod udp_sender.ko
dmesg | tail -n 20
sudo rmmod udp_sender
```
