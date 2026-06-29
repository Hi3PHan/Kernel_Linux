# Codebase Survey - 3 Lab Hien Co

Repo root: `D:\Data\CLionProjects\Lap_Trinh_Nhan_Linux\Lab_Cuoi_Ky`

Ghi chu khao sat:
- Khong viet code dashboard trong buoc nay.
- WSL tren host hien tai bi tu choi khi chay help (`Wsl/Service/CreateInstance/E_ACCESSDENIED`), nen syntax/output duoc lay truc tiep tu source, Makefile va README.
- Cac lenh can `sudo`, `insmod`, `rmmod`, package manager hoac thay doi thoi gian he thong chua duoc chay de tranh side effect.
- Neu README va parser/source khac nhau, uu tien source.

## Lab 1: Shell & Automation

- Duong dan lab: `01_shell_system_admin`
- Entrypoint: `01_shell_system_admin/scripts/admin_tool.sh`
- Duong dan tuyet doi: `D:\Data\CLionProjects\Lap_Trinh_Nhan_Linux\Lab_Cuoi_Ky\01_shell_system_admin\scripts\admin_tool.sh`
- Cach goi command mode tu repo root: `./01_shell_system_admin/scripts/admin_tool.sh --cmd <subcommand> [args]`
- Cach goi theo README: `cd 01_shell_system_admin && ./scripts/admin_tool.sh`
- Neu khong co `--cmd`, script vao menu tuong tac va doc input bang `read`.
- Cach bypass tuong tac: dung `--cmd`; cac thao tac nguy hiem can them `--yes`.
- Log file: `${HOME}/.shell_system_admin_lab/admin_tool.log`
- Log format: `[YYYY-MM-DD HH:MM:SS] ACTION key=value ...`
- Output chinh: plain text, mot so lenh in output cua tool Linux (`ls -lah`, `find`, `date`, `timedatectl`, `tail`); khong co JSON.
- README co nhac `at`/job chay mot lan, nhung source hien tai khong co menu/command `at`.

### Danh sach subcommand

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `help`, `-h`, `--help` | none | Plain text usage | One-shot, exit 0. |
| `list` | `<dir>` path directory | `ls -lah` table | Loi: `Thu muc khong ton tai: <dir>` ra stderr, exit 1. |
| `create-file` | `<path>` path | `Da tao file: <path>` | Tao file bang `touch`; loi neu path da ton tai. |
| `create-dir` | `<path>` path | `Da tao thu muc: <path>` | Dung `mkdir -p`; thanh cong ca khi dir da ton tai. |
| `copy` | `<src>` path, `<dst>` path | `Da copy "<src>" -> "<dst>"` | Dung `cp -a`; loi neu src khong ton tai. |
| `move` | `<src>` path, `<dst>` path | `Da move/rename "<src>" -> "<dst>"` | Dung `mv`; loi neu src khong ton tai. |
| `delete` | `<path>` path, `--yes` required | `Da xoa: <path>` | Dung `rm -rf --`; thieu `--yes` in stderr `Tu choi thao tac nguy hiem vi thieu --yes.` va exit 1. |
| `find` | `<dir>` directory, `<pattern>` string | Moi ket qua 1 path | Dung `find <dir> -iname "*<pattern>*" -print`. |
| `backup` | `<src-dir>` directory, `<dst-dir>` directory | `Da tao backup: <archive>` | Tao `<dst>/<basename>_YYYYMMDD_HHMMSS.tar.gz`; can command `tar`. |
| `cron-list` | none | Header `Cac cron job cua lab:` + cron lines hoac `Chua co job nao.` | Can `crontab`; loc tag `# LAB_ADMIN_TOOL:`. |
| `cron-add` | `<name>` string, `<expr>` cron expr string, `<command>` string | `Da them cron job: <name>` | Can `crontab`; command la 1 argv duy nhat, nen dashboard phai quote neu co space. |
| `cron-remove` | `<name>` string, `--yes` required | `Da xoa neu ton tai cron job: <name>` | Xoa dong khop `# LAB_ADMIN_TOOL:<name>`; can `crontab`. |
| `time-show` | none | `date` output + `timedatectl` output neu co | One-shot; khong can root de xem. |
| `timezone-set` | `<zone>` string, `--yes` required | `Da doi timezone thanh: <zone>` | Can `timedatectl` va chay `sudo timedatectl set-timezone`. Exit code co the khong phan anh loi `sudo` vi script khong check return cua `sudo`. |
| `time-set` | `<YYYY-MM-DD HH:MM:SS>` string, `--yes` required | `Da cap nhat thoi gian he thong.` | Chay `sudo timedatectl set-ntp false` va `sudo timedatectl set-time`. Exit code co the khong phan anh loi `sudo`. |
| `ntp-enable` | `--yes` required | `Da bat dong bo thoi gian NTP.` | Chay `sudo timedatectl set-ntp true`. Exit code co the khong phan anh loi `sudo`. |
| `pkg-update` | `--yes` required | Output cua package manager | Detect `apt-get`, `dnf`, `yum`, `pacman`; chay bang `sudo`. Exit code co the khong phan anh loi package manager vi script log sau lenh. |
| `pkg-install` | `<pkg> [pkg...]`, `--yes` required | Output cua package manager | Thieu package: `Danh sach package rong.` exit 1. |
| `pkg-install-file` | `<file>` path, `--yes` required | Output cua package manager | Doc file, bo qua dong rong va dong bat dau bang `#`; moi dong la 1 package argv. |
| `pkg-remove` | `<pkg> [pkg...]`, `--yes` required | Output cua package manager | Thieu package: `Danh sach package rong.` exit 1. |
| `log` | none | 50 dong cuoi log hoac `Chua co log.` | One-shot, doc `${HOME}/.shell_system_admin_lab/admin_tool.log`. |

### Exit code va loi

- Thanh cong command mode thuong tra 0; loi validate/input thuong tra 1.
- Unknown command in stderr: `Lenh command mode khong hop le: <cmd>` + usage, exit 1.
- Sai so tham so in usage ra stderr, exit 1.
- Thieu tool qua `need_cmd`: `Lenh: <cmd> khong ton tai.` in stdout, exit 1.
- Can chu y: script co `set -u` nhung khong co `set -e`; nhom `sudo timedatectl` va package manager khong check status truoc khi in thong bao thanh cong/log, nen dashboard khong nen chi dua vao exit code/thong bao thanh cong cho cac lenh nay.

### One-shot vs stream/tuong tac

- Tat ca lenh `--cmd` cua Lab 1 la one-shot.
- Khong co subcommand stream.
- Mode menu khong co `--cmd` la tuong tac va can input `read`; dashboard nen tranh goi mode nay.

## Lab 2: Process & Network

- Duong dan lab: `02_ubuntu_process_file_socket_network`
- Binary/entrypoint hien co: `02_ubuntu_process_file_socket_network/ubuntu_sys_tool`
- Source entrypoint: `02_ubuntu_process_file_socket_network/src/main.c`
- Command parser: `02_ubuntu_process_file_socket_network/src/cli_commands.c`
- Duong dan tuyet doi binary: `D:\Data\CLionProjects\Lap_Trinh_Nhan_Linux\Lab_Cuoi_Ky\02_ubuntu_process_file_socket_network\ubuntu_sys_tool`
- Cach build: `cd 02_ubuntu_process_file_socket_network && make`
- Target Makefile: `ubuntu_sys_tool`
- Cach goi command mode tu repo root: `./02_ubuntu_process_file_socket_network/ubuntu_sys_tool --cmd <subcommand> [args]`
- Cach goi theo README: `cd 02_ubuntu_process_file_socket_network && ./ubuntu_sys_tool`
- Neu khong co `--cmd`, binary vao menu tuong tac.
- Log file rieng: khong thay trong code.
- Output chinh: plain text/table/raw file content; khong co JSON.

### Danh sach subcommand

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `help`, `-h`, `--help` | none | Plain text usage | Exit 0. |
| `ps` | none | Table: `PID NAME STATUS` | Doc toan bo PID trong `/proc`, one-shot. |
| `proc-info` | `<pid>` integer 1..4194304 | Cac dong tu `/proc/<pid>/status`: `Name`, `State`, `Pid`, `PPid`, `VmRSS` | PID invalid: `PID khong hop le.` stderr, exit 1. |
| `start-process` | `<command>` string | `Da tao tien trinh con PID=<pid>` sau do `PID <pid> ket thuc, exit=<code>.` hoac bi signal | Fork/exec qua `/bin/sh -c <command>`, parent wait bang `waitpid(WNOHANG)`. Command la 1 argv duy nhat; co the chay lau. |
| `kill` | `<pid>` integer, `[signal]` integer 1..64 optional, default SIGTERM | `Da gui signal <sig> toi PID <pid>` | Loi system in bang `perror("kill")`, vi du `kill: Operation not permitted`. |
| `stat` | `<path>` path | Key-value: `Path`, `Size`, `Mode`, `UID/GID`, `Inode` | Loi `stat: <message>` stderr, exit 1. |
| `read-file` | `<path>` path | Raw bytes/file content ra stdout | Loi `open`, `read`, hoac `write stdout` bang perror. |
| `write-file` | `<path>` path, `<text>` string | `Da ghi vao file: <path>` | Append text + newline; text la 1 argv duy nhat. |
| `copy-file` | `<src>` path, `<dst>` path | `Da copy "<src>" -> "<dst>"` | Dung syscall read/write. |
| `chmod` | `<path>` path, `<mode>` octal 000..777 | `Da doi quyen <path> thanh <mode>` | Mode invalid: `Mode khong hop le.` stderr. |
| `delete-file` | `<path>` path, `--yes` required | `Da xoa file: <path>` | Thieu `--yes`: `Tu choi xoa file vi thieu --yes.` stderr. Parser unlink `argv[0]` va khong enforce exactly 1 non-yes arg. |
| `interfaces` | none | Table: `Interface Family Address` | Doc `getifaddrs()`, one-shot. |
| `resolve` | `<host>` string | Moi IP 1 dong | Dung `getaddrinfo`; loi: `getaddrinfo: <message>` stderr. |
| `tcp-connect` | `<host>` string, `<port>` string/number | `Ket noi TCP thanh cong toi host:port` hoac `Khong ket noi duoc toi host:port` | Port duoc dua vao `getaddrinfo` dang string, code khong parse range rieng. |
| `routes` | none | Raw `/proc/net/route` | One-shot. |
| `tcp-server` | `<port>` integer 1..65535 | Stream text | Long-running. Ready line: `Dang listen TCP port <port>. Gui SIGTERM/SIGINT de dung.` Stop line: `TCP server da dung.` |
| `tcp-client` | `<host>` string, `<port>` integer 1..65535, `<message>` string | `Server tra ve: <echo>` | Host co the la IPv4 hoac hostname qua `gethostbyname`; message la 1 argv. |
| `udp-receiver` | `<port>` integer 1..65535 | Stream text | Long-running. Ready line: `Dang nhan UDP port <port>. Gui SIGTERM/SIGINT de dung.` Moi packet: `Tu <ip>:<port> -> <message>`. Stop line: `UDP receiver da dung.` |
| `udp-send` | `<host>` IPv4 string, `<port>` integer 1..65535, `<message>` string | `Da gui UDP datagram.` | Host phai la IPv4 hop le; hostname bi tu choi voi `Vui long nhap IPv4 hop le cho UDP demo.` |

### Exit code va loi

- `run_command_mode` tra 0 khi thanh cong, 1 khi validate/system error.
- Sai command: `Lenh command mode khong hop le: <cmd>` stderr + usage, exit 1.
- Sai so tham so: in usage, exit 1.
- Port/PID/signal invalid: thong diep tieng Viet ra stderr, exit 1.
- System call failures dung `perror`, vi du `bind: Address already in use`, `bind: Permission denied`, `open: Permission denied`, `unlink: Permission denied`.
- Lab nay khong yeu cau root cho phan chinh, nhung quyen user Linux van anh huong `kill`, `chmod`, `delete-file`, file write/copy, bind port thap.

### One-shot vs stream/tuong tac

- One-shot: `ps`, `proc-info`, `kill`, `stat`, `read-file`, `write-file`, `copy-file`, `chmod`, `delete-file`, `interfaces`, `resolve`, `tcp-connect`, `routes`, `tcp-client`, `udp-send`.
- Stream/long-running: `start-process`, `tcp-server`, `udp-receiver`.
- Dashboard can track PID va stop bang SIGTERM/SIGINT; source co handler dong socket khi nhan signal.
- Fork/exec co subcommand `--cmd start-process <command>` tuong ung voi menu tao process.

## Lab 3: Kernel Modules & Firewall

- Duong dan lab: `03_kernel_module_networking`
- Duong dan tuyet doi: `D:\Data\CLionProjects\Lap_Trinh_Nhan_Linux\Lab_Cuoi_Ky\03_kernel_module_networking`
- Lab nay gom nhieu sub-lab kernel module va user-space CLI rieng.
- Build kernel modules dung Kbuild: `make -C /lib/modules/$(uname -r)/build M=$(pwd) modules`.
- Load/unload trong README dung `insmod`/`rmmod`, khong thay `modprobe` trong repo.
- Xem log kernel bang `dmesg`, co the can `sudo dmesg` tren may bat `kernel.dmesg_restrict`.

### Module/CLI inventory

| Thanh phan | Duong dan | Build command | Output sau build | Load/unload | Ghi chu |
|---|---|---|---|---|---|
| `packet_logger` | `03_kernel_module_networking/1-2-netfilter/packet_logger` | `make` | `packet_logger.ko` (dang co) | `sudo insmod packet_logger.ko`; `sudo rmmod packet_logger` | Khong co user CLI. Hook `NF_INET_LOCAL_OUT`, log TCP SYN, luon accept. |
| `dst_filter` | `03_kernel_module_networking/1-2-netfilter/dst_filter` | `make` hoac `make module`, `make user` | `dst_filter.ko` va `filterctl` (dang co) | `sudo insmod dst_filter.ko`; `sudo rmmod dst_filter` | Device `/dev/dst_filter`, mode 0600. |
| `tcp_listen` | `03_kernel_module_networking/3-4-tcp-sock/tcp_listen` | `make` | `tcp_listen.ko` (dang co) | `sudo insmod tcp_listen.ko`; `sudo rmmod tcp_listen` | Tao TCP listen socket port 60000. Source bind `INADDR_ANY`, log noi `127.0.0.1:60000`. |
| `tcp_accept` | `03_kernel_module_networking/3-4-tcp-sock/tcp_accept` | `make` | `tcp_accept.ko` (dang co) | `sudo insmod tcp_accept.ko`; `sudo rmmod tcp_accept` | Tao kernel thread `tcp_accept_lab`, listen loopback port 60000. Khong load cung luc voi `tcp_listen`. |
| `udp_sender` | `03_kernel_module_networking/5-udp-sock/udp_sender` | `make` | `udp_sender.ko` (dang co) | `sudo insmod udp_sender.ko`; `sudo rmmod udp_sender` | Khi insmod gui payload `kernelsocket` toi `127.0.0.1:60001`; module van loaded cho den rmmod. |
| `tcp_udp_steg` | `03_kernel_module_networking/6-tcp-udp-steg` | `make` hoac `make module`, `make user` | `tcp_udp_steg.ko`, `stegctl` (chua thay artifact trong working tree hien tai) | `sudo insmod tcp_udp_steg.ko`; `sudo rmmod tcp_udp_steg` | Device `/dev/tcp_udp_steg`, mode 0600; netfilter LOCAL_OUT/LOCAL_IN. |
| `edge_firewall` | `03_kernel_module_networking/edge_firewall` | `make` hoac `make module`, `make user`, `make scripts` | `local_fw.ko`, `forward_fw.ko`, `fwctl` (dang co) | `sudo insmod local_fw.ko`; `sudo insmod forward_fw.ko`; `sudo rmmod forward_fw`; `sudo rmmod local_fw` | Device `/dev/local_fw`, `/dev/forward_fw`, mode 0600; hook LOCAL_OUT va FORWARD. |
| `packet_capture` | `03_kernel_module_networking/packet_capture` | `make` hoac `make module`, `make user` | `packet_capture.ko`, `capturectl` | `sudo insmod packet_capture.ko`; `sudo rmmod packet_capture` | Device `/dev/packet_capture`, mode 0600; hook LOCAL_IN va LOCAL_OUT; capture ICMP/TCP/UDP realtime, luon accept. |

### `filterctl` command thuc te

Cach goi: `cd 03_kernel_module_networking/1-2-netfilter/dst_filter && sudo ./filterctl <subcommand>`

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `set` | `<ipv4>` IPv4 string | `dst_filter: rule set to <ip>` | Invalid IP: `invalid IPv4 address: <ip>` stderr. |
| `clear` | none | `dst_filter: rule cleared` | Xoa rule active. |
| `status` | none | `dst_filter: disabled` hoac `dst_filter: enabled dst=<ip>` | Doc rule qua ioctl. |

Exit/permission:
- Mo device fail: `open /dev/dst_filter failed: <strerror>` stderr, exit 1.
- Device mode 0600 nen user thuong co the gap `Permission denied`; module chua load co the la `No such file or directory`.

Kernel log:
- Load: `dst_filter: loaded, device=/dev/dst_filter`
- Set: `dst_filter: set dst=<ip> enabled=1`
- Clear: `dst_filter: cleared rule`
- Packet match: `TCP connection initiated from <src-ip>:<src-port>`
- Filter goi y: `dmesg | grep -E 'dst_filter|TCP connection initiated'`

### `stegctl` command thuc te

Cach goi: `cd 03_kernel_module_networking/6-tcp-udp-steg && sudo ./stegctl <subcommand>`

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `set` | `<message...>` string words | `loaded <bytes> bytes (<bits> bits)` | Parser chap nhan argc >= 3 va ghep argv[2..] bang space; max 256 byte. |
| `clear` | none | `cleared` | Xoa TX/RX state trong module. |
| `status` | none | Multi-line key-value: `enabled`, `msg bytes`, `sent bits`, `last char` | Struct co them counters nhung CLI hien tai khong in counters. |

Exit/permission:
- Mo device fail: `open /dev/tcp_udp_steg: <strerror>` stderr, exit 1.
- Message qua dai: `message too long (max 256)` stderr, exit 1.
- Ioctl fail: `SET_MESSAGE: <strerror>`, `CLEAR: <strerror>`, `GET_STATUS: <strerror>`.

Kernel log:
- Load: `tcp_udp_steg: loaded (/dev/tcp_udp_steg)`
- Set: `tcp_udp_steg: set <bytes> bytes (<bits> bits)`
- Clear: `tcp_udp_steg: cleared`
- RX: `tcp_udp_steg: RX '<char>' (0xXX)` hoac `tcp_udp_steg: RX 0xXX`
- TX bit log la `pr_debug`, khong dam bao hien trong dmesg mac dinh.
- Filter goi y: `dmesg | grep tcp_udp_steg`

### `fwctl` command thuc te

Cach goi: `cd 03_kernel_module_networking/edge_firewall && sudo ./fwctl <subcommand>`

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `status` | `[local|forward|both]` optional, default `both` | Neu missing module/device: `[local] module not loaded or device missing (/dev/local_fw)`. Neu no rule: `[local] no active rule, default action is accept`. Neu active: multi-line active rule. | `status both` tra OR exit code cua local/forward. |
| `stats` | `[local|forward|both]` optional, default `both` | Multi-line counters: `packets_seen`, `packets_matched`, `packets_accepted`, `packets_dropped`, `local_packets`, `forwarded_packets` | Missing module/device in stdout nhu `status`. |
| `clear` | `[local|forward|both]` optional, default `both` | `[local] rule cleared` | Open device loi in stderr `<label>: open /dev/... failed: <strerror>`. |
| `log` | `<local|forward|both> <any|tcp|udp|icmp> <src-ip|any> <dst-ip|any> <src-port|any> <dst-port|any>` | `[local] rule installed` | Action log van `NF_ACCEPT`. |
| `accept` | Same rule args as `log` | `[local] rule installed` | Action accept. |
| `drop` | Same rule args as `log` | `[local] rule installed` | Action drop. |

Rule grammar tu source:

```text
fwctl <log|accept|drop> <local|forward|both> <any|tcp|udp|icmp> \
      <src-ip|any> <dst-ip|any> <src-port|any> <dst-port|any>
```

Rule constraints:
- IP phai la IPv4 literal hoac `any`.
- Port phai la `any` hoac number 1..65535.
- Protocol `icmp` bat buoc `src-port=any` va `dst-port=any`; neu sai: `icmp rule must use src-port=any and dst-port=any` stderr.
- `both` khong gui vao kernel; CLI tach thanh rule rieng cho `/dev/local_fw` va `/dev/forward_fw`.
- README co vi du `sudo ./fwctl drop forward icmp any 8.8.8.8` bi thieu `any any`; source yeu cau du 7 tham so sau action.

Exit/permission:
- Khong co argv: usage stderr, exit 1.
- Target/action/protocol/IP/port/syntax sai: usage stderr, exit 1.
- `status`/`stats` mo device voi `quiet=true`; neu non-root gap `Permission denied`, output van co the bi gom chung thanh `module not loaded or device missing`, exit 1. Dashboard khong nen dung rieng message nay de ket luan module chua load.
- `clear` va rule install mo device voi `quiet=false`; loi ro hon: `local: open /dev/local_fw failed: Permission denied` hoac `No such file or directory`.

Kernel log:
- Local load: `local_fw: loaded, device=/dev/local_fw, hook=NF_INET_LOCAL_OUT`
- Forward load: `forward_fw: loaded, device=/dev/forward_fw, hook=NF_INET_FORWARD`
- Set rule: `<local_fw|forward_fw>: set rule action=<action> direction=<direction> protocol=<protocol>`
- Clear: `<local_fw|forward_fw>: cleared active rule`
- Packet log/drop/accept TCP/UDP: `<tag>: <action> <proto> <src-ip>:<src-port> -> <dst-ip>:<dst-port>`
- Packet log/drop/accept ICMP: `<tag>: <action> icmp <src-ip> -> <dst-ip>`
- Filter goi y: `dmesg | grep -E 'local_fw|forward_fw'`

### `capturectl` command thuc te

Cach goi: `cd 03_kernel_module_networking/packet_capture && sudo ./capturectl <subcommand>`

| Subcommand | Tham so | Output format | Ghi chu |
|---|---|---|---|
| `status` | none | `packet_capture: disabled` hoac `packet_capture: enabled protocol=<proto> src=<ip|any> direction=<dir>` | Doc filter active qua ioctl. |
| `stats` | none | Multi-line counters: `packets_seen`, `packets_matched`, `records_stored`, `overwritten_records`, `dropped_records`, `newest_seq`, `buffer_count`, `buffer_size` | Dung de biet ring buffer co bi ghi de khong. |
| `clear` | none | `packet_capture: records cleared` | Xoa ring buffer va reset counters, giu filter active. |
| `set` | `<any|icmp|tcp|udp> <src-ip|any> <in|out|both>` | `packet_capture: filter set protocol=<proto> src=<ip|any> direction=<dir>` | Bat capture theo filter. |
| `read` | `[--since <seq>] [--limit <n>]` | Header `SEQ TIME_NS DIR PROTO SRC_IP SRC_PORT DST_IP DST_PORT LEN` + cac packet line | One-shot read. |
| `stream` | `[--since <seq>] [--interval-ms <n>]` | Cung format voi `read`, chay long-running va flush realtime | Dashboard dung `CommandRunner::commandOutput` de parse tung dong. |

### Edge firewall scripts

| Script | Cach goi | Output | Root detect |
|---|---|---|---|
| `scripts/setup_forwarding.sh` | `sudo ./scripts/setup_forwarding.sh` | In `[edge_firewall] enabling IPv4 forwarding`, chay `sysctl -w net.ipv4.ip_forward=1`, in `ip -br addr`, `ip route`, va reminder NAT/client VM. | Neu non-root: `Please run as root: sudo $0` stderr, exit 1. |
| `scripts/disable_forwarding.sh` | `sudo ./scripts/disable_forwarding.sh` | In `[edge_firewall] disabling IPv4 forwarding`, chay `sysctl -w net.ipv4.ip_forward=0`, in state. | Neu non-root: `Please run as root: sudo $0` stderr, exit 1. |
| `test_edge_firewall.sh` | `sudo ./test_edge_firewall.sh` | Smoke test local ICMP drop; co cleanup trap `fwctl clear local` va `rmmod local_fw`. | Neu non-root: `Please run as root: sudo $0` stderr, exit 1. Neu chua build: `Build first: make` stderr, exit 1. |

## Quyen root

### Lab 1

- Chay duoc user thuong neu thao tac nam trong quyen filesystem user: `list`, `create-file`, `create-dir`, `copy`, `move`, `delete`, `find`, `backup`, `cron-*`, `time-show`, `log`.
- Can `sudo`/system privilege: `timezone-set`, `time-set`, `ntp-enable`, `pkg-update`, `pkg-install`, `pkg-install-file`, `pkg-remove`.
- Detect thieu `--yes`: stderr `Tu choi thao tac nguy hiem vi thieu --yes.`, exit 1.
- Detect thieu command dependency: stdout `Lenh: <cmd> khong ton tai.`, exit 1.
- Detect thieu quyen sudo/package/time thuc te: chua xac minh tren VM; source khong check status chat che nen can parse stderr cua `sudo`/package manager va khong tin moi exit code.

### Lab 2

- Khong co check root rieng.
- User thuong chay duoc cac lenh doc `/proc`, doc route, interface, DNS, socket port cao.
- Co the can privilege hoac owner permission cho `kill`, `chmod`, `delete-file`, write/copy file path he thong, bind port thap.
- Detect thieu quyen qua `perror`: vi du `kill: Operation not permitted`, `open: Permission denied`, `chmod: Permission denied`, `unlink: Permission denied`, `bind: Permission denied`.

### Lab 3

- Build `make` thuong khong can root nhung can kernel headers/build toolchain.
- `insmod`, `rmmod`, `sysctl -w net.ipv4.ip_forward=...` can root.
- `fwctl`, `filterctl`, `stegctl` can root khi device node mode 0600; non-root thuong gap `Permission denied`.
- `dmesg` co the can root tuy cau hinh kernel.
- Thong diep stderr chuan cua `insmod`/`rmmod` phu thuoc distro/module state va chua duoc xac minh tren VM target do khong chay root. Dashboard nen dung nonzero exit + stderr, va neu can chinh xac thi can test tren Ubuntu VM.

## Process/Module song lau can quan ly lifecycle

| Thanh phan | Loai lifecycle | Ready signal | Stop/cleanup |
|---|---|---|---|
| Lab 2 `tcp-server` | User process long-running | stdout `Dang listen TCP port <port>. Gui SIGTERM/SIGINT de dung.` | Track PID, gui SIGTERM/SIGINT; stop line `TCP server da dung.` |
| Lab 2 `udp-receiver` | User process long-running | stdout `Dang nhan UDP port <port>. Gui SIGTERM/SIGINT de dung.` | Track PID, gui SIGTERM/SIGINT; stop line `UDP receiver da dung.` |
| Lab 3 `tcp_listen.ko` | Kernel module persistent | `insmod` return + dmesg `tcp_listen: listening on 127.0.0.1:60000` | `sudo rmmod tcp_listen`; module giu socket port 60000. |
| Lab 3 `tcp_accept.ko` | Kernel module persistent + kernel thread | `insmod` return + dmesg `tcp_accept: listening on 127.0.0.1:60000` | `sudo rmmod tcp_accept`; dung kernel thread `tcp_accept_lab`. |
| Lab 3 `udp_sender.ko` | Kernel module persistent | `insmod` return + dmesg `udp_sender: socket created, sending message...`/`sent ...` | `sudo rmmod udp_sender`; insmod co side effect gui UDP 1 lan. |
| Lab 3 `packet_logger.ko` | Kernel module persistent | `insmod` return; packet log khi co TCP SYN | `sudo rmmod packet_logger`. |
| Lab 3 `dst_filter.ko` | Kernel module persistent + `/dev/dst_filter` | dmesg `dst_filter: loaded, device=/dev/dst_filter`; `filterctl status` | `sudo ./filterctl clear` tuy chon; `sudo rmmod dst_filter`. |
| Lab 3 `tcp_udp_steg.ko` | Kernel module persistent + `/dev/tcp_udp_steg` | dmesg `tcp_udp_steg: loaded (/dev/tcp_udp_steg)`; `stegctl status` | `sudo ./stegctl clear` tuy chon; `sudo rmmod tcp_udp_steg`. |
| Lab 3 `local_fw.ko` | Kernel module persistent + `/dev/local_fw` | dmesg `local_fw: loaded, device=/dev/local_fw, hook=NF_INET_LOCAL_OUT`; `fwctl status local` | `sudo ./fwctl clear local` tuy chon; `sudo rmmod local_fw`. |
| Lab 3 `forward_fw.ko` | Kernel module persistent + `/dev/forward_fw` | dmesg `forward_fw: loaded, device=/dev/forward_fw, hook=NF_INET_FORWARD`; `fwctl status forward` | `sudo ./fwctl clear forward` tuy chon; `sudo rmmod forward_fw`. |
| `setup_forwarding.sh` | System sysctl state | stdout `net.ipv4.ip_forward = 1` tu `sysctl` | Dung `sudo ./scripts/disable_forwarding.sh` de tra ve `0`. |

Quyet dinh cleanup de dashboard dung sau nay:
- Khong tu dong `rmmod` module khi dong app neu module da loaded truoc khi dashboard mo; day la state he thong/VM cua lab.
- Neu dashboard sau nay tu load module trong session va user bat cleanup, co the clear rule roi rmmod theo thu tu an toan: `forward_fw` truoc `local_fw`; cac module port 60000 (`tcp_listen`/`tcp_accept`) chi mot module tai mot thoi diem.
- Luon track PID cho Lab 2 stream process de kill khi Stop/dong app.
- Khong tu chay package install/remove, time set, delete, sysctl forwarding hay insmod/rmmod neu user chua xac nhan ro.
