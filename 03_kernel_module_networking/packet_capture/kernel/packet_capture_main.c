// SPDX-License-Identifier: GPL-2.0
/*
 * packet_capture.ko
 *
 * Small IPv4 ICMP/TCP/UDP capture module for the Qt dashboard. The netfilter hooks
 * only observe packets and always return NF_ACCEPT.
 */

#include <linux/atomic.h>
#include <linux/icmp.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tcp.h>
#include <linux/uaccess.h>
#include <linux/udp.h>
#include <net/ip.h>
#include <net/net_namespace.h>

#include "../include/packet_capture_ioctl.h"

struct capture_packet_info {
	__u8 direction;
	__u8 protocol;
	__u16 ip_total_len;
	__be32 src_addr;
	__be32 dst_addr;
	__be16 src_port;
	__be16 dst_port;
	__u16 payload_offset;
	__u16 payload_len;
};

static struct packet_capture_filter active_filter;
static struct packet_capture_record capture_ring[PACKET_CAPTURE_RING_SIZE];
static unsigned int capture_head;
static unsigned int capture_count;
static __u64 capture_next_seq = 1;
static DEFINE_SPINLOCK(capture_lock);

static atomic64_t packets_seen = ATOMIC64_INIT(0);
static atomic64_t packets_matched = ATOMIC64_INIT(0);
static atomic64_t records_stored = ATOMIC64_INIT(0);
static atomic64_t overwritten_records = ATOMIC64_INIT(0);
static atomic64_t dropped_records = ATOMIC64_INIT(0);

static struct nf_hook_ops capture_hooks[2];

/*
 * Hàm direction_name
 * Công dụng: Chuyển đổi mã hướng bắt gói (IN, OUT, BOTH) thành chuỗi tương ứng.
 * Phục vụ cho việc in log trong kernel.
 */
static const char *direction_name(__u8 direction)
{
	switch (direction) {
	case PACKET_CAPTURE_DIR_IN:
		return "in";
	case PACKET_CAPTURE_DIR_OUT:
		return "out";
	case PACKET_CAPTURE_DIR_BOTH:
		return "both";
	default:
		return "unknown";
	}
}

/*
 * Hàm protocol_name
 * Công dụng: Chuyển đổi mã giao thức (ANY, ICMP, TCP, UDP) thành chuỗi.
 * Dùng để in log thông tin dễ đọc ra kernel buffer (dmesg).
 */
static const char *protocol_name(__u8 protocol)
{
	switch (protocol) {
	case PACKET_CAPTURE_PROTO_ANY:
		return "any";
	case PACKET_CAPTURE_PROTO_ICMP:
		return "icmp";
	case PACKET_CAPTURE_PROTO_TCP:
		return "tcp";
	case PACKET_CAPTURE_PROTO_UDP:
		return "udp";
	default:
		return "unknown";
	}
}

/*
 * Hàm validate_filter
 * Công dụng: Kiểm tra tính hợp lệ của cấu hình bộ lọc từ user-space gửi xuống.
 * Trả về 0 nếu hợp lệ, hoặc mã lỗi (-EINVAL) nếu các thông số không đúng chuẩn.
 */
static int validate_filter(const struct packet_capture_filter *filter)
{
	if (!filter->enabled)
		return 0;

	if (filter->protocol != PACKET_CAPTURE_PROTO_ANY &&
	    filter->protocol != PACKET_CAPTURE_PROTO_ICMP &&
	    filter->protocol != PACKET_CAPTURE_PROTO_TCP &&
	    filter->protocol != PACKET_CAPTURE_PROTO_UDP)
		return -EINVAL;

	if (filter->direction != PACKET_CAPTURE_DIR_IN &&
	    filter->direction != PACKET_CAPTURE_DIR_OUT &&
	    filter->direction != PACKET_CAPTURE_DIR_BOTH)
		return -EINVAL;

	return 0;
}

/*
 * Hàm filter_matches_packet
 * Công dụng: So khớp thông tin gói tin hiện tại với cấu hình bộ lọc (active_filter).
 * Trả về true nếu gói tin thỏa mãn tất cả các điều kiện lọc (giao thức, hướng, IP),
 * ngược lại trả về false.
 */
static bool filter_matches_packet(const struct packet_capture_filter *filter,
				  const struct capture_packet_info *pkt)
{
	if (!filter->enabled)
		return false;

	if (filter->protocol != PACKET_CAPTURE_PROTO_ANY &&
	    filter->protocol != pkt->protocol)
		return false;

	if (filter->direction != PACKET_CAPTURE_DIR_BOTH &&
	    filter->direction != pkt->direction)
		return false;

	if (filter->src_addr && filter->src_addr != pkt->src_addr)
		return false;

	if (filter->dst_addr && filter->dst_addr != pkt->dst_addr)
		return false;

	return true;
}

/*
 * Hàm set_payload_bounds
 * Công dụng: Tính toán độ dài phần payload (dữ liệu phần mềm) của gói tin dựa vào
 * độ dài IP header và Transport header (TCP/UDP). Trả về true nếu gói tin
 * đủ lớn chứa payload, ngược lại trả về false.
 */
static bool set_payload_bounds(const struct iphdr *iph,
			       struct capture_packet_info *pkt,
			       unsigned int transport_header_len)
{
	unsigned int ip_header_len = iph->ihl * 4;
	unsigned int payload_offset = ip_header_len + transport_header_len;

	if (pkt->ip_total_len < payload_offset)
		return false;

	pkt->payload_offset = (__u16)payload_offset;
	pkt->payload_len = pkt->ip_total_len - pkt->payload_offset;
	return true;
}

/*
 * Hàm read_transport_info
 * Công dụng: Đọc thông tin từ tầng Transport (L4) dựa trên giao thức (TCP, UDP, ICMP).
 * Trích xuất port nguồn, port đích và gọi set_payload_bounds để thiết lập giới hạn dữ liệu.
 */
static bool read_transport_info(const struct sk_buff *skb,
				const struct iphdr *iph,
				struct capture_packet_info *pkt)
{
	unsigned int offset = skb_network_offset(skb) + iph->ihl * 4;

	if (iph->protocol == IPPROTO_TCP) {
		struct tcphdr tcph_buf;
		const struct tcphdr *tcph;
		unsigned int tcp_header_len;

		tcph = skb_header_pointer(skb, offset, sizeof(tcph_buf),
					  &tcph_buf);
		if (!tcph)
			return false;

		tcp_header_len = tcph->doff * 4;
		if (tcp_header_len < sizeof(*tcph))
			return false;

		pkt->src_port = tcph->source;
		pkt->dst_port = tcph->dest;
		return set_payload_bounds(iph, pkt, tcp_header_len);
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
		return set_payload_bounds(iph, pkt, sizeof(*udph));
	}

	if (iph->protocol == IPPROTO_ICMP) {
		struct icmphdr icmph_buf;
		const struct icmphdr *icmph;

		icmph = skb_header_pointer(skb, offset, sizeof(icmph_buf),
					   &icmph_buf);
		if (!icmph)
			return false;

		return set_payload_bounds(iph, pkt, sizeof(*icmph));
	}

	return false;
}

/*
 * Hàm copy_packet_snapshot
 * Công dụng: Sao chép một phần dữ liệu thực của gói tin (snapshot) từ sk_buff
 * vào cấu trúc bản ghi (record). Giới hạn tối đa PACKET_CAPTURE_SNAPSHOT_BYTES.
 */
static int copy_packet_snapshot(const struct sk_buff *skb,
				struct packet_capture_record *record)
{
	unsigned int snapshot_len;

	snapshot_len = min_t(unsigned int, record->ip_total_len,
			     PACKET_CAPTURE_SNAPSHOT_BYTES);
	if (!snapshot_len)
		return 0;

	if (skb_copy_bits(skb, skb_network_offset(skb), record->data,
			  snapshot_len) < 0)
		return -EFAULT;

	record->captured_len = (__u16)snapshot_len;
	return 0;
}

/*
 * Hàm store_packet_record
 * Công dụng: Đóng gói thông tin metadata và snapshot của gói tin vào một record,
 * sau đó đẩy record này vào Ring Buffer. Nếu buffer đầy, nó sẽ ghi đè lên
 * record cũ nhất (cập nhật capture_head).
 */
static void store_packet_record(const struct capture_packet_info *pkt,
				const struct sk_buff *skb)
{
	struct packet_capture_record record = { 0 };
	unsigned long flags;
	unsigned int index;

	record.timestamp_ns = ktime_get_ns();
	record.direction = pkt->direction;
	record.protocol = pkt->protocol;
	record.ip_total_len = pkt->ip_total_len;
	record.src_addr = pkt->src_addr;
	record.dst_addr = pkt->dst_addr;
	record.src_port = pkt->src_port;
	record.dst_port = pkt->dst_port;
	record.payload_offset = pkt->payload_offset;
	record.payload_len = pkt->payload_len;

	if (copy_packet_snapshot(skb, &record) < 0) {
		atomic64_inc(&dropped_records);
		return;
	}

	spin_lock_irqsave(&capture_lock, flags);

	record.seq = capture_next_seq++;
	if (capture_count < PACKET_CAPTURE_RING_SIZE) {
		index = (capture_head + capture_count) % PACKET_CAPTURE_RING_SIZE;
		capture_count++;
	} else {
		index = capture_head;
		capture_head = (capture_head + 1) % PACKET_CAPTURE_RING_SIZE;
		atomic64_inc(&overwritten_records);
	}

	capture_ring[index] = record;
	atomic64_inc(&records_stored);

	spin_unlock_irqrestore(&capture_lock, flags);
}

/*
 * Hàm capture_hook_common
 * Công dụng: Hàm xử lý cốt lõi được gọi bởi cả hook IN và OUT.
 * - Loại bỏ các gói tin không liên quan (không phải IPv4, phân mảnh, không đúng giao thức).
 * - Lấy thông tin cơ bản, so khớp bộ lọc, và lưu gói tin nếu thỏa mãn.
 * - Luôn luôn trả về NF_ACCEPT để không can thiệp vào quá trình mạng của hệ thống.
 */
static unsigned int capture_hook_common(struct sk_buff *skb, __u8 direction)
{
	struct packet_capture_filter filter_snapshot;
	struct capture_packet_info pkt = { 0 };
	const struct iphdr *iph;
	unsigned long flags;

	if (!skb)
		return NF_ACCEPT;

	atomic64_inc(&packets_seen);

	iph = ip_hdr(skb);
	if (!iph || iph->version != 4 || iph->ihl < 5)
		return NF_ACCEPT;

	if (ip_is_fragment(iph))
		return NF_ACCEPT;

	if (iph->protocol != IPPROTO_TCP &&
	    iph->protocol != IPPROTO_UDP &&
	    iph->protocol != IPPROTO_ICMP)
		return NF_ACCEPT;

	pkt.direction = direction;
	pkt.protocol = iph->protocol;
	pkt.ip_total_len = ntohs(iph->tot_len);
	pkt.src_addr = iph->saddr;
	pkt.dst_addr = iph->daddr;

	if (!read_transport_info(skb, iph, &pkt)) {
		atomic64_inc(&dropped_records);
		return NF_ACCEPT;
	}

	spin_lock_irqsave(&capture_lock, flags);
	filter_snapshot = active_filter;
	spin_unlock_irqrestore(&capture_lock, flags);

	if (!filter_matches_packet(&filter_snapshot, &pkt))
		return NF_ACCEPT;

	atomic64_inc(&packets_matched);
	store_packet_record(&pkt, skb);
	return NF_ACCEPT;
}

/*
 * Hàm capture_in_hook
 * Công dụng: Hook cho NF_INET_LOCAL_IN, bắt các gói tin đang đi vào hệ thống.
 */
static unsigned int capture_in_hook(void *priv, struct sk_buff *skb,
				    const struct nf_hook_state *state)
{
	(void)priv;
	(void)state;
	return capture_hook_common(skb, PACKET_CAPTURE_DIR_IN);
}

/*
 * Hàm capture_out_hook
 * Công dụng: Hook cho NF_INET_LOCAL_OUT, bắt các gói tin đang đi ra từ hệ thống.
 */
static unsigned int capture_out_hook(void *priv, struct sk_buff *skb,
				     const struct nf_hook_state *state)
{
	(void)priv;
	(void)state;
	return capture_hook_common(skb, PACKET_CAPTURE_DIR_OUT);
}

/*
 * Hàm clear_records
 * Công dụng: Xóa toàn bộ dữ liệu trong Ring Buffer và đặt lại các biến đếm thống kê về 0.
 */
static void clear_records(void)
{
	unsigned long flags;

	spin_lock_irqsave(&capture_lock, flags);
	memset(capture_ring, 0, sizeof(capture_ring));
	capture_head = 0;
	capture_count = 0;
	capture_next_seq = 1;
	spin_unlock_irqrestore(&capture_lock, flags);

	atomic64_set(&packets_seen, 0);
	atomic64_set(&packets_matched, 0);
	atomic64_set(&records_stored, 0);
	atomic64_set(&overwritten_records, 0);
	atomic64_set(&dropped_records, 0);
}

/*
 * Hàm fill_stats
 * Công dụng: Thu thập các số liệu thống kê (số gói đã thấy, đã khớp, đã ghi đè, v.v.)
 * để gửi trả về cho user-space qua lệnh ioctl.
 */
static void fill_stats(struct packet_capture_stats *stats)
{
	unsigned long flags;

	stats->packets_seen = atomic64_read(&packets_seen);
	stats->packets_matched = atomic64_read(&packets_matched);
	stats->records_stored = atomic64_read(&records_stored);
	stats->overwritten_records = atomic64_read(&overwritten_records);
	stats->dropped_records = atomic64_read(&dropped_records);
	stats->buffer_size = PACKET_CAPTURE_RING_SIZE;

	spin_lock_irqsave(&capture_lock, flags);
	stats->buffer_count = capture_count;
	stats->newest_seq = capture_next_seq > 1 ? capture_next_seq - 1 : 0;
	spin_unlock_irqrestore(&capture_lock, flags);
}

/*
 * Hàm fill_read_request
 * Công dụng: Xử lý yêu cầu đọc gói tin từ user-space.
 * Copy các bản ghi mới (có seq > since_seq) từ Ring Buffer vào cấu trúc request
 * (giới hạn tối đa bằng limit).
 */
static void fill_read_request(struct packet_capture_read_request *request)
{
	unsigned long flags;
	unsigned int i;
	unsigned int limit;

	limit = request->limit;
	if (limit == 0 || limit > PACKET_CAPTURE_MAX_READ)
		limit = PACKET_CAPTURE_MAX_READ;

	request->count = 0;
	request->overwritten_records = atomic64_read(&overwritten_records);
	request->dropped_records = atomic64_read(&dropped_records);

	spin_lock_irqsave(&capture_lock, flags);
	request->newest_seq = capture_next_seq > 1 ? capture_next_seq - 1 : 0;

	for (i = 0; i < capture_count && request->count < limit; i++) {
		unsigned int index = (capture_head + i) % PACKET_CAPTURE_RING_SIZE;
		struct packet_capture_record record = capture_ring[index];

		if (record.seq <= request->since_seq)
			continue;

		request->records[request->count++] = record;
	}

	spin_unlock_irqrestore(&capture_lock, flags);
}

/*
 * Hàm capture_ioctl
 * Công dụng: Giao tiếp với user-space qua các lệnh ioctl.
 * Xử lý các lệnh như cài đặt/lấy bộ lọc, xóa dữ liệu, lấy thống kê và đọc gói tin.
 */
static long capture_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct packet_capture_filter filter;
	struct packet_capture_stats stats;
	struct packet_capture_read_request *request;
	unsigned long flags;
	int ret;

	(void)file;

	switch (cmd) {
	case PACKET_CAPTURE_IOCTL_SET_FILTER:
		if (copy_from_user(&filter, (void __user *)arg, sizeof(filter)))
			return -EFAULT;

		ret = validate_filter(&filter);
		if (ret)
			return ret;

		spin_lock_irqsave(&capture_lock, flags);
		active_filter = filter;
		spin_unlock_irqrestore(&capture_lock, flags);

		pr_info("packet_capture: set filter protocol=%s src=%pI4 dst=%pI4 direction=%s enabled=%u\n",
			protocol_name(filter.protocol), &filter.src_addr,
			&filter.dst_addr,
			direction_name(filter.direction), filter.enabled);
		return 0;

	case PACKET_CAPTURE_IOCTL_GET_FILTER:
		spin_lock_irqsave(&capture_lock, flags);
		filter = active_filter;
		spin_unlock_irqrestore(&capture_lock, flags);

		if (copy_to_user((void __user *)arg, &filter, sizeof(filter)))
			return -EFAULT;
		return 0;

	case PACKET_CAPTURE_IOCTL_CLEAR:
		clear_records();
		pr_info("packet_capture: records cleared\n");
		return 0;

	case PACKET_CAPTURE_IOCTL_GET_STATS:
		memset(&stats, 0, sizeof(stats));
		fill_stats(&stats);
		if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;

	case PACKET_CAPTURE_IOCTL_READ:
		request = kzalloc(sizeof(*request), GFP_KERNEL);
		if (!request)
			return -ENOMEM;

		if (copy_from_user(request, (void __user *)arg, sizeof(*request))) {
			kfree(request);
			return -EFAULT;
		}

		fill_read_request(request);
		ret = copy_to_user((void __user *)arg, request, sizeof(*request))
			? -EFAULT : 0;
		kfree(request);
		return ret;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations capture_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = capture_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = capture_ioctl,
#endif
};

static struct miscdevice capture_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PACKET_CAPTURE_DEVICE_NAME,
	.fops = &capture_fops,
	.mode = 0600,
};

/*
 * Hàm packet_capture_init
 * Công dụng: Hàm khởi tạo module khi được nạp vào kernel (insmod).
 * - Đăng ký miscdevice để tạo character device giao tiếp ioctl.
 * - Đăng ký 2 Netfilter hooks để chặn bắt gói tin IN và OUT.
 */
static int __init packet_capture_init(void)
{
	int ret;

	ret = misc_register(&capture_miscdev);
	if (ret)
		return ret;

	capture_hooks[0].hook = capture_in_hook;
	capture_hooks[0].pf = PF_INET;
	capture_hooks[0].hooknum = NF_INET_LOCAL_IN;
	capture_hooks[0].priority = NF_IP_PRI_FIRST;

	capture_hooks[1].hook = capture_out_hook;
	capture_hooks[1].pf = PF_INET;
	capture_hooks[1].hooknum = NF_INET_LOCAL_OUT;
	capture_hooks[1].priority = NF_IP_PRI_FIRST;

	ret = nf_register_net_hook(&init_net, &capture_hooks[0]);
	if (ret)
		goto err_misc;

	ret = nf_register_net_hook(&init_net, &capture_hooks[1]);
	if (ret)
		goto err_in_hook;

	pr_info("packet_capture: loaded, device=%s\n",
		PACKET_CAPTURE_DEVICE_PATH);
	return 0;

err_in_hook:
	nf_unregister_net_hook(&init_net, &capture_hooks[0]);
err_misc:
	misc_deregister(&capture_miscdev);
	return ret;
}

/*
 * Hàm packet_capture_exit
 * Công dụng: Hàm dọn dẹp khi gỡ module khỏi kernel (rmmod).
 * - Hủy đăng ký Netfilter hooks.
 * - Hủy đăng ký miscdevice.
 */
static void __exit packet_capture_exit(void)
{
	nf_unregister_net_hook(&init_net, &capture_hooks[1]);
	nf_unregister_net_hook(&init_net, &capture_hooks[0]);
	misc_deregister(&capture_miscdev);
	pr_info("packet_capture: unloaded\n");
}

module_init(packet_capture_init);
module_exit(packet_capture_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Realtime IPv4 ICMP/TCP/UDP packet capture with netfilter");
MODULE_VERSION("0.1");
