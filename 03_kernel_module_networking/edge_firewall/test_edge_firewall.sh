#!/usr/bin/env bash
set -euo pipefail

# Test smoke local outbound. Forwarding test can phai co client VM rieng nen
# khong tu dong chay trong script nay.

if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run as root: sudo $0" >&2
    exit 1
fi

if [[ ! -e ./local_fw.ko || ! -x ./fwctl ]]; then
    echo "Build first: make" >&2
    exit 1
fi

cleanup() {
    ./fwctl clear local >/dev/null 2>&1 || true
    rmmod local_fw >/dev/null 2>&1 || true
}
trap cleanup EXIT

insmod ./local_fw.ko
./fwctl status local

echo "[edge_firewall] install local ICMP drop rule for 8.8.8.8"
./fwctl drop local icmp any 8.8.8.8 any any
./fwctl status local

echo "[edge_firewall] ping is expected to fail while rule is active"
if ping -c 1 -W 1 8.8.8.8; then
    echo "warning: ping succeeded; check whether packet used IPv6 or another path"
fi

./fwctl stats local
./fwctl clear local

echo "[edge_firewall] local smoke test finished"
