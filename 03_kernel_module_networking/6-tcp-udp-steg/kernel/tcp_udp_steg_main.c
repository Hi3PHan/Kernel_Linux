// SPDX-License-Identifier: GPL-2.0
/*
 * tcp_udp_steg - Kernel module giấu tin (steganography) trong header IPv4.
 *
 * Nguyên lý:
 *   - Mỗi gói IPv4 TCP/UDP mang 1 bit bí mật trong bit thấp nhất (LSB)
 *     của trường Identification (iph->id, 16 bit).
 *   - Module KHÔNG sửa payload TCP/UDP, nên ứng dụng vẫn hoạt động bình thường.
 *
 * Hai Netfilter hook:
 *   NF_INET_LOCAL_OUT  (hook_out) - Ghi bit vào iph->id trước khi gói đi ra.
 *   NF_INET_LOCAL_IN   (hook_in)  - Đọc bit từ iph->id khi gói đến, ghép thành ký tự.
 *
 * Giao tiếp user-space: miscdevice /dev/tcp_udp_steg + ioctl.
 */
#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/init.h>
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
#include "../include/tcp_udp_steg_ioctl.h"

/* ======================================================================
 * Trạng thái TX (transmit - phía gửi bit)
 * Được bảo vệ bởi tx_lock vì hook chạy trong softirq context.
 * ====================================================================== */
static DEFINE_SPINLOCK(tx_lock);
static char  tx_msg[STEG_MAX_MESSAGE]; /* Chuỗi cần giấu (tối đa 256 byte) */
static __u32 tx_len;                   /* Độ dài chuỗi (byte) */
static __u32 tx_pos;                   /* Vị trí bit tiếp theo cần gửi (0..tx_len*8-1) */
static bool  tx_on;                    /* true = còn bit chưa gửi */

/* ======================================================================
 * Trạng thái RX (receive - phía nhận bit)
 * ====================================================================== */
static DEFINE_SPINLOCK(rx_lock);
static __u8 rx_byte; /* Byte đang ghép dần từ các bit nhận được */
static __u8 rx_bits; /* Số bit đã ghép vào rx_byte (0..7) */
static __u8 rx_last; /* Ký tự cuối cùng đã ghép đủ 8 bit */

/* ======================================================================
 * pkt_ok() - Kiểm tra gói có đủ điều kiện để giấu/đọc bit không.
 *
 * Điều kiện:
 *   - Phải là IPv4 (version=4, ihl>=5).
 *   - Không bị phân mảnh (fragment) vì iph->id của fragment có ý nghĩa khác.
 *   - Protocol là TCP hoặc UDP.
 *   - Transport header phải truy cập được trong skb (kiểm tra bằng
 *     skb_header_pointer để tránh đọc ngoài vùng nhớ hợp lệ).
 * ====================================================================== */
static bool pkt_ok(const struct sk_buff *skb, const struct iphdr *iph)
{
	unsigned int off;

	if (!skb || !iph || iph->version != 4 || iph->ihl < 5)
		return false;

	/* Bỏ qua gói bị fragment - iph->id trong fragment có vai trò reassembly */
	if (ip_is_fragment(iph))
		return false;

	/* Offset đến transport header = network_offset + IP header length */
	off = skb_network_offset(skb) + iph->ihl * 4;

	if (iph->protocol == IPPROTO_TCP) {
		struct tcphdr h;
		/* skb_header_pointer: đọc an toàn từ skb, trả NULL nếu thiếu dữ liệu */
		return skb_header_pointer(skb, off, sizeof(h), &h) != NULL;
	}
	if (iph->protocol == IPPROTO_UDP) {
		struct udphdr h;
		return skb_header_pointer(skb, off, sizeof(h), &h) != NULL;
	}
	return false;
}

/* ======================================================================
 * next_bit() - Lấy 1 bit tiếp theo từ tx_msg để nhúng vào gói.
 *
 * Bit được lấy theo thứ tự MSB-first của từng byte:
 *   byte 0: bit7, bit6, ..., bit0
 *   byte 1: bit7, bit6, ..., bit0
 *   ...
 *
 * Trả về false nếu đã gửi hết tất cả bit.
 * ====================================================================== */
static bool next_bit(__u8 *bit)
{
	unsigned long flags;
	bool ok = false;

	spin_lock_irqsave(&tx_lock, flags);
	if (tx_on && tx_pos < tx_len * 8) {
		/*
		 * tx_pos / 8  : chỉ số byte trong tx_msg
		 * 7 - tx_pos%8: chỉ số bit trong byte đó (MSB trước)
		 */
		*bit = ((__u8)tx_msg[tx_pos / 8] >> (7 - tx_pos % 8)) & 1;
		if (++tx_pos >= tx_len * 8)
			tx_on = false; /* Đã gửi hết, tắt TX */
		ok = true;
	}
	spin_unlock_irqrestore(&tx_lock, flags);
	return ok;
}

/* ======================================================================
 * hook_out() - Netfilter hook NF_INET_LOCAL_OUT
 *
 * Được gọi mỗi khi máy này gửi ra 1 gói IPv4.
 * Nếu còn bit cần gửi và gói thỏa mãn pkt_ok():
 *   1. skb_make_writable() - đảm bảo header có thể ghi (clone skb nếu cần).
 *   2. Lấy 1 bit từ next_bit().
 *   3. Ghi bit vào LSB của iph->id.
 *   4. Tính lại IP checksum bằng ip_send_check().
 * ====================================================================== */
static unsigned int hook_out(void *priv, struct sk_buff *skb,
			     const struct nf_hook_state *state)
{
	struct iphdr *iph;
	__u8 bit;
	__u16 id;

	(void)priv; (void)state;
	if (!skb)
		return NF_ACCEPT;

	iph = ip_hdr(skb);
	if (!pkt_ok(skb, iph))
		return NF_ACCEPT;

	/* Kiểm tra nhanh còn bit không (tránh gọi skb_make_writable vô ích) */
	{
		unsigned long f;
		bool pending;
		spin_lock_irqsave(&tx_lock, f);
		pending = tx_on && tx_pos < tx_len * 8;
		spin_unlock_irqrestore(&tx_lock, f);
		if (!pending)
			return NF_ACCEPT;
	}

	/*
	 * skb_make_writable() đảm bảo ít nhất (ihl*4) byte đầu của IP header
	 * có thể ghi. Nếu skb đang chia sẻ (cloned), hàm này sẽ tách bản sao.
	 * Trả về 0 = thành công, khác 0 = lỗi → bỏ qua gói này.
	 */
	if (skb_make_writable(skb, skb_network_offset(skb) + iph->ihl * 4))
		return NF_ACCEPT;

	if (!next_bit(&bit))
		return NF_ACCEPT;

	/* Đọc lại iph sau skb_make_writable vì con trỏ có thể thay đổi */
	iph = ip_hdr(skb);

	/* Ghi bit vào LSB của iph->id, giữ nguyên 15 bit còn lại */
	id  = (ntohs(iph->id) & 0xfffe) | bit;
	iph->id    = htons(id);

	/* Cần tính lại checksum sau khi thay đổi header */
	iph->check = 0;
	ip_send_check(iph);

	pr_debug("tcp_udp_steg: TX bit=%u\n", bit);
	return NF_ACCEPT;
}

/* ======================================================================
 * hook_in() - Netfilter hook NF_INET_LOCAL_IN
 *
 * Được gọi khi máy này nhận 1 gói IPv4.
 * Đọc LSB của iph->id, ghép vào rx_byte (MSB-first).
 * Sau 8 bit ghép đủ 1 ký tự thì in ra dmesg.
 * ====================================================================== */
static unsigned int hook_in(void *priv, struct sk_buff *skb,
			    const struct nf_hook_state *state)
{
	const struct iphdr *iph;
	__u8 bit, ch = 0;
	bool done = false;
	unsigned long flags;

	(void)priv; (void)state;
	if (!skb)
		return NF_ACCEPT;

	iph = ip_hdr(skb);
	if (!pkt_ok(skb, iph))
		return NF_ACCEPT;

	/* Lấy bit thấp nhất của IP Identification */
	bit = ntohs(iph->id) & 1;

	spin_lock_irqsave(&rx_lock, flags);
	rx_byte = (rx_byte << 1) | bit; /* Dịch trái rồi chèn bit mới vào LSB */
	if (++rx_bits == 8) {           /* Đủ 8 bit = 1 ký tự hoàn chỉnh */
		ch      = rx_byte;
		rx_last = ch;
		rx_byte = rx_bits = 0;  /* Reset để ghép ký tự tiếp theo */
		done = true;
	}
	spin_unlock_irqrestore(&rx_lock, flags);

	if (done) {
		/* In ký tự đọc được ra kernel log (xem bằng dmesg) */
		if (isprint(ch))
			pr_info("tcp_udp_steg: RX '%c' (0x%02x)\n", ch, ch);
		else
			pr_info("tcp_udp_steg: RX 0x%02x\n", ch);
	}
	return NF_ACCEPT;
}

/* ======================================================================
 * do_clear() - Xóa toàn bộ trạng thái TX và RX về 0.
 * Được gọi từ ioctl STEG_IOCTL_CLEAR.
 * ====================================================================== */
static void do_clear(void)
{
	unsigned long f;

	spin_lock_irqsave(&tx_lock, f);
	memset(tx_msg, 0, sizeof(tx_msg));
	tx_len = tx_pos = 0;
	tx_on  = false;
	spin_unlock_irqrestore(&tx_lock, f);

	spin_lock_irqsave(&rx_lock, f);
	rx_byte = rx_bits = rx_last = 0;
	spin_unlock_irqrestore(&rx_lock, f);
}

/* ======================================================================
 * steg_ioctl() - Xử lý các lệnh ioctl từ user-space (stegctl tool).
 *
 * STEG_IOCTL_SET_MESSAGE : copy chuỗi từ user vào tx_msg, kích hoạt TX.
 * STEG_IOCTL_CLEAR       : gọi do_clear().
 * STEG_IOCTL_GET_STATUS  : điền struct steg_status rồi copy về user.
 * ====================================================================== */
static long steg_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long f;

	(void)file;
	switch (cmd) {
	case STEG_IOCTL_SET_MESSAGE: {
		struct steg_message m;

		/* copy_from_user: đọc dữ liệu từ user-space an toàn */
		if (copy_from_user(&m, (void __user *)arg, sizeof(m)))
			return -EFAULT;
		if (m.len > STEG_MAX_MESSAGE)
			return -EINVAL;

		spin_lock_irqsave(&tx_lock, f);
		memset(tx_msg, 0, sizeof(tx_msg));
		memcpy(tx_msg, m.data, m.len);
		tx_len = m.len;
		tx_pos = 0;          /* Reset vị trí bit về đầu chuỗi */
		tx_on  = m.len > 0; /* Bật TX nếu chuỗi không rỗng */
		spin_unlock_irqrestore(&tx_lock, f);

		pr_info("tcp_udp_steg: set %u bytes (%u bits)\n",
			m.len, m.len * 8);
		return 0;
	}
	case STEG_IOCTL_CLEAR:
		do_clear();
		pr_info("tcp_udp_steg: cleared\n");
		return 0;

	case STEG_IOCTL_GET_STATUS: {
		struct steg_status s;

		memset(&s, 0, sizeof(s));
		spin_lock_irqsave(&tx_lock, f);
		s.enabled       = tx_on;
		s.message_len   = tx_len;
		s.tx_bit_pos    = tx_pos;
		s.tx_total_bits = tx_len * 8;
		spin_unlock_irqrestore(&tx_lock, f);

		spin_lock_irqsave(&rx_lock, f);
		s.last_char = rx_last;
		spin_unlock_irqrestore(&rx_lock, f);

		/* copy_to_user: trả dữ liệu về user-space an toàn */
		if (copy_to_user((void __user *)arg, &s, sizeof(s)))
			return -EFAULT;
		return 0;
	}
	default:
		return -ENOTTY; /* Lệnh ioctl không hợp lệ */
	}
}

/* ======================================================================
 * Đăng ký miscdevice /dev/tcp_udp_steg
 * miscdevice đơn giản hơn cdev: kernel tự cấp minor number, tự tạo /dev entry.
 * ====================================================================== */
static const struct file_operations steg_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = steg_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = steg_ioctl, /* Hỗ trợ 32-bit process trên kernel 64-bit */
#endif
};

static struct miscdevice steg_dev = {
	.minor = MISC_DYNAMIC_MINOR, /* Kernel tự chọn minor number */
	.name  = STEG_DEVICE_NAME,  /* Tên node: /dev/tcp_udp_steg */
	.fops  = &steg_fops,
	.mode  = 0600,               /* Chỉ root đọc/ghi được */
};

/* ======================================================================
 * Khai báo Netfilter hooks (khởi tạo struct tĩnh cho gọn).
 * ====================================================================== */
static struct nf_hook_ops out_ops = {
	.hook     = hook_out,
	.pf       = PF_INET,           /* IPv4 */
	.hooknum  = NF_INET_LOCAL_OUT, /* Gói do máy này tạo ra, chưa đi ra card mạng */
	.priority = NF_IP_PRI_FIRST,   /* Ưu tiên cao nhất, chạy trước các hook khác */
};

static struct nf_hook_ops in_ops = {
	.hook     = hook_in,
	.pf       = PF_INET,
	.hooknum  = NF_INET_LOCAL_IN,  /* Gói đến và được giao cho socket local */
	.priority = NF_IP_PRI_FIRST,
};

/* ======================================================================
 * Module init / exit
 * ====================================================================== */
static int __init steg_init(void)
{
	int ret;

	/* 1. Đăng ký miscdevice để user-space có thể mở /dev/tcp_udp_steg */
	ret = misc_register(&steg_dev);
	if (ret)
		return ret;

	/* 2. Đăng ký hook OUT */
	ret = nf_register_net_hook(&init_net, &out_ops);
	if (ret)
		goto err_misc;

	/* 3. Đăng ký hook IN */
	ret = nf_register_net_hook(&init_net, &in_ops);
	if (ret)
		goto err_out;

	pr_info("tcp_udp_steg: loaded (%s)\n", STEG_DEVICE_PATH);
	return 0;

err_out:
	nf_unregister_net_hook(&init_net, &out_ops);
err_misc:
	misc_deregister(&steg_dev);
	return ret;
}

static void __exit steg_exit(void)
{
	/* Hủy đăng ký theo thứ tự ngược lại để tránh race condition */
	nf_unregister_net_hook(&init_net, &in_ops);
	nf_unregister_net_hook(&init_net, &out_ops);
	misc_deregister(&steg_dev);
	pr_info("tcp_udp_steg: unloaded\n");
}

module_init(steg_init);
module_exit(steg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Hide 1 bit/packet in IPv4 ID LSB (TCP/UDP)");
MODULE_VERSION("0.1");
