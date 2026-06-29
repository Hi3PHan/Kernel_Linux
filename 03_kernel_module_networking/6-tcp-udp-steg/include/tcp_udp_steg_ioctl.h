/* SPDX-License-Identifier: GPL-2.0 */
#ifndef TCP_UDP_STEG_IOCTL_H
#define TCP_UDP_STEG_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define STEG_DEVICE_NAME "tcp_udp_steg"
#define STEG_DEVICE_PATH "/dev/tcp_udp_steg"
#define STEG_MAX_MESSAGE 256

struct steg_message {
	__u32 len;
	char data[STEG_MAX_MESSAGE];
};

struct steg_status {
	__u8 enabled;
	__u32 message_len;
	__u32 tx_bit_pos;
	__u32 tx_total_bits;
	__u64 packets_seen;
	__u64 packets_modified;
	__u64 packets_read;
	__u64 chars_read;
	__u8 last_char;
};

#define STEG_IOCTL_MAGIC 'S'
#define STEG_IOCTL_SET_MESSAGE _IOW(STEG_IOCTL_MAGIC, 1, struct steg_message)
#define STEG_IOCTL_CLEAR _IO(STEG_IOCTL_MAGIC, 2)
#define STEG_IOCTL_GET_STATUS _IOR(STEG_IOCTL_MAGIC, 3, struct steg_status)

#endif
