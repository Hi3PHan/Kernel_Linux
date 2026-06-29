# Lab: Lap trinh shell quan tri he thong

Lab nay bam theo de bai:

> Lap trinh shell de quan ly file, lap lich tac vu, thiet lap thoi gian he
> thong, cai dat/go bo cac chuong trinh tu dong.

## Cau truc

```text
01_shell_system_admin/
|-- README.md
|-- packages.txt
`-- scripts/
    `-- admin_tool.sh
```

## Chuc nang chinh

Script `scripts/admin_tool.sh` gom 4 nhom chuc nang:

1. Quan ly file
   - Liet ke thu muc.
   - Xem thong tin file/thu muc bang `stat`.
   - Doc nhanh noi dung file text.
   - Tao file/thu muc.
   - Copy file/thu muc.
   - Move/rename file/thu muc.
   - Xoa file/thu muc co xac nhan.
   - Tim file theo ten.
   - Backup thu muc thanh file `.tar.gz`.

2. Lap lich tac vu
   - Them cron job.
   - Liet ke cron job do lab tao.
   - Xoa cron job theo ten.
   - Tao job chay mot lan bang `at`.

3. Thiet lap thoi gian he thong
   - Xem thoi gian hien tai.
   - Doi timezone bang `timedatectl`.
   - Dat ngay gio he thong.
   - Bat dong bo NTP.

4. Cai dat/go bo chuong trinh tu dong
   - Tu nhan dien package manager: `apt`, `dnf`, `yum`, `pacman`.
   - Cap nhat package index/cache.
   - Cai package theo danh sach nhap tay.
   - Cai package tu file `packages.txt`.
   - Go package co xac nhan.

Moi thao tac duoc ghi log vao:

```text
~/.shell_system_admin_lab/admin_tool.log
```

## Cach chay tren Linux

Can cai cac goi ho tro demo:

```bash
sudo apt update
sudo apt install -y cron at tar
```

```bash
cd 01_shell_system_admin
chmod +x scripts/admin_tool.sh
./scripts/admin_tool.sh
```

Neu dung Ubuntu/Debian, nen cai cac goi ho tro demo:

```bash
sudo apt-get update
sudo apt-get install -y cron at tar
sudo systemctl enable --now cron
sudo systemctl enable --now atd
```

## Goi y demo nhanh

### 1. Quan ly file

Trong menu `Quan ly file`:

- Tao thu muc `/tmp/lab_shell_demo`.
- Tao file `/tmp/lab_shell_demo/note.txt`.
- Xem thong tin va doc file `/tmp/lab_shell_demo/note.txt`.
- Copy file sang `/tmp/lab_shell_demo/note_copy.txt`.
- Tim file voi tu khoa `note`.
- Backup thu muc `/tmp/lab_shell_demo` vao `/tmp`.

### 2. Lap lich tac vu

Them cron job:

```text
Ten job: write_time
Lich cron: */5 * * * *
Lenh: date >> /tmp/lab_shell_demo/time.log
```

Sau do dung menu liet ke job de thay dong cron co tag `LAB_ADMIN_TOOL`.

### 3. Thoi gian he thong

Xem thoi gian hien tai, sau do co the doi timezone:

```text
Asia/Ho_Chi_Minh
```

Luu y: thao tac dat ngay gio va timezone can quyen `sudo`.

### 4. Cai/go chuong trinh

Chon cai package tu file danh sach va nhap:

```text
packages.txt
```

File nay chi gom cac goi nho de demo. Khi go package, script se hoi xac nhan
truoc khi chay lenh go.

## Luu y an toan

- Script co chuc nang xoa file va go package, nen da co buoc hoi xac nhan.
- Cac thao tac he thong nhu cai package, go package, doi gio he thong can
  `sudo`.
- Nen demo trong may ao Linux de tranh anh huong may chinh.
