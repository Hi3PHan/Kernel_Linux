/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shared core cho hai module rieng biet:
 * - local_fw.ko: hook NF_INET_LOCAL_OUT.
 * - forward_fw.ko: hook NF_INET_FORWARD.
 *
 * File nay duoc include vao tung translation unit rieng. Moi module co rule,
 * counters, misc device va hook rieng, nen khi load/unload van doc lap.
 */

#ifndef EDGE_FW_CORE_H
#define EDGE_FW_CORE_H

#include <linux/atomic.h>
#include <linux/icmp.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tcp.h>
#include <linux/uaccess.h>
#include <linux/udp.h>
#include <net/ip.h>
#include <net/net_namespace.h>

#include "../include/edge_fw_ioctl.h"

struct edge_packet_info {
	__u8 protocol;
	__be32 src_addr;
	__be32 dst_addr;
	__be16 src_port;
	__be16 dst_port;
};

/*
 * Rule hien tai duoc doc trong netfilter hook va ghi trong ioctl.
 * Hook co the chay trong softirq context, nen khong dung mutex. Spinlock chi
 * giu rat ngan de copy rule ra snapshot, khong goi copy_to_user/from_user ben
 * trong lock.
 */
static struct edge_fw_rule edge_current_rule;
static DEFINE_SPINLOCK(edge_rule_lock);

/*
 * Counters dung atomic64_t vi hook co the chay dong thoi tren nhieu CPU.
 */
static atomic64_t edge_packets_seen = ATOMIC64_INIT(0);
static atomic64_t edge_packets_matched = ATOMIC64_INIT(0);
static atomic64_t edge_packets_accepted = ATOMIC64_INIT(0);
static atomic64_t edge_packets_dropped = ATOMIC64_INIT(0);
static atomic64_t edge_local_packets = ATOMIC64_INIT(0);
static atomic64_t edge_forwarded_packets = ATOMIC64_INIT(0);

static struct nf_hook_ops edge_hook_ops;

static const char *edge_direction_name(__u8 direction)
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

static const char *edge_protocol_name(__u8 protocol)
{
	switch (protocol) {
	case EDGE_PROTO_ANY:
		return "any";
	case EDGE_PROTO_ICMP:
		return "icmp";
	case EDGE_PROTO_TCP:
		return "tcp";
	case EDGE_PROTO_UDP:
		return "udp";
	default:
		return "unknown";
	}
}

static const char *edge_action_name(__u8 action)
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

static int edge_validate_rule(const struct edge_fw_rule *rule)
{
	if (!rule->enabled)
		return 0;

	if (rule->direction != EDGE_DIR_LOCAL &&
	    rule->direction != EDGE_DIR_FORWARD)
		return -EINVAL;

	if (rule->direction != EDGE_FW_DIRECTION)
		return -EINVAL;

	if (rule->protocol != EDGE_PROTO_ANY &&
	    rule->protocol != EDGE_PROTO_TCP &&
	    rule->protocol != EDGE_PROTO_UDP &&
	    rule->protocol != EDGE_PROTO_ICMP)
		return -EINVAL;

	if (rule->action != EDGE_ACTION_LOG &&
	    rule->action != EDGE_ACTION_ACCEPT &&
	    rule->action != EDGE_ACTION_DROP)
		return -EINVAL;

	if (rule->protocol == EDGE_PROTO_ICMP &&
	    (rule->src_port || rule->dst_port))
		return -EINVAL;

	return 0;
}

/*
 * Gia tri IP/port trong rule bang 0 nghia la wildcard "any".
 * IP va port duoc luu o network byte order, nen so sanh truc tiep voi header.
 */
static bool edge_packet_matches_rule(const struct edge_fw_rule *rule,
				     const struct edge_packet_info *pkt)
{
	if (!rule->enabled)
		return false;

	if (rule->protocol != EDGE_PROTO_ANY && rule->protocol != pkt->protocol)
		return false;

	if (rule->src_addr && rule->src_addr != pkt->src_addr)
		return false;

	if (rule->dst_addr && rule->dst_addr != pkt->dst_addr)
		return false;

	if (rule->src_port && rule->src_port != pkt->src_port)
		return false;

	if (rule->dst_port && rule->dst_port != pkt->dst_port)
		return false;

	return true;
}

/*
 * skb_header_pointer() an toan hon doc tcp_hdr()/udp_hdr() truc tiep khi skb
 * khong linear. Offset duoc tinh tu skb->data den transport header.
 */
static bool edge_read_transport_ports(const struct sk_buff *skb,
				      const struct iphdr *iph,
				      struct edge_packet_info *pkt)
{
	unsigned int offset = skb_network_offset(skb) + iph->ihl * 4;

	if (iph->protocol == IPPROTO_TCP) {
		struct tcphdr tcph_buf;
		const struct tcphdr *tcph;

		tcph = skb_header_pointer(skb, offset, sizeof(tcph_buf),
					  &tcph_buf);
		if (!tcph)
			return false;

		pkt->src_port = tcph->source;
		pkt->dst_port = tcph->dest;
		return true;
	}

	if (iph->protocol == IPPROTO_UDP) {
		struct udphdr udph_buf;
		const struct udphdr *udph;

		udph = skb_header_pointer(skb, offset, sizeof(udph_buf),
					  &udph_buf);
		if (!udph)
			return false;

		pkt->src_port = udph->source;
		pkt->dst_port = udph->dest;
		return true;
	}

	return true;
}

/*
 * %pI4 in IPv4 address dang network byte order. ntohs() doi port tu network
 * byte order sang host byte order de log de doc.
 */
static void edge_log_packet(const char *prefix,
			    const struct edge_packet_info *pkt)
{
	if (pkt->protocol == EDGE_PROTO_TCP || pkt->protocol == EDGE_PROTO_UDP) {
		pr_info("%s: %s %s %pI4:%u -> %pI4:%u\n",
			EDGE_FW_LOG_PREFIX, prefix,
			edge_protocol_name(pkt->protocol),
			&pkt->src_addr, ntohs(pkt->src_port),
			&pkt->dst_addr, ntohs(pkt->dst_port));
		return;
	}

	pr_info("%s: %s %s %pI4 -> %pI4\n",
		EDGE_FW_LOG_PREFIX, prefix, edge_protocol_name(pkt->protocol),
		&pkt->src_addr, &pkt->dst_addr);
}

static unsigned int edge_accept_packet(void)
{
	atomic64_inc(&edge_packets_accepted);
	return NF_ACCEPT;
}

/*
 * Netfilter hook tra NF_ACCEPT de cho packet tiep tuc, hoac NF_DROP de chan.
 * local_fw.ko chi thay LOCAL_OUT; forward_fw.ko chi thay FORWARD.
 */
static unsigned int edge_nf_hook(void *priv, struct sk_buff *skb,
				 const struct nf_hook_state *state)
{
	struct edge_fw_rule rule_snapshot;
	struct edge_packet_info pkt = { 0 };
	const struct iphdr *iph;
	unsigned long flags;

	(void)priv;
	(void)state;

	if (!skb)
		return NF_ACCEPT;

	atomic64_inc(&edge_packets_seen);
	if (EDGE_FW_DIRECTION == EDGE_DIR_LOCAL)
		atomic64_inc(&edge_local_packets);
	else
		atomic64_inc(&edge_forwarded_packets);

	/*
	 * ip_hdr() lay IPv4 header tu skb. Hai module nay dang ky PF_INET nen
	 * chi xu ly IPv4. IPv6 khong nam trong pham vi lab nay.
	 */
	iph = ip_hdr(skb);
	if (!iph || iph->version != 4 || iph->ihl < 5)
		return edge_accept_packet();

	/*
	 * Fragment co the khong chua TCP/UDP header day du. V1 accept fragment
	 * de tranh doc sai port trong demo.
	 */
	if (ip_is_fragment(iph))
		return edge_accept_packet();

	pkt.protocol = iph->protocol;
	pkt.src_addr = iph->saddr;
	pkt.dst_addr = iph->daddr;

	if (pkt.protocol == EDGE_PROTO_TCP || pkt.protocol == EDGE_PROTO_UDP) {
		if (!edge_read_transport_ports(skb, iph, &pkt))
			return edge_accept_packet();
	} else if (pkt.protocol != EDGE_PROTO_ICMP) {
		return edge_accept_packet();
	}

	spin_lock_irqsave(&edge_rule_lock, flags);
	rule_snapshot = edge_current_rule;
	spin_unlock_irqrestore(&edge_rule_lock, flags);

	if (!edge_packet_matches_rule(&rule_snapshot, &pkt))
		return edge_accept_packet();

	atomic64_inc(&edge_packets_matched);

	if (rule_snapshot.action == EDGE_ACTION_LOG) {
		edge_log_packet("log", &pkt);
		return edge_accept_packet();
	}

	if (rule_snapshot.action == EDGE_ACTION_DROP) {
		edge_log_packet("drop", &pkt);
		atomic64_inc(&edge_packets_dropped);
		return NF_DROP;
	}

	edge_log_packet("accept", &pkt);
	return edge_accept_packet();
}

static void edge_fill_stats(struct edge_fw_stats *stats)
{
	stats->packets_seen = atomic64_read(&edge_packets_seen);
	stats->packets_matched = atomic64_read(&edge_packets_matched);
	stats->packets_accepted = atomic64_read(&edge_packets_accepted);
	stats->packets_dropped = atomic64_read(&edge_packets_dropped);
	stats->local_packets = atomic64_read(&edge_local_packets);
	stats->forwarded_packets = atomic64_read(&edge_forwarded_packets);
}

/*
 * ioctl la ranh gioi user-space/kernel-space. copy_from_user() va
 * copy_to_user() bat buoc dung vi kernel khong duoc dereference truc tiep
 * con tro do user-space dua vao.
 */
static long edge_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct edge_fw_rule rule;
	struct edge_fw_stats stats;
	unsigned long flags;
	int ret;

	(void)file;

	switch (cmd) {
	case EDGE_FW_IOCTL_SET_RULE:
		if (copy_from_user(&rule, (void __user *)arg, sizeof(rule)))
			return -EFAULT;

		ret = edge_validate_rule(&rule);
		if (ret)
			return ret;

		spin_lock_irqsave(&edge_rule_lock, flags);
		edge_current_rule = rule;
		spin_unlock_irqrestore(&edge_rule_lock, flags);

		pr_info("%s: set rule action=%s direction=%s protocol=%s\n",
			EDGE_FW_LOG_PREFIX, edge_action_name(rule.action),
			edge_direction_name(rule.direction),
			edge_protocol_name(rule.protocol));
		return 0;

	case EDGE_FW_IOCTL_CLEAR_RULE:
		spin_lock_irqsave(&edge_rule_lock, flags);
		memset(&edge_current_rule, 0, sizeof(edge_current_rule));
		spin_unlock_irqrestore(&edge_rule_lock, flags);
		pr_info("%s: cleared active rule\n", EDGE_FW_LOG_PREFIX);
		return 0;

	case EDGE_FW_IOCTL_GET_RULE:
		spin_lock_irqsave(&edge_rule_lock, flags);
		rule = edge_current_rule;
		spin_unlock_irqrestore(&edge_rule_lock, flags);

		if (copy_to_user((void __user *)arg, &rule, sizeof(rule)))
			return -EFAULT;
		return 0;

	case EDGE_FW_IOCTL_GET_STATS:
		edge_fill_stats(&stats);
		if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations edge_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = edge_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = edge_ioctl,
#endif
};

/*
 * miscdevice tao /dev/local_fw hoac /dev/forward_fw ma khong can tu cap
 * major/minor. mode 0600 yeu cau sudo/root khi cau hinh firewall.
 */
static struct miscdevice edge_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = EDGE_FW_DEVICE_NAME,
	.fops = &edge_fops,
	.mode = 0600,
};

static int __init edge_module_init(void)
{
	int ret;

	ret = misc_register(&edge_miscdev);
	if (ret) {
		pr_err("%s: failed to register misc device: %d\n",
		       EDGE_FW_LOG_PREFIX, ret);
		return ret;
	}

	edge_hook_ops.hook = edge_nf_hook;
	edge_hook_ops.pf = PF_INET;
	edge_hook_ops.hooknum = EDGE_FW_HOOKNUM;
	edge_hook_ops.priority = NF_IP_PRI_FIRST;

	ret = nf_register_net_hook(&init_net, &edge_hook_ops);
	if (ret) {
		pr_err("%s: failed to register netfilter hook: %d\n",
		       EDGE_FW_LOG_PREFIX, ret);
		goto err_misc;
	}

	pr_info("%s: loaded, device=%s, hook=%s\n",
		EDGE_FW_LOG_PREFIX, EDGE_FW_DEVICE_PATH, EDGE_FW_HOOK_NAME);
	return 0;

err_misc:
	misc_deregister(&edge_miscdev);
	return ret;
}

static void __exit edge_module_exit(void)
{
	/*
	 * Unregister hook truoc khi huy misc device de dam bao khong con packet
	 * callback nao chay vao code module trong luc unload.
	 */
	nf_unregister_net_hook(&init_net, &edge_hook_ops);
	misc_deregister(&edge_miscdev);
	pr_info("%s: unloaded\n", EDGE_FW_LOG_PREFIX);
}

module_init(edge_module_init);
module_exit(edge_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION(EDGE_FW_MODULE_DESCRIPTION);
MODULE_VERSION("0.1");

#endif
