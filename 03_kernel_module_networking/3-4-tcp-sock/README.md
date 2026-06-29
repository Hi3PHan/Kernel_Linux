# Bai 3-4: TCP Socket Trong Kernel

Thu muc nay danh cho hai bai goc trong Linux Kernel Labs Networking, tach
thanh hai module rieng:

3. `tcp_listen/`: tao TCP socket trong kernel va listen o port `60000`.
4. `tcp_accept/`: tao TCP listener va accept connection trong kernel thread.

Hai module deu dung `sock_create_kern()`, `kernel_bind()`,
`kernel_listen()` va cleanup bang `sock_release()`.

Luu y: `tcp_listen.ko` va `tcp_accept.ko` cung dung port `60000`, nen chi
load mot module tai mot thoi diem.

Build/test nhanh:

```bash
cd tcp_listen && make
sudo insmod tcp_listen.ko
ss -ltn sport = :60000
sudo rmmod tcp_listen

cd ../tcp_accept && make
sudo insmod tcp_accept.ko
nc 127.0.0.1 60000
dmesg | tail -n 20
sudo rmmod tcp_accept
```
