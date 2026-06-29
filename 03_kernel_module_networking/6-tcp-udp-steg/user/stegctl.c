// SPDX-License-Identifier: GPL-2.0
/*
 * stegctl - Công cụ điều khiển module tcp_udp_steg.ko từ user-space.
 *
 * Giao tiếp với kernel qua ioctl thông qua device /dev/tcp_udp_steg.
 * Ba lệnh:
 *   set <msg>  - Nạp chuỗi cần giấu vào kernel module.
 *   clear      - Xóa trạng thái TX/RX trong module.
 *   status     - In ra trạng thái hiện tại (đã gửi bao nhiêu bit, ...).
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../include/tcp_udp_steg_ioctl.h"

/* In hướng dẫn sử dụng ra stderr */
static void usage(const char *p)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s set <message>\n"
		"  %s clear\n"
		"  %s status\n", p, p, p);
}

int main(int argc, char **argv)
{
	int fd, ret = 0;

	if (argc < 2) { usage(argv[0]); return 1; }

	/* Mở device node do kernel module tạo ra khi insmod */
	fd = open(STEG_DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", STEG_DEVICE_PATH, strerror(errno));
		return 1;
	}

	/* ---- Lệnh: set <message> ---- */
	if (!strcmp(argv[1], "set") && argc >= 3) {
		struct steg_message m = { 0 };
		size_t used = 0;

		/*
		 * Ghép tất cả các từ argv[2..] thành một chuỗi,
		 * chèn dấu cách giữa các từ (i > 2).
		 */
		for (int i = 2; i < argc; i++) {
			size_t n = strlen(argv[i]);
			if (i > 2) m.data[used++] = ' '; /* Thêm space giữa các từ */
			if (used + n > STEG_MAX_MESSAGE) {
				fprintf(stderr, "message too long (max %d)\n", STEG_MAX_MESSAGE);
				ret = 1; goto out;
			}
			memcpy(m.data + used, argv[i], n);
			used += n;
		}
		m.len = used;

		/*
		 * Gửi struct steg_message xuống kernel qua ioctl.
		 * Kernel sẽ copy m vào tx_msg[] và bắt đầu nhúng bit.
		 */
		if (ioctl(fd, STEG_IOCTL_SET_MESSAGE, &m) < 0) {
			fprintf(stderr, "SET_MESSAGE: %s\n", strerror(errno)); ret = 1;
		} else {
			printf("loaded %u bytes (%u bits)\n", m.len, m.len * 8);
		}

	/* ---- Lệnh: clear ---- */
	} else if (!strcmp(argv[1], "clear") && argc == 2) {
		/*
		 * Yêu cầu kernel xóa toàn bộ trạng thái TX lẫn RX:
		 * tx_msg, tx_pos, rx_byte, rx_bits, rx_last đều về 0.
		 */
		if (ioctl(fd, STEG_IOCTL_CLEAR) < 0) {
			fprintf(stderr, "CLEAR: %s\n", strerror(errno)); ret = 1;
		} else {
			printf("cleared\n");
		}

	/* ---- Lệnh: status ---- */
	} else if (!strcmp(argv[1], "status") && argc == 2) {
		struct steg_status s;

		/*
		 * Kernel điền struct steg_status rồi copy_to_user().
		 * Ta in ra các trường quan trọng:
		 *   enabled    - còn bit đang chờ gửi không?
		 *   msg bytes  - độ dài chuỗi cần giấu (byte)
		 *   sent bits  - đã nhúng bao nhiêu bit / tổng
		 *   last char  - ký tự cuối cùng đã ghép được ở phía RX
		 */
		if (ioctl(fd, STEG_IOCTL_GET_STATUS, &s) < 0) {
			fprintf(stderr, "GET_STATUS: %s\n", strerror(errno)); ret = 1;
		} else {
			printf("enabled:   %s\n",    s.enabled ? "yes" : "no");
			printf("msg bytes: %u\n",    s.message_len);
			printf("sent bits: %u/%u\n", s.tx_bit_pos, s.tx_total_bits);
			printf("last char: 0x%02x\n", s.last_char);
		}

	} else {
		usage(argv[0]); ret = 1;
	}

out:
	close(fd);
	return ret;
}
