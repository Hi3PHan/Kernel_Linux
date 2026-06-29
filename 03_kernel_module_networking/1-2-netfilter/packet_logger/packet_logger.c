// SPDX-License-Identifier: GPL-2.0
/*
 * Bai 1 - Netfilter packet logger
 *
 * Module dang ky hook NF_INET_LOCAL_OUT de quan sat packet TCP SYN sinh ra tu
 * chinh may. Hook chi log va luon tra NF_ACCEPT, nen khong lam thay doi luong
 * mang cua VM.
 */

#include <linux/init.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <net/ip.h>
#include <net/net_namespace.h>

static struct nf_hook_ops logger_ops;

/*
 * log_tcp_syn() doc TCP header bang skb_header_pointer() de tranh loi khi skb
 * khong linear. Port trong TCP header o network byte order, nen in bang ntohs().
 */
static void log_tcp_syn(const struct sk_buff *skb, const struct iphdr *iph)
{
	struct tcphdr tcph_buf;
	const struct tcphdr *tcph;
	unsigned int offset = skb_network_offset(skb) + iph->ihl * 4;

	tcph = skb_header_pointer(skb, offset, sizeof(tcph_buf), &tcph_buf);
	if (!tcph)
		return;

	if (tcph->syn && !tcph->ack){
		pr_info("TCP connection initiated from %d:%u\n",
			&iph->saddr, tcph->source);
		pr_info("TCP connection initiated from %pI4:%u\n",
			&iph->saddr, tcph->source);
}
}

/*
 * Hook function duoc netfilter goi cho moi IPv4 packet tai LOCAL_OUT.
 * LOCAL_OUT chi la packet do chinh VM tao ra, khong phai packet forward.
 */
static unsigned int logger_hook(void *priv, struct sk_buff *skb,
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

	log_tcp_syn(skb, iph);
	return NF_ACCEPT;
}

static int __init packet_logger_init(void)
{
	logger_ops.hook = logger_hook;
	logger_ops.pf = PF_INET;
	logger_ops.hooknum = NF_INET_LOCAL_OUT;
	logger_ops.priority = NF_IP_PRI_FIRST;

	return nf_register_net_hook(&init_net, &logger_ops);
}

static void __exit packet_logger_exit(void)
{
	nf_unregister_net_hook(&init_net, &logger_ops);
	pr_info("packet_logger: unloaded\n");
}

module_init(packet_logger_init);
module_exit(packet_logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Log outbound TCP SYN packets with netfilter");
MODULE_VERSION("0.1");
