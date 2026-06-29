# Lab: Lap trinh quan ly tien trinh, file, socket va network trong Ubuntu

Lab nay viet bang C/POSIX, chay tren Ubuntu. Chuong trinh gom menu tong hop
de demo cac nhom kien thuc he thong:

- Quan ly tien trinh.
- Quan ly file.
- Lap trinh socket TCP/UDP.
- Doc thong tin va kiem tra network trong Ubuntu.

## Cau truc

```text
02_ubuntu_process_file_socket_network/
|-- Makefile
|-- README.md
`-- src/
    |-- common.c/.h
    |-- main.c
    |-- process_manager.c/.h
    |-- file_manager.c/.h
    |-- socket_tools.c/.h
    `-- network_info.c/.h
```

## Yeu cau moi truong

Tren Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential
```

## Build va chay

```bash
cd 02_ubuntu_process_file_socket_network
make
./ubuntu_sys_tool
```

Don dep file build:

```bash
make clean
```

## Chuc nang da cai dat

### 1. Quan ly tien trinh

File code: `src/process_manager.c`

- Liet ke tien trinh bang cach doc `/proc`.
- Xem chi tiet tien trinh tu `/proc/<pid>/status`.
- Tao tien trinh moi bang `fork()` va `execl()`.
- Gui signal toi tien trinh bang `kill()`.
- Kiem tra tien trinh con bang `waitpid(..., WNOHANG)`.

Demo:

```text
Menu 1 -> 3
Lenh: sleep 30

Menu 1 -> 5
Menu 1 -> 4
PID: <pid vua tao>
Signal: 15
```

### 2. Quan ly file

File code: `src/file_manager.c`

- Xem thong tin file bang `stat()`.
- Tao/ghi file bang `open()` va `write()`.
- Doc file bang `open()` va `read()`.
- Copy file bang vong lap `read()`/`write()`.
- Doi quyen file bang `chmod()`.
- Xoa file bang `unlink()`.

Demo:

```text
Menu 2 -> 2
File: /tmp/lab_file.txt
Noi dung: hello ubuntu

Menu 2 -> 3
File: /tmp/lab_file.txt

Menu 2 -> 4
Nguon: /tmp/lab_file.txt
Dich: /tmp/lab_file_copy.txt
```

### 3. Socket TCP/UDP

File code: `src/socket_tools.c`

- TCP echo server: `socket()`, `setsockopt()`, `bind()`, `listen()`,
  `accept()`, `read()`, `write()`.
- TCP client: `socket()`, `connect()`, gui message va nhan echo.
- UDP receiver: `socket()`, `bind()`, `recvfrom()`.
- UDP sender: `socket()`, `sendto()`.

Demo TCP bang 2 terminal:

Terminal 1:

```bash
./ubuntu_sys_tool
# Menu 3 -> 1
# Port: 9000
```

Terminal 2:

```bash
./ubuntu_sys_tool
# Menu 3 -> 2
# Host: 127.0.0.1
# Port: 9000
# Message: hello tcp
```

Demo UDP bang 2 terminal:

Terminal 1:

```bash
./ubuntu_sys_tool
# Menu 3 -> 3
# Port: 9001
```

Terminal 2:

```bash
./ubuntu_sys_tool
# Menu 3 -> 4
# Host: 127.0.0.1
# Port: 9001
# Message: hello udp
```

### 4. Network trong Ubuntu

File code: `src/network_info.c`

- Liet ke network interface va dia chi IP bang `getifaddrs()`.
- Resolve hostname bang `getaddrinfo()`.
- Test ket noi TCP toi `host:port`.
- Doc bang route tu `/proc/net/route`.

Demo:

```text
Menu 4 -> 1
Menu 4 -> 2
Hostname: ubuntu.com
Menu 4 -> 3
Host: ubuntu.com
Port: 80
Menu 4 -> 4
```

## Ghi chu khi bao cao

Co the trinh bay lab theo 4 module:

1. `process_manager.c`: tien trinh va signal.
2. `file_manager.c`: syscall file.
3. `socket_tools.c`: TCP/UDP socket.
4. `network_info.c`: interface, DNS, route va connect test.

Lab khong can quyen root cho phan chinh. Mot so thao tac co the that bai neu
nguoi dung khong co quyen gui signal/xoa file cua user khac; day la hanh vi
binh thuong cua Ubuntu.
