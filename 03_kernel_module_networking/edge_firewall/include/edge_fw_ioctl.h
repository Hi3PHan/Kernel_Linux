/* SPDX-License-Identifier: GPL-2.0 */
#ifndef EDGE_FW_IOCTL_H
#define EDGE_FW_IOCTL_H

/*
 * Header dung chung cho kernel module va chuong trinh user-space.
 * Cac kieu __u8, __u16, __u32, __u64 co kich thuoc co dinh nen an toan hon
 * khi truyen du lieu qua ioctl giua user-space va kernel-space.
 */
#include <linux/types.h>
#include <linux/ioctl.h>

#define EDGE_FW_LOCAL_DEVICE_NAME "local_fw"
#define EDGE_FW_FORWARD_DEVICE_NAME "forward_fw"
#define EDGE_FW_LOCAL_DEVICE_PATH "/dev/local_fw"
#define EDGE_FW_FORWARD_DEVICE_PATH "/dev/forward_fw"

/*
 * Direction cho biet rule ap dung cho hook nao:
 * - LOCAL: packet sinh ra tu chinh gateway VM, di qua NF_INET_LOCAL_OUT.
 * - FORWARD: packet di xuyen qua gateway VM, di qua NF_INET_FORWARD.
 * - BOTH: ap dung cho ca hai truong hop tren.
 */
enum edge_fw_direction {
	EDGE_DIR_LOCAL = 1,
	EDGE_DIR_FORWARD = 2,
	EDGE_DIR_BOTH = 3,
};

/*
 * Protocol dung dung gia tri IP protocol number de de so sanh voi iph->protocol.
 * ANY = 0 la convention rieng cua tool, khong phai IP protocol number.
 */
enum edge_fw_protocol {
	EDGE_PROTO_ANY = 0,
	EDGE_PROTO_ICMP = 1,
	EDGE_PROTO_TCP = 6,
	EDGE_PROTO_UDP = 17,
};

/*
 * Action quyet dinh gia tri tra ve cho netfilter:
 * - LOG van tra NF_ACCEPT.
 * - ACCEPT tra NF_ACCEPT.
 * - DROP tra NF_DROP.
 */
enum edge_fw_action {
	EDGE_ACTION_LOG = 1,
	EDGE_ACTION_ACCEPT = 2,
	EDGE_ACTION_DROP = 3,
};

/*
 * Mot rule active duy nhat cho v1. Gia tri IP/port bang 0 duoc hieu la "any".
 * IP va port duoc luu o network byte order de so sanh truc tiep voi header.
 */
struct edge_fw_rule {
	__u8 enabled;
	__u8 direction;
	__u8 protocol;
	__u8 action;
	__be32 src_addr;
	__be32 dst_addr;
	__be16 src_port;
	__be16 dst_port;
};

/*
 * Counters dung de user-space kiem tra module co thay packet va co drop hay
 * khong. Kernel tang counters trong hook, user-space doc qua ioctl.
 */
struct edge_fw_stats {
	__u64 packets_seen;
	__u64 packets_matched;
	__u64 packets_accepted;
	__u64 packets_dropped;
	__u64 local_packets;
	__u64 forwarded_packets;
};

#define EDGE_FW_IOCTL_MAGIC 'E'
#define EDGE_FW_IOCTL_SET_RULE _IOW(EDGE_FW_IOCTL_MAGIC, 1, struct edge_fw_rule)
#define EDGE_FW_IOCTL_CLEAR_RULE _IO(EDGE_FW_IOCTL_MAGIC, 2)
#define EDGE_FW_IOCTL_GET_RULE _IOR(EDGE_FW_IOCTL_MAGIC, 3, struct edge_fw_rule)
#define EDGE_FW_IOCTL_GET_STATS _IOR(EDGE_FW_IOCTL_MAGIC, 4, struct edge_fw_stats)

#endif
