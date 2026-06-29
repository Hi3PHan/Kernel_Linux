// SPDX-License-Identifier: GPL-2.0
/*
 * forward_fw.ko
 *
 * Module rieng cho packet duoc forward qua gateway VM. Packet chi di vao hook
 * FORWARD khi VM bat net.ipv4.ip_forward=1 va routing table hop le.
 */

#define EDGE_FW_DEVICE_NAME EDGE_FW_FORWARD_DEVICE_NAME
#define EDGE_FW_DEVICE_PATH EDGE_FW_FORWARD_DEVICE_PATH
#define EDGE_FW_DIRECTION EDGE_DIR_FORWARD
#define EDGE_FW_HOOKNUM NF_INET_FORWARD
#define EDGE_FW_HOOK_NAME "NF_INET_FORWARD"
#define EDGE_FW_LOG_PREFIX "forward_fw"
#define EDGE_FW_MODULE_DESCRIPTION "Forwarded traffic firewall using NF_INET_FORWARD"

#include "edge_fw_core.h"
