// SPDX-License-Identifier: GPL-2.0
/*
 * fwctl - user-space CLI cau hinh local_fw.ko va forward_fw.ko.
 *
 * Hai kernel module duoc load rieng:
 * - /dev/local_fw cau hinh hook NF_INET_LOCAL_OUT.
 * - /dev/forward_fw cau hinh hook NF_INET_FORWARD.
 *
 * CLI nay chi parse tham so va goi ioctl. Quyet dinh NF_ACCEPT/NF_DROP nam
 * trong kernel module.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/edge_fw_ioctl.h"

struct fw_device {
	const char *label;
	const char *path;
	__u8 direction;
};

static const struct fw_device fw_devices[] = {
	{ "local", EDGE_FW_LOCAL_DEVICE_PATH, EDGE_DIR_LOCAL },
	{ "forward", EDGE_FW_FORWARD_DEVICE_PATH, EDGE_DIR_FORWARD },
};

static void print_usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s status [local|forward|both]\n"
		"  %s stats [local|forward|both]\n"
		"  %s clear [local|forward|both]\n"
		"  %s <log|accept|drop> <local|forward|both> <any|tcp|udp|icmp> "
		"<src-ip|any> <dst-ip|any> <src-port|any> <dst-port|any>\n\n"
		"Examples:\n"
		"  sudo insmod local_fw.ko\n"
		"  sudo insmod forward_fw.ko\n"
		"  sudo %s drop forward icmp any 8.8.8.8 any any\n"
		"  sudo %s drop forward tcp any 93.184.216.34 any 80\n"
		"  sudo %s log local udp any 127.0.0.1 any 60001\n"
		"  sudo %s clear both\n",
		prog, prog, prog, prog, prog, prog, prog, prog);
}

static const char *direction_to_string(__u8 direction)
{
	switch (direction) {
	case EDGE_DIR_LOCAL:
		return "local";
	case EDGE_DIR_FORWARD:
		return "forward";
	case EDGE_DIR_BOTH:
		return "both";
	default:
		return "unknown";
	}
}

static const char *protocol_to_string(__u8 protocol)
{
	switch (protocol) {
	case EDGE_PROTO_ANY:
		return "any";
	case EDGE_PROTO_TCP:
		return "tcp";
	case EDGE_PROTO_UDP:
		return "udp";
	case EDGE_PROTO_ICMP:
		return "icmp";
	default:
		return "unknown";
	}
}

static const char *action_to_string(__u8 action)
{
	switch (action) {
	case EDGE_ACTION_LOG:
		return "log";
	case EDGE_ACTION_ACCEPT:
		return "accept";
	case EDGE_ACTION_DROP:
		return "drop";
	default:
		return "unknown";
	}
}

static int parse_action(const char *text, __u8 *action)
{
	if (!strcmp(text, "log"))
		*action = EDGE_ACTION_LOG;
	else if (!strcmp(text, "accept"))
		*action = EDGE_ACTION_ACCEPT;
	else if (!strcmp(text, "drop"))
		*action = EDGE_ACTION_DROP;
	else
		return -1;

	return 0;
}

static int parse_direction(const char *text, __u8 *direction)
{
	if (!strcmp(text, "local"))
		*direction = EDGE_DIR_LOCAL;
	else if (!strcmp(text, "forward"))
		*direction = EDGE_DIR_FORWARD;
	else if (!strcmp(text, "both"))
		*direction = EDGE_DIR_BOTH;
	else
		return -1;

	return 0;
}

static int parse_protocol(const char *text, __u8 *protocol)
{
	if (!strcmp(text, "any"))
		*protocol = EDGE_PROTO_ANY;
	else if (!strcmp(text, "tcp"))
		*protocol = EDGE_PROTO_TCP;
	else if (!strcmp(text, "udp"))
		*protocol = EDGE_PROTO_UDP;
	else if (!strcmp(text, "icmp"))
		*protocol = EDGE_PROTO_ICMP;
	else
		return -1;

	return 0;
}

/*
 * inet_pton() doi chuoi IPv4 sang network byte order. Gia tri "any" duoc
 * ma hoa bang 0 dung voi convention cua kernel module.
 */
static int parse_ip(const char *text, __be32 *addr)
{
	struct in_addr in;

	if (!strcmp(text, "any")) {
		*addr = 0;
		return 0;
	}

	if (inet_pton(AF_INET, text, &in) != 1)
		return -1;

	*addr = in.s_addr;
	return 0;
}

/*
 * htons() chuyen port sang network byte order de kernel so sanh truc tiep voi
 * port trong TCP/UDP header. Gia tri "any" duoc ma hoa bang 0.
 */
static int parse_port(const char *text, __be16 *port)
{
	char *end;
	long value;

	if (!strcmp(text, "any")) {
		*port = 0;
		return 0;
	}

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || *end != '\0' || value < 1 || value > 65535)
		return -1;

	*port = htons((uint16_t)value);
	return 0;
}

static bool device_selected(__u8 requested, __u8 device_direction)
{
	return requested == EDGE_DIR_BOTH || requested == device_direction;
}

static int open_device(const struct fw_device *dev, bool quiet)
{
	int fd = open(dev->path, O_RDWR);

	if (fd < 0 && !quiet)
		fprintf(stderr, "%s: open %s failed: %s\n",
			dev->label, dev->path, strerror(errno));

	return fd;
}

static void print_ip(__be32 addr)
{
	char buffer[INET_ADDRSTRLEN];
	struct in_addr in = { .s_addr = addr };

	if (!addr) {
		printf("any");
		return;
	}

	if (!inet_ntop(AF_INET, &in, buffer, sizeof(buffer))) {
		printf("<invalid>");
		return;
	}

	printf("%s", buffer);
}

static void print_port(__be16 port)
{
	if (!port)
		printf("any");
	else
		printf("%u", ntohs(port));
}

static int show_status_one(const struct fw_device *dev)
{
	struct edge_fw_rule rule;
	int fd;

	fd = open_device(dev, true);
	if (fd < 0) {
		printf("[%s] module not loaded or device missing (%s)\n",
		       dev->label, dev->path);
		return 1;
	}

	if (ioctl(fd, EDGE_FW_IOCTL_GET_RULE, &rule) < 0) {
		fprintf(stderr, "[%s] ioctl GET_RULE failed: %s\n",
			dev->label, strerror(errno));
		close(fd);
		return 1;
	}

	if (!rule.enabled) {
		printf("[%s] no active rule, default action is accept\n", dev->label);
		close(fd);
		return 0;
	}

	printf("[%s] active rule:\n", dev->label);
	printf("  action:    %s\n", action_to_string(rule.action));
	printf("  direction: %s\n", direction_to_string(rule.direction));
	printf("  protocol:  %s\n", protocol_to_string(rule.protocol));
	printf("  src:       ");
	print_ip(rule.src_addr);
	printf(":");
	print_port(rule.src_port);
	printf("\n");
	printf("  dst:       ");
	print_ip(rule.dst_addr);
	printf(":");
	print_port(rule.dst_port);
	printf("\n");

	close(fd);
	return 0;
}

static int show_stats_one(const struct fw_device *dev)
{
	struct edge_fw_stats stats;
	int fd;

	fd = open_device(dev, true);
	if (fd < 0) {
		printf("[%s] module not loaded or device missing (%s)\n",
		       dev->label, dev->path);
		return 1;
	}

	if (ioctl(fd, EDGE_FW_IOCTL_GET_STATS, &stats) < 0) {
		fprintf(stderr, "[%s] ioctl GET_STATS failed: %s\n",
			dev->label, strerror(errno));
		close(fd);
		return 1;
	}

	printf("[%s] stats:\n", dev->label);
	printf("  packets_seen:      %llu\n",
	       (unsigned long long)stats.packets_seen);
	printf("  packets_matched:   %llu\n",
	       (unsigned long long)stats.packets_matched);
	printf("  packets_accepted:  %llu\n",
	       (unsigned long long)stats.packets_accepted);
	printf("  packets_dropped:   %llu\n",
	       (unsigned long long)stats.packets_dropped);
	printf("  local_packets:     %llu\n",
	       (unsigned long long)stats.local_packets);
	printf("  forwarded_packets: %llu\n",
	       (unsigned long long)stats.forwarded_packets);

	close(fd);
	return 0;
}

static int clear_one(const struct fw_device *dev)
{
	int fd;

	fd = open_device(dev, false);
	if (fd < 0)
		return 1;

	if (ioctl(fd, EDGE_FW_IOCTL_CLEAR_RULE) < 0) {
		fprintf(stderr, "[%s] ioctl CLEAR_RULE failed: %s\n",
			dev->label, strerror(errno));
		close(fd);
		return 1;
	}

	printf("[%s] rule cleared\n", dev->label);
	close(fd);
	return 0;
}

static int parse_rule(int argc, char **argv, struct edge_fw_rule *rule,
		      __u8 *target)
{
	memset(rule, 0, sizeof(*rule));
	rule->enabled = 1;

	if (argc != 8)
		return -1;

	if (parse_action(argv[1], &rule->action) < 0 ||
	    parse_direction(argv[2], target) < 0 ||
	    parse_protocol(argv[3], &rule->protocol) < 0 ||
	    parse_ip(argv[4], &rule->src_addr) < 0 ||
	    parse_ip(argv[5], &rule->dst_addr) < 0 ||
	    parse_port(argv[6], &rule->src_port) < 0 ||
	    parse_port(argv[7], &rule->dst_port) < 0)
		return -1;

	if (rule->protocol == EDGE_PROTO_ICMP &&
	    (rule->src_port || rule->dst_port)) {
		fprintf(stderr, "icmp rule must use src-port=any and dst-port=any\n");
		return -1;
	}

	return 0;
}

static int set_one(const struct fw_device *dev, struct edge_fw_rule rule)
{
	int fd;

	fd = open_device(dev, false);
	if (fd < 0)
		return 1;

	/*
	 * Module local chi chap nhan EDGE_DIR_LOCAL; module forward chi chap
	 * nhan EDGE_DIR_FORWARD. Khi user nhap "both", CLI tach thanh hai rule.
	 */
	rule.direction = dev->direction;

	if (ioctl(fd, EDGE_FW_IOCTL_SET_RULE, &rule) < 0) {
		fprintf(stderr, "[%s] ioctl SET_RULE failed: %s\n",
			dev->label, strerror(errno));
		close(fd);
		return 1;
	}

	printf("[%s] rule installed\n", dev->label);
	close(fd);
	return 0;
}

static int run_for_target(__u8 target,
			  int (*fn)(const struct fw_device *dev))
{
	size_t i;
	int ret = 0;

	for (i = 0; i < sizeof(fw_devices) / sizeof(fw_devices[0]); i++) {
		if (!device_selected(target, fw_devices[i].direction))
			continue;
		ret |= fn(&fw_devices[i]);
	}

	return ret;
}

static int set_for_target(__u8 target, const struct edge_fw_rule *rule)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < sizeof(fw_devices) / sizeof(fw_devices[0]); i++) {
		if (!device_selected(target, fw_devices[i].direction))
			continue;
		ret |= set_one(&fw_devices[i], *rule);
	}

	return ret;
}

static int parse_optional_target(int argc, char **argv, __u8 *target)
{
	*target = EDGE_DIR_BOTH;

	if (argc == 2)
		return 0;

	if (argc == 3)
		return parse_direction(argv[2], target);

	return -1;
}

int main(int argc, char **argv)
{
	struct edge_fw_rule rule;
	__u8 target;

	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "status")) {
		if (parse_optional_target(argc, argv, &target) < 0) {
			print_usage(argv[0]);
			return 1;
		}
		return run_for_target(target, show_status_one);
	}

	if (!strcmp(argv[1], "stats")) {
		if (parse_optional_target(argc, argv, &target) < 0) {
			print_usage(argv[0]);
			return 1;
		}
		return run_for_target(target, show_stats_one);
	}

	if (!strcmp(argv[1], "clear")) {
		if (parse_optional_target(argc, argv, &target) < 0) {
			print_usage(argv[0]);
			return 1;
		}
		return run_for_target(target, clear_one);
	}

	if (parse_rule(argc, argv, &rule, &target) < 0) {
		print_usage(argv[0]);
		return 1;
	}

	return set_for_target(target, &rule);
}
