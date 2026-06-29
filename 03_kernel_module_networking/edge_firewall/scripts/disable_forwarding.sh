#!/usr/bin/env bash
set -euo pipefail

# Tat IPv4 forwarding de dua VM ve trang thai an toan sau khi demo.

if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run as root: sudo $0" >&2
    exit 1
fi

echo "[edge_firewall] disabling IPv4 forwarding"
sysctl -w net.ipv4.ip_forward=0

echo
echo "[edge_firewall] forwarding state:"
sysctl net.ipv4.ip_forward
