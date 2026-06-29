// SPDX-License-Identifier: GPL-2.0
/*
 * Bai 2 - Destination IP filter
 *
 * Module mo rong bai 1: user-space cau hinh destination IP qua ioctl, kernel
 * hook chi log packet co daddr khop rule. Module van luon NF_ACCEPT.
 */

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
#include <net/ip.h>
#include <net/net_namespace.h>

#include "../include/dst_filter_ioctl.h"

static struct dst_filter_rule active_rule;
static DEFINE_SPINLOCK(rule_lock);
static struct nf_hook_ops filter_ops;

static bool rule_matches(const struct iphdr *iph)
{
	struct dst_filter_rule snapshot;
	unsigned long flags;

	spin_lock_irqsave(&rule_lock, flags);
	snapshot = active_rule;
	spin_unlock_irqrestore(&rule_lock, flags);

	if (!snapshot.enabled)
		return false;

	return snapshot.dst_addr == iph->daddr;
}

/*
 * log_tcp_syn() chi hien thi packet khoi tao ket noi TCP: SYN set, ACK clear.
 */
static void log_tcp_syn(const struct sk_buff *skb, const struct iphdr *iph)
{
	struct tcphdr tcph_buf;
	const struct tcphdr *tcph;
	unsigned int offset = skb_network_offset(skb) + iph->ihl * 4;

	tcph = skb_header_pointer(skb, offset, sizeof(tcph_buf), &tcph_buf);
	if (!tcph)
		return;

	if (tcph->syn && !tcph->ack)
		pr_info("TCP connection initiated from %pI4:%u\n",
			&iph->saddr, ntohs(tcph->source));
}

static unsigned int filter_hook(void *priv, struct sk_buff *skb,
				const struct nf_hook_state *state)
{
	const struct iphdr *iph;

	(void)priv;
	(void)state;

	if (!skb)
		return NF_ACCEPT;

	iph = ip_hdr(skb);
	if (!iph || iph->version != 4 || iph->ihl < 5)
		return NF_ACCEPT;

	if (ip_is_fragment(iph) || iph->protocol != IPPROTO_TCP)
		return NF_ACCEPT;

	if (rule_matches(iph))
		log_tcp_syn(skb, iph);

	return NF_ACCEPT;
}

/*
 * ioctl dung copy_from_user/copy_to_user vi con tro user-space khong an toan
 * de kernel dereference truc tiep.
 */
static long filter_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dst_filter_rule rule;
	unsigned long flags;

	(void)file;

	switch (cmd) {
	case DST_FILTER_IOCTL_SET:
		if (copy_from_user(&rule, (void __user *)arg, sizeof(rule)))
			return -EFAULT;

		spin_lock_irqsave(&rule_lock, flags);
		active_rule = rule;
		spin_unlock_irqrestore(&rule_lock, flags);
		pr_info("dst_filter: set dst=%pI4 enabled=%u\n",
			&rule.dst_addr, rule.enabled);
		return 0;

	case DST_FILTER_IOCTL_CLEAR:
		spin_lock_irqsave(&rule_lock, flags);
		memset(&active_rule, 0, sizeof(active_rule));
		spin_unlock_irqrestore(&rule_lock, flags);
		pr_info("dst_filter: cleared rule\n");
		return 0;

	case DST_FILTER_IOCTL_GET:
		spin_lock_irqsave(&rule_lock, flags);
		rule = active_rule;
		spin_unlock_irqrestore(&rule_lock, flags);

		if (copy_to_user((void __user *)arg, &rule, sizeof(rule)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations filter_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = filter_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = filter_ioctl,
#endif
};

static struct miscdevice filter_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DST_FILTER_DEVICE_NAME,
	.fops = &filter_fops,
	.mode = 0600,
};

static int __init dst_filter_init(void)
{
	int ret;

	ret = misc_register(&filter_miscdev);
	if (ret)
		return ret;

	filter_ops.hook = filter_hook;
	filter_ops.pf = PF_INET;
	filter_ops.hooknum = NF_INET_LOCAL_OUT;
	filter_ops.priority = NF_IP_PRI_FIRST;

	ret = nf_register_net_hook(&init_net, &filter_ops);
	if (ret)
		goto err_misc;

	pr_info("dst_filter: loaded, device=%s\n", DST_FILTER_DEVICE_PATH);
	return 0;

err_misc:
	misc_deregister(&filter_miscdev);
	return ret;
}

static void __exit dst_filter_exit(void)
{
	nf_unregister_net_hook(&init_net, &filter_ops);
	misc_deregister(&filter_miscdev);
	pr_info("dst_filter: unloaded\n");
}

module_init(dst_filter_init);
module_exit(dst_filter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Log outbound packets matching a destination IPv4 address");
MODULE_VERSION("0.1");
