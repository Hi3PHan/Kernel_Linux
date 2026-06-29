// SPDX-License-Identifier: GPL-2.0
/*
 * filterctl - CLI cau hinh dst_filter.ko qua ioctl.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/dst_filter_ioctl.h"

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s set <ipv4>\n"
		"  %s clear\n"
		"  %s status\n",
		prog, prog, prog);
}

static int open_device(void)
{
	int fd = open(DST_FILTER_DEVICE_PATH, O_RDWR);

	if (fd < 0)
		fprintf(stderr, "open %s failed: %s\n",
			DST_FILTER_DEVICE_PATH, strerror(errno));

	return fd;
}

static int set_rule(int fd, const char *ip_text)
{
	struct dst_filter_rule rule = { 0 };
	struct in_addr addr;

	if (inet_pton(AF_INET, ip_text, &addr) != 1) {
		fprintf(stderr, "invalid IPv4 address: %s\n", ip_text);
		return 1;
	}

	rule.enabled = 1;
	rule.dst_addr = addr.s_addr;

	if (ioctl(fd, DST_FILTER_IOCTL_SET, &rule) < 0) {
		fprintf(stderr, "ioctl SET failed: %s\n", strerror(errno));
		return 1;
	}

	printf("dst_filter: rule set to %s\n", ip_text);
	return 0;
}

static int clear_rule(int fd)
{
	if (ioctl(fd, DST_FILTER_IOCTL_CLEAR) < 0) {
		fprintf(stderr, "ioctl CLEAR failed: %s\n", strerror(errno));
		return 1;
	}

	printf("dst_filter: rule cleared\n");
	return 0;
}

static int show_status(int fd)
{
	struct dst_filter_rule rule;
	char ip[INET_ADDRSTRLEN];
	struct in_addr addr;

	if (ioctl(fd, DST_FILTER_IOCTL_GET, &rule) < 0) {
		fprintf(stderr, "ioctl GET failed: %s\n", strerror(errno));
		return 1;
	}

	if (!rule.enabled) {
		printf("dst_filter: disabled\n");
		return 0;
	}

	addr.s_addr = rule.dst_addr;
	if (!inet_ntop(AF_INET, &addr, ip, sizeof(ip)))
		strcpy(ip, "<invalid>");

	printf("dst_filter: enabled dst=%s\n", ip);
	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	int ret;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	fd = open_device();
	if (fd < 0)
		return 1;

	if (!strcmp(argv[1], "set") && argc == 3)
		ret = set_rule(fd, argv[2]);
	else if (!strcmp(argv[1], "clear") && argc == 2)
		ret = clear_rule(fd);
	else if (!strcmp(argv[1], "status") && argc == 2)
		ret = show_status(fd);
	else {
		usage(argv[0]);
		ret = 1;
	}

	close(fd);
	return ret;
}
