// SPDX-License-Identifier: GPL-2.0
/*
 * local_fw.ko
 *
 * Module rieng cho packet sinh ra tu chinh gateway VM. Hook LOCAL_OUT khong
 * thay packet cua client VM di xuyen qua gateway.
 */

#define EDGE_FW_DEVICE_NAME EDGE_FW_LOCAL_DEVICE_NAME
#define EDGE_FW_DEVICE_PATH EDGE_FW_LOCAL_DEVICE_PATH
#define EDGE_FW_DIRECTION EDGE_DIR_LOCAL
#define EDGE_FW_HOOKNUM NF_INET_LOCAL_OUT
#define EDGE_FW_HOOK_NAME "NF_INET_LOCAL_OUT"
#define EDGE_FW_LOG_PREFIX "local_fw"
#define EDGE_FW_MODULE_DESCRIPTION "Local outbound firewall using NF_INET_LOCAL_OUT"

#include "edge_fw_core.h"
