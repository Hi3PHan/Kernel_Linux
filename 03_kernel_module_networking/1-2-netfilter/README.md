# Bai 1-2: Netfilter

Thu muc nay danh cho hai bai goc trong Linux Kernel Labs Networking, tach
thanh hai module rieng:

1. `packet_logger/`: dang ky netfilter hook de hien thi packet TCP SYN
   outbound.
2. `dst_filter/`: them ioctl de cau hinh destination address can log.

Phan mo rong `../edge_firewall` su dung lai y tuong cua bai nay va bo sung
rule theo direction, protocol, IP, port, action `log/accept/drop`.

Build nhanh tung bai:

```bash
cd packet_logger && make
sudo insmod packet_logger.ko
dmesg | tail -n 20
sudo rmmod packet_logger

cd ../dst_filter && make
sudo insmod dst_filter.ko
sudo ./filterctl set 127.0.0.1
nc 127.0.0.1 60000
dmesg | tail -n 20
sudo rmmod dst_filter
```
