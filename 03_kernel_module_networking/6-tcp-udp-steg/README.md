# Bai 6: Giau Tin Trong Goi TCP/UDP

Module `tcp_udp_steg.ko` minh hoa covert channel don gian trong kernel:
moi packet IPv4 TCP/UDP mang 1 bit bi mat trong bit thap nhat cua truong
IPv4 Identification (`iph->id`).

Module khong sua payload TCP/UDP, nen du lieu ung dung van duoc giu nguyen.
Sau khi sua `iph->id`, module tinh lai IP checksum bang `ip_send_check()`.

## Cau truc packet trong skb

```text
struct sk_buff *skb
        |
        v
+-------------------+-------------------+-------------------+----------+
| Ethernet header   | IPv4 header       | TCP/UDP header    | Payload  |
+-------------------+-------------------+-------------------+----------+
                    ^
                    ip_hdr(skb)
```

Vi tri giau bit:

```text
IPv4 Identification: 16 bit

bit: 15 14 13 ... 2 1 0
                         ^
                         bit thap nhat, noi giau 1 bit
```

## Build va chay

```bash
cd 03_kernel_module_networking/6-tcp-udp-steg
make
sudo insmod tcp_udp_steg.ko
sudo ./stegctl set "hello"
```

Tao traffic TCP/UDP, vi du:

```bash
nc -u 127.0.0.1 60001
```

Nhap vai dong bat ky de tao packet UDP. Xem log:

```bash
dmesg | tail -n 30
sudo ./stegctl status
```

Don dep:

```bash
sudo ./stegctl clear
sudo rmmod tcp_udp_steg
```

## Gioi han

- Chi ho tro IPv4 TCP/UDP.
- Bo qua packet bi fragment.
- Moi packet chi mang 1 bit.
- Khong xu ly mat goi, dao thu tu, hay ma hoa noi dung.
- Day la demo hoc thuat, khong phai cong cu bao mat.
