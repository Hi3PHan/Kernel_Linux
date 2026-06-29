// SPDX-License-Identifier: GPL-2.0
/*
 * capturectl - user-space control tool for packet_capture.ko.
 *
 * Usage:
 *   capturectl status
 *   capturectl stats
 *   capturectl clear
 *   capturectl set <any|icmp|tcp|udp> <src-ip|any> <in|out|both> [--dst-ip <ip|any>]
 *   capturectl read [--since <seq>] [--limit <n>]
 *   capturectl stream [--since <seq>] [--interval-ms <n>]
 *   capturectl detail <seq>
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/packet_capture_ioctl.h"

/* ---------- globals ---------- */

static volatile sig_atomic_t stop_requested;

static void handle_signal(int signo) { (void)signo; stop_requested = 1; }

/* ---------- usage ---------- */

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s status\n"
		"  %s stats\n"
		"  %s clear\n"
		"  %s set <any|icmp|tcp|udp> <src-ip|any> <in|out|both> [--dst-ip <ip|any>]\n"
		"  %s read [--since <seq>] [--limit <n>]\n"
		"  %s stream [--since <seq>] [--interval-ms <n>]\n"
		"  %s detail <seq>\n",
		prog, prog, prog, prog, prog, prog, prog);
}

/* ---------- device ---------- */

static int open_device(void)
{
	int fd = open(PACKET_CAPTURE_DEVICE_PATH, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "open %s failed: %s\n",
			PACKET_CAPTURE_DEVICE_PATH, strerror(errno));
	return fd;
}

/* ---------- parse helpers ---------- */

/*
 * Hàm parse_protocol
 * Công dụng: Chuyển chuỗi giao thức từ command line ("any", "icmp", "tcp", "udp")
 * sang hằng số nguyên tương ứng (PACKET_CAPTURE_PROTO_*) để truyền xuống kernel.
 */
static int parse_protocol(const char *str, __u8 *result)
{
	if (!strcmp(str, "any"))  { *result = PACKET_CAPTURE_PROTO_ANY;  return 0; }
	if (!strcmp(str, "icmp")) { *result = PACKET_CAPTURE_PROTO_ICMP; return 0; }
	if (!strcmp(str, "tcp"))  { *result = PACKET_CAPTURE_PROTO_TCP;  return 0; }
	if (!strcmp(str, "udp"))  { *result = PACKET_CAPTURE_PROTO_UDP;  return 0; }
	return -1;
}

/*
 * Hàm parse_direction
 * Công dụng: Chuyển chuỗi hướng bắt gói ("in", "out", "both")
 * sang hằng số nguyên (PACKET_CAPTURE_DIR_*) quy định bởi header ioctl.
 */
static int parse_direction(const char *str, __u8 *result)
{
	if (!strcmp(str, "in"))   { *result = PACKET_CAPTURE_DIR_IN;   return 0; }
	if (!strcmp(str, "out"))  { *result = PACKET_CAPTURE_DIR_OUT;  return 0; }
	if (!strcmp(str, "both")) { *result = PACKET_CAPTURE_DIR_BOTH; return 0; }
	return -1;
}

/*
 * Hàm parse_ip
 * Công dụng: Chuyển đổi chuỗi địa chỉ IPv4 (VD: "192.168.1.1") sang định dạng
 * số nguyên 32-bit (network byte order - __be32). 
 * Nếu tham số là "any", trả về 0 (0 đồng nghĩa với không lọc IP).
 */
static int parse_ip(const char *str, __be32 *result)
{
	struct in_addr in;
	if (!strcmp(str, "any")) { *result = 0; return 0; }
	if (inet_pton(AF_INET, str, &in) != 1) return -1;
	*result = in.s_addr;
	return 0;
}

/*
 * Hàm parse_u64
 * Công dụng: Chuyển đổi một chuỗi văn bản sang số nguyên không dấu 64-bit (uint64_t).
 * Sử dụng cho các tham số như `--since` (số thứ tự gói tin cần đọc).
 */
static int parse_u64(const char *str, uint64_t *result)
{
	char *end;
	errno = 0;
	unsigned long long val = strtoull(str, &end, 10);
	if (errno != 0 || *end) return -1;
	*result = val;
	return 0;
}

/*
 * Hàm parse_u32_range
 * Công dụng: Tương tự parse_u64 nhưng giới hạn giá trị trong một khoảng [lo, hi].
 * Ép kiểu kết quả về uint32_t. Dùng cho `--limit` và `--interval-ms` để tránh
 * giá trị quá lớn gây lỗi.
 */
static int parse_u32_range(const char *str, uint32_t lo, uint32_t hi, uint32_t *result)
{
	uint64_t val;
	if (parse_u64(str, &val) < 0 || val < lo || val > hi) return -1;
	*result = (uint32_t)val;
	return 0;
}

/* ---------- format helpers ---------- */

/*
 * Hàm proto_str
 * Công dụng: Hàm ngược của parse_protocol. Chuyển hằng số giao thức
 * thành chuỗi để in ra màn hình.
 */
static const char *proto_str(__u8 protocol)
{
	switch (protocol) {
	case PACKET_CAPTURE_PROTO_ANY:  return "any";
	case PACKET_CAPTURE_PROTO_ICMP: return "icmp";
	case PACKET_CAPTURE_PROTO_TCP:  return "tcp";
	case PACKET_CAPTURE_PROTO_UDP:  return "udp";
	default:                        return "unknown";
	}
}

/*
 * Hàm dir_str
 * Công dụng: Hàm ngược của parse_direction.
 */
static const char *dir_str(__u8 direction)
{
	switch (direction) {
	case PACKET_CAPTURE_DIR_IN:   return "in";
	case PACKET_CAPTURE_DIR_OUT:  return "out";
	case PACKET_CAPTURE_DIR_BOTH: return "both";
	default:                      return "unknown";
	}
}

/*
 * Hàm ip_str
 * Công dụng: Chuyển đổi số nguyên 32-bit (network byte order) thành chuỗi IPv4
 * dạng x.x.x.x và ghi vào buffer.
 */
static void ip_str(__be32 addr, char *buffer, size_t size)
{
	struct in_addr in = { .s_addr = addr };
	if (!inet_ntop(AF_INET, &in, buffer, size))
		snprintf(buffer, size, "<invalid>");
}

/* 
 * Hàm ip_or_any
 * Công dụng: Tương tự ip_str nhưng nếu địa chỉ là 0 (0.0.0.0) thì in ra "any".
 * Giúp hiển thị trực quan hơn khi in ra cấu hình filter hiện tại.
 */
static void ip_or_any(__be32 addr, char *buffer, size_t size)
{
	if (!addr) { snprintf(buffer, size, "any"); return; }
	ip_str(addr, buffer, size);
}

/* ---------- print helpers ---------- */

/*
 * Hàm print_record_header
 * Công dụng: In ra dòng tiêu đề cho bảng hiển thị các gói tin.
 */
static void print_record_header(void)
{
	printf("SEQ TIME_NS DIR PROTO SRC_IP SRC_PORT DST_IP DST_PORT LEN\n");
}

/*
 * Hàm print_record
 * Công dụng: In ra thông tin cơ bản của một gói tin (một hàng trong bảng hiển thị).
 * Thông tin bao gồm: số thứ tự (SEQ), timestamp, hướng (DIR), giao thức,
 * IP nguồn, cổng nguồn, IP đích, cổng đích, và độ dài gói tin.
 */
static void print_record(const struct packet_capture_record *r)
{
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
	ip_str(r->src_addr, src, sizeof(src));
	ip_str(r->dst_addr, dst, sizeof(dst));
	printf("%llu %llu %s %s %s %u %s %u %u\n",
	       (unsigned long long)r->seq,
	       (unsigned long long)r->timestamp_ns,
	       dir_str(r->direction), proto_str(r->protocol),
	       src, ntohs(r->src_port),
	       dst, ntohs(r->dst_port),
	       r->ip_total_len);
}

/*
 * Hàm print_hex_ascii
 * Công dụng: In nội dung nhị phân của gói tin dưới định dạng hexdump,
 * hiển thị cả mã hex và ký tự ASCII tương ứng.
 * Rất hữu ích khi người dùng xem chi tiết payload.
 */
static void print_hex_ascii(const __u8 *data, uint16_t len)
{
	uint16_t off;
	for (off = 0; off < len; off += 16) {
		uint16_t i, line = (uint16_t)(len - off);
		if (line > 16) 
			line = 16;

		printf("  %04x  ", off);

		for (i = 0; i < 16; i++) {
			if (i < line) printf("%02x ", data[off + i]);
			else          printf("   ");
			if (i == 7)   printf(" ");
		}
		printf(" ");
		for (i = 0; i < line; i++) {
			unsigned char ch = data[off + i];
			putchar(isprint(ch) ? ch : '.');
		}
		putchar('\n');
	}
}

/*
 * Các hàm be16 và be32
 * Công dụng: Trích xuất số nguyên 16-bit và 32-bit từ buffer dữ liệu thô (mảng byte),
 * chuyển từ định dạng big-endian (network byte order) sang định dạng của host.
 */
static uint16_t be16(const __u8 *p) 
{
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t be32(const __u8 *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  |  p[3];
}

/*
 * Hàm print_payload_preview
 * Công dụng: In ra một đoạn trích ngắn (tối đa 64 bytes) của phần payload gói tin.
 * Nó tính toán độ dài thực sự đã được capture, và tỷ lệ các ký tự in ra được (printable),
 * từ đó đoán xem payload là dạng văn bản hay nhị phân/mã hóa.
 */
static void print_payload_preview(const struct packet_capture_record *r)
{
	uint16_t off = r->payload_offset, avail, preview, i, readable = 0;

	if (!r->payload_len)          { printf("  Payload: none\n"); return; }
	if (r->captured_len <= off)   {
		printf("  Payload: %u bytes, not captured\n", r->payload_len);
		return;
	}
	avail   = r->captured_len - off;
	preview = avail > 64 ? 64 : avail;

	printf("  Length: %u bytes\n  Captured: %u/%u bytes\n",
	       r->payload_len, avail, r->payload_len);

	for (i = 0; i < preview; i++) {
		unsigned char ch = r->data[off + i];
		if (isprint(ch) || ch == '\r' || ch == '\n' || ch == '\t')
			readable++;
	}
	printf("  Preview: ");
	if (preview && readable * 100 / preview >= 70) {
		for (i = 0; i < preview; i++) {
			unsigned char ch = r->data[off + i];
			if      (ch == '\r') printf("\\r");
			else if (ch == '\n') printf("\\n");
			else if (ch == '\t') printf("\\t");
			else                 putchar(isprint(ch) ? ch : '.');
		}
		putchar('\n');
	} else {
		printf("binary/encrypted/non-printable\n");
	}
}

/*
 * Hàm icmp_type_name
 * Công dụng: Trả về tên hiển thị (như "Echo Request", "Echo Reply") 
 * dựa trên mã type của gói tin ICMP.
 */
static const char *icmp_type_name(uint8_t t)
{
	switch (t) {
	case 0:  return "Echo Reply";
	case 3:  return "Destination Unreachable";
	case 5:  return "Redirect";
	case 8:  return "Echo Request";
	case 11: return "Time Exceeded";
	case 12: return "Parameter Problem";
	default: return "Unknown";
	}
}

/*
 * Hàm print_tcp_flags
 * Công dụng: Phân tích 8 bit flag của TCP header (FIN, SYN, RST, ACK...)
 * và in ra các flag đang được bật, kèm theo dòng giải thích ý nghĩa.
 * VD: (SYN) -> "Meaning: connection open request".
 */
static void print_tcp_flags(__u8 flags)
{
	static const struct { __u8 bit; const char *name; } bits[] = {
		{0x01,"FIN"},{0x02,"SYN"},{0x04,"RST"},{0x08,"PSH"},
		{0x10,"ACK"},{0x20,"URG"},{0x40,"ECE"},{0x80,"CWR"},
	};
	unsigned i;
	printf("  Flags: 0x%02x", flags);
	for (i = 0; i < 8; i++)
		if (flags & bits[i].bit) printf(" %s", bits[i].name);
	putchar('\n');

	if      ((flags & 0x02) && !(flags & 0x10)) printf("  Meaning: connection open request (SYN)\n");
	else if ((flags & 0x02) &&  (flags & 0x10)) printf("  Meaning: connection open response (SYN/ACK)\n");
	else if ((flags & 0x10) && !(flags & 0x08)) printf("  Meaning: acknowledgement\n");
	else if  (flags & 0x08)                     printf("  Meaning: data push\n");
	else if  (flags & 0x04)                     printf("  Meaning: connection reset\n");
	else if  (flags & 0x01)                     printf("  Meaning: connection close\n");
}

/*
 * Hàm print_transport
 * Công dụng: Dựa trên thông tin giao thức (TCP, UDP, ICMP), hàm này parse
 * transport header và in ra các thông số cụ thể: cổng nguồn/đích,
 * mã lỗi, hoặc các flag (nếu là TCP).
 */
static void print_transport(const struct packet_capture_record *r,
			    uint8_t proto, uint16_t ip_hlen)
{
	const __u8 *data = r->data;
	uint16_t cap = r->captured_len;

	printf("\nTransport\n");

	if (proto == PACKET_CAPTURE_PROTO_TCP) {
		if (cap < ip_hlen + 20) { printf("  TCP: not enough captured bytes\n"); return; }
		uint16_t tcp_hlen = (data[ip_hlen + 12] >> 4) * 4;
		printf("  TCP %u -> %u\n", be16(data + ip_hlen), be16(data + ip_hlen + 2));
		printf("  Seq: %u\n",  be32(data + ip_hlen + 4));
		printf("  Ack: %u\n",  be32(data + ip_hlen + 8));
		printf("  Header: %u bytes\n", tcp_hlen);
		print_tcp_flags(data[ip_hlen + 13]);
		return;
	}
	if (proto == PACKET_CAPTURE_PROTO_UDP) {
		if (cap < ip_hlen + 8) { printf("  UDP: not enough captured bytes\n"); return; }
		printf("  UDP %u -> %u\n", be16(data + ip_hlen), be16(data + ip_hlen + 2));
		printf("  Length: %u bytes\n", be16(data + ip_hlen + 4));
		return;
	}
	if (proto == PACKET_CAPTURE_PROTO_ICMP) {
		if (cap < ip_hlen + 4) { printf("  ICMP: not enough captured bytes\n"); return; }
		uint8_t type = data[ip_hlen], code = data[ip_hlen + 1];
		printf("  ICMP: %s\n", icmp_type_name(type));
		printf("  Type/Code: %u/%u\n", type, code);
		if ((type == 0 || type == 8) && cap >= ip_hlen + 8) {
			printf("  Identifier: %u\n", be16(data + ip_hlen + 4));
			printf("  Sequence: %u\n",   be16(data + ip_hlen + 6));
		}
	}
}

/*
 * Hàm print_record_detail
 * Công dụng: Hàm chính để in chi tiết MỘT gói tin cụ thể khi người dùng dùng
 * lệnh `capturectl detail <seq>`. Gọi lần lượt các hàm in network layer,
 * transport layer, payload preview và hexdump.
 */
static void print_record_detail(const struct packet_capture_record *r)
{
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
	bool has_port = r->protocol == PACKET_CAPTURE_PROTO_TCP ||
	                r->protocol == PACKET_CAPTURE_PROTO_UDP;
	const __u8 *data = r->data;
	uint16_t cap = r->captured_len;

	ip_str(r->src_addr, src, sizeof(src));
	ip_str(r->dst_addr, dst, sizeof(dst));

	printf("SEQ %llu\n", (unsigned long long)r->seq);
	printf("  Direction: %s\n", dir_str(r->direction));
	printf("  Protocol: %s\n",  proto_str(r->protocol));
	if (has_port)
		printf("  Flow: %s:%u -> %s:%u\n",
		       src, ntohs(r->src_port), dst, ntohs(r->dst_port));
	else
		printf("  Flow: %s -> %s\n", src, dst);
	printf("  IP length: %u bytes\n", r->ip_total_len);

	/* decode network layer */
	if (cap < 20) {
		printf("  Not enough captured bytes to decode IPv4 header\n");
	} else {
		uint8_t  ver      = data[0] >> 4;
		uint16_t ip_hlen  = (uint16_t)((data[0] & 0x0f) * 4);
		uint16_t frag     = be16(data + 6);
		uint8_t  proto    = data[9];

		printf("\nNetwork\n");
		if (frag & 0x3fff)
			printf("  Fragment: flags=0x%x offset=%u\n",
			       (frag >> 13) & 0x7, frag & 0x1fff);

		if (ver == 4 && ip_hlen >= 20 && cap >= ip_hlen)
			print_transport(r, proto, ip_hlen);
		else
			printf("  Cannot decode transport header\n");
	}

	printf("\nPayload\n");
	print_payload_preview(r);

	printf("\nHex / ASCII Snapshot\n");
	if (cap)
		print_hex_ascii(data, cap);
	else
		printf("(no bytes captured)\n");
}

/* ---------- ioctl wrappers ---------- */

/*
 * Hàm do_show_status
 * Công dụng: Gọi ioctl lấy cấu hình filter hiện tại từ kernel module
 * và hiển thị ra màn hình dạng dễ đọc.
 */
static int do_show_status(int fd)
{
	struct packet_capture_filter f;
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];

	if (ioctl(fd, PACKET_CAPTURE_IOCTL_GET_FILTER, &f) < 0) {
		fprintf(stderr, "ioctl GET_FILTER: %s\n", strerror(errno));
		return 1;
	}
	if (!f.enabled) { printf("packet_capture: disabled\n"); return 0; }

	ip_or_any(f.src_addr, src, sizeof(src));
	ip_or_any(f.dst_addr, dst, sizeof(dst));
	printf("packet_capture: enabled protocol=%s src=%s dst=%s direction=%s\n",
	       proto_str(f.protocol), src, dst, dir_str(f.direction));
	return 0;
}

/*
 * Hàm do_show_stats
 * Công dụng: Lấy thông số thống kê từ kernel (số gói đã thấy, đã match filter,
 * số gói bị ghi đè do vòng đệm đầy...) và in ra.
 */
static int do_show_stats(int fd)
{
	struct packet_capture_stats s;

	if (ioctl(fd, PACKET_CAPTURE_IOCTL_GET_STATS, &s) < 0) {
		fprintf(stderr, "ioctl GET_STATS: %s\n", strerror(errno));
		return 1;
	}
	printf("packets_seen:        %llu\n", (unsigned long long)s.packets_seen);
	printf("packets_matched:     %llu\n", (unsigned long long)s.packets_matched);
	printf("records_stored:      %llu\n", (unsigned long long)s.records_stored);
	printf("overwritten_records: %llu\n", (unsigned long long)s.overwritten_records);
	printf("dropped_records:     %llu\n", (unsigned long long)s.dropped_records);
	printf("newest_seq:          %llu\n", (unsigned long long)s.newest_seq);
	printf("buffer_count:        %u\n",   s.buffer_count);
	printf("buffer_size:         %u\n",   s.buffer_size);
	return 0;
}

/*
 * Hàm do_clear
 * Công dụng: Xóa bộ đệm vòng (ring buffer) trong kernel và reset các thống kê.
 */
static int do_clear(int fd)
{
	if (ioctl(fd, PACKET_CAPTURE_IOCTL_CLEAR) < 0) {
		fprintf(stderr, "ioctl CLEAR: %s\n", strerror(errno));
		return 1;
	}
	printf("packet_capture: records cleared\n");
	return 0;
}

/*
 * Hàm do_set_filter
 * Công dụng: Thiết lập bộ lọc gói tin mới cho kernel.
 * Tham số lệnh: set <proto> <src-ip|any> <direction> [--dst-ip <ip|any>]
 * Nó sẽ parse các chuỗi văn bản thành struct packet_capture_filter
 * và gửi lệnh ioctl PACKET_CAPTURE_IOCTL_SET_FILTER.
 */
static int do_set_filter(int fd, int argc, char **argv)
{
	struct packet_capture_filter f = { 0 };
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
	int i;

	if (argc < 5) return -1;

	f.enabled = 1;

	if (parse_protocol(argv[2], &f.protocol) < 0) {
		fprintf(stderr, "invalid protocol: %s\n", argv[2]); return 1;
	}
	if (parse_ip(argv[3], &f.src_addr) < 0) {
		fprintf(stderr, "invalid src IP: %s\n", argv[3]); return 1;
	}
	if (parse_direction(argv[4], &f.direction) < 0) {
		fprintf(stderr, "invalid direction: %s\n", argv[4]); return 1;
	}

	/* optional flags */
	for (i = 5; i < argc; i++) {
		if (!strcmp(argv[i], "--dst-ip") && i + 1 < argc) {
			if (parse_ip(argv[++i], &f.dst_addr) < 0) {
				fprintf(stderr, "invalid dst IP: %s\n", argv[i]);
				return 1;
			}
		} else {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return -1;
		}
	}

	if (ioctl(fd, PACKET_CAPTURE_IOCTL_SET_FILTER, &f) < 0) {
		fprintf(stderr, "ioctl SET_FILTER: %s\n", strerror(errno));
		return 1;
	}

	ip_or_any(f.src_addr, src, sizeof(src));
	ip_or_any(f.dst_addr, dst, sizeof(dst));
	printf("packet_capture: filter set protocol=%s src=%s dst=%s direction=%s\n",
	       argv[2], src, dst, argv[4]);
	return 0;
}

/* ---------- read / stream helpers ---------- */

/*
 * Hàm read_batch
 * Công dụng: Đọc một đợt các gói tin từ kernel. Mỗi lần đọc tối đa 'limit' gói,
 * bắt đầu từ sau gói tin có số SEQ = *since.
 * Gửi ioctl PACKET_CAPTURE_IOCTL_READ, sau đó lặp qua kết quả để in ra,
 * và cập nhật lại con trỏ *since bằng SEQ của gói cuối cùng đọc được.
 */
static int read_batch(int fd, uint64_t *since, uint32_t limit, uint32_t *printed)
{
	struct packet_capture_read_request *req;
	uint32_t i;

	req = calloc(1, sizeof(*req));
	if (!req) { fprintf(stderr, "calloc failed\n"); return 1; }

	req->since_seq = *since;
	req->limit     = limit;

	if (ioctl(fd, PACKET_CAPTURE_IOCTL_READ, req) < 0) {
		fprintf(stderr, "ioctl READ: %s\n", strerror(errno));
		free(req); return 1;
	}

	for (i = 0; i < req->count; i++) {
		print_record(&req->records[i]);
		*since = req->records[i].seq;
		(*printed)++;
	}
	free(req);
	return 0;
}

/*
 * Hàm do_read
 * Công dụng: Xử lý lệnh `capturectl read [--since <seq>] [--limit <n>]`.
 * Nó liên tục gọi hàm read_batch cho tới khi đọc đủ giới hạn hoặc không còn 
 * gói nào mới để đọc (kernel trả về 0 gói).
 */
static int do_read(int fd, int argc, char **argv)
{
	uint64_t since = 0;
	uint32_t limit = PACKET_CAPTURE_MAX_READ;
	uint32_t printed = 0;
	int i;

	for (i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--since") && i + 1 < argc) {
			if (parse_u64(argv[++i], &since) < 0) return -1;
		} else if (!strcmp(argv[i], "--limit") && i + 1 < argc) {
			if (parse_u32_range(argv[++i], 1, 100000, &limit) < 0) return -1;
		} else return -1;
	}

	print_record_header();
	while (printed < limit) {
		uint32_t before = printed;
		uint32_t batch  = limit - printed;
		if (batch > PACKET_CAPTURE_MAX_READ) batch = PACKET_CAPTURE_MAX_READ;
		if (read_batch(fd, &since, batch, &printed) != 0) return 1;
		if (printed == before) break;
	}
	return 0;
}

/*
 * Hàm do_stream
 * Công dụng: Xử lý lệnh `capturectl stream`. Khác với `read` (đọc xong rồi thoát),
 * `stream` sẽ vào một vòng lặp vô hạn, định kỳ thức dậy sau mỗi `interval_ms` 
 * (mặc định 100ms) để đọc và in các gói tin mới bắt được ra màn hình (như lệnh tail -f).
 * Thoát ra khi nhận được tín hiệu SIGINT/SIGTERM (Ctrl+C).
 */
static int do_stream(int fd, int argc, char **argv)
{
	uint64_t since       = 0;
	uint32_t interval_ms = 100;
	int i;

	for (i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--since") && i + 1 < argc) {
			if (parse_u64(argv[++i], &since) < 0) return -1;
		} else if (!strcmp(argv[i], "--interval-ms") && i + 1 < argc) {
			if (parse_u32_range(argv[++i], 50, 60000, &interval_ms) < 0) return -1;
		} else return -1;
	}

	signal(SIGINT,  handle_signal);
	signal(SIGTERM, handle_signal);

	print_record_header();
	fflush(stdout);

	while (!stop_requested) {
		uint32_t printed;
		do {
			printed = 0;
			if (read_batch(fd, &since, PACKET_CAPTURE_MAX_READ, &printed) != 0)
				return 1;
			fflush(stdout);
		} while (printed == PACKET_CAPTURE_MAX_READ && !stop_requested);
		usleep((useconds_t)interval_ms * 1000U);
	}
	return 0;
}

/*
 * Hàm do_detail
 * Công dụng: Lấy thông tin đầy đủ, chi tiết của một gói tin dựa vào số thứ tự
 * (SEQ). Hàm thiết lập lệnh đọc với since = seq - 1, sau đó duyệt trong số
 * kết quả trả về để tìm gói tin có số SEQ chính xác và in bằng print_record_detail.
 */
static int do_detail(int fd, int argc, char **argv)
{
	struct packet_capture_read_request *req;
	uint64_t seq;
	uint32_t i;
	int found = 0;

	if (argc != 3 || parse_u64(argv[2], &seq) < 0 || seq == 0)
		return -1;

	req = calloc(1, sizeof(*req));
	if (!req) { fprintf(stderr, "calloc failed\n"); return 1; }

	req->since_seq = seq > 0 ? seq - 1 : 0;
	req->limit     = PACKET_CAPTURE_MAX_READ;

	if (ioctl(fd, PACKET_CAPTURE_IOCTL_READ, req) < 0) {
		fprintf(stderr, "ioctl READ: %s\n", strerror(errno));
		free(req); return 1;
	}

	for (i = 0; i < req->count && !found; i++) {
		if (req->records[i].seq == seq) {
			print_record_detail(&req->records[i]);
			found = 1;
		}
	}
	if (!found)
		fprintf(stderr, "packet seq %llu not found in ring buffer\n",
			(unsigned long long)seq);
	free(req);
	return found ? 0 : 1;
}

/* ---------- main ---------- */

/*
 * Hàm main
 * Công dụng: Điểm bắt đầu của ứng dụng CLI.
 * 1. Mở thiết bị character /dev/packet_capture.
 * 2. So khớp tham số command line (argv[1]) với các tính năng (status, stats, clear, v.v.).
 * 3. Chuyển hướng thực thi tới hàm do_* tương ứng.
 */
int main(int argc, char **argv)
{
	int fd, ret;

	if (argc < 2) { usage(argv[0]); return 1; }

	fd = open_device();
	if (fd < 0) return 1;

	if      (!strcmp(argv[1], "status") && argc == 2) ret = do_show_status(fd);
	else if (!strcmp(argv[1], "stats")  && argc == 2) ret = do_show_stats(fd);
	else if (!strcmp(argv[1], "clear")  && argc == 2) ret = do_clear(fd);
	else if (!strcmp(argv[1], "set"))                 ret = do_set_filter(fd, argc, argv);
	else if (!strcmp(argv[1], "read"))                ret = do_read(fd, argc, argv);
	else if (!strcmp(argv[1], "stream"))              ret = do_stream(fd, argc, argv);
	else if (!strcmp(argv[1], "detail"))              ret = do_detail(fd, argc, argv);
	else                                              ret = -1;

	if (ret < 0) { usage(argv[0]); ret = 1; }

	close(fd);
	return ret;
}
