#!/usr/bin/env bash
set -euo pipefail

# Script nay chi cau hinh Linux routing/forwarding cua VM gateway.
# Cac module local_fw/forward_fw khong tu route va khong NAT packet.

if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run as root: sudo $0" >&2
    exit 1
fi

echo "[edge_firewall] enabling IPv4 forwarding"
sysctl -w net.ipv4.ip_forward=1

echo
echo "[edge_firewall] interfaces:"
ip -br addr

echo
echo "[edge_firewall] routes:"
ip route

echo
echo "[edge_firewall] client VM setup reminder:"
echo "  1. Put client VM on the same LAN/host-only network as gateway LAN interface."
echo "  2. Set client default gateway to the gateway VM LAN IP."
echo "  3. If client needs Internet access, add NAT outside this module, for example:"
echo "     sudo iptables -t nat -A POSTROUTING -o <WAN_IFACE> -j MASQUERADE"
