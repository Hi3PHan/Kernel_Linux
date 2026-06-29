# Edge Mini Firewall

Day la cap kernel module firewall nho de demo vai tro thiet bi bien/gateway.
Hai module dung netfilter de quan sat packet local outbound va packet duoc
forward qua VM, nhung duoc load/unload doc lap.

## Thanh phan

```text
edge_firewall/
|-- include/edge_fw_ioctl.h
|-- kernel/edge_fw_core.h
|-- kernel/local_fw_main.c
|-- kernel/forward_fw_main.c
|-- user/fwctl.c
|-- scripts/setup_forwarding.sh
|-- scripts/disable_forwarding.sh
|-- Makefile
`-- test_edge_firewall.sh
```

## Build

Can cai tren Ubuntu VM:

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) netcat-openbsd
make
```

## Load/unload

```bash
sudo insmod local_fw.ko
sudo insmod forward_fw.ko
lsmod | grep -E 'local_fw|forward_fw'
dmesg | tail -n 20
sudo rmmod forward_fw
sudo rmmod local_fw
```

## CLI

```bash
sudo ./fwctl status
sudo ./fwctl status local
sudo ./fwctl status forward
sudo ./fwctl clear
sudo ./fwctl log forward tcp any 192.168.56.10 any 80
sudo ./fwctl drop forward tcp any 192.168.56.10 any 80
sudo ./fwctl drop forward icmp any 8.8.8.8
sudo ./fwctl drop local udp any 127.0.0.1 any 60001
sudo ./fwctl drop both tcp any 93.184.216.34 any 80
sudo ./fwctl stats
```

Cu phap rule:

```text
fwctl <log|accept|drop> <local|forward|both> <any|tcp|udp|icmp> \
      <src-ip|any> <dst-ip|any> <src-port|any> <dst-port|any>
```

## Luu y forwarding

Firewall chi quyet dinh `NF_ACCEPT` hoac `NF_DROP`. No khong thay the routing
table va khong tu NAT. De test nhu thiet bi bien:

1. VM gateway can co 2 card mang, vi du NAT/WAN va host-only/LAN.
2. Bat IPv4 forwarding bang `scripts/setup_forwarding.sh`.
3. Client VM dat default gateway tro ve IP LAN cua gateway VM.
4. Neu can client ra Internet that, them NAT bang iptables/nftables ben ngoai
   module.

## Test nhanh local outbound

```bash
sudo insmod local_fw.ko
sudo ./fwctl drop local icmp any 8.8.8.8
ping -c 3 8.8.8.8
sudo ./fwctl stats local
sudo ./fwctl clear local
sudo rmmod local_fw
```

## Test forward ICMP

```bash
sudo ./scripts/setup_forwarding.sh
sudo insmod forward_fw.ko
sudo ./fwctl drop forward icmp any 8.8.8.8
```

Tu client VM:

```bash
ping -c 3 8.8.8.8
```

Quay lai gateway VM:

```bash
sudo ./fwctl stats forward
dmesg | tail -n 30
```
