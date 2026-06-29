/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PACKET_CAPTURE_IOCTL_H
#define PACKET_CAPTURE_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define PACKET_CAPTURE_DEVICE_NAME "packet_capture"
#define PACKET_CAPTURE_DEVICE_PATH "/dev/packet_capture"
#define PACKET_CAPTURE_RING_SIZE 1024
#define PACKET_CAPTURE_MAX_READ 64
#define PACKET_CAPTURE_SNAPSHOT_BYTES 128

enum packet_capture_direction {
	PACKET_CAPTURE_DIR_IN = 1,
	PACKET_CAPTURE_DIR_OUT = 2,
	PACKET_CAPTURE_DIR_BOTH = 3,
};

enum packet_capture_protocol {
	PACKET_CAPTURE_PROTO_ANY = 0,
	PACKET_CAPTURE_PROTO_ICMP = 1,
	PACKET_CAPTURE_PROTO_TCP = 6,
	PACKET_CAPTURE_PROTO_UDP = 17,
};

struct packet_capture_filter {
	__u8 enabled;
	__u8 protocol;
	__u8 direction;
	__u8 reserved;
	__be32 src_addr;
	__be32 dst_addr;
};

struct packet_capture_record {
	__u64 seq;
	__u64 timestamp_ns;
	__u8 direction;
	__u8 protocol;
	__u16 ip_total_len;
	__be32 src_addr;
	__be32 dst_addr;
	__be16 src_port;
	__be16 dst_port;
	__u16 captured_len;
	__u16 payload_offset;
	__u16 payload_len;
	__u16 reserved2;
	__u8 data[PACKET_CAPTURE_SNAPSHOT_BYTES];
};

struct packet_capture_stats {
	__u64 packets_seen;
	__u64 packets_matched;
	__u64 records_stored;
	__u64 overwritten_records;
	__u64 dropped_records;
	__u64 newest_seq;
	__u32 buffer_count;
	__u32 buffer_size;
};

struct packet_capture_read_request {
	__u64 since_seq;
	__u32 limit;
	__u32 count;
	__u64 newest_seq;
	__u64 overwritten_records;
	__u64 dropped_records;
	struct packet_capture_record records[PACKET_CAPTURE_MAX_READ];
};

#define PACKET_CAPTURE_IOCTL_MAGIC 'P'
#define PACKET_CAPTURE_IOCTL_SET_FILTER _IOW(PACKET_CAPTURE_IOCTL_MAGIC, 1, struct packet_capture_filter)
#define PACKET_CAPTURE_IOCTL_GET_FILTER _IOR(PACKET_CAPTURE_IOCTL_MAGIC, 2, struct packet_capture_filter)
#define PACKET_CAPTURE_IOCTL_CLEAR _IO(PACKET_CAPTURE_IOCTL_MAGIC, 3)
#define PACKET_CAPTURE_IOCTL_GET_STATS _IOR(PACKET_CAPTURE_IOCTL_MAGIC, 4, struct packet_capture_stats)
#define PACKET_CAPTURE_IOCTL_READ _IOWR(PACKET_CAPTURE_IOCTL_MAGIC, 5, struct packet_capture_read_request)

#endif
