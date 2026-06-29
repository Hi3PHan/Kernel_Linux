// SPDX-License-Identifier: GPL-2.0
/*
 * Bai 5 - UDP sender trong kernel
 *
 * Module tao UDP socket va gui mot message toi 127.0.0.1:60001 khi insmod.
 * No minh hoa kernel_sendmsg(), struct msghdr va struct kvec.
 */

#include <linux/init.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/string.h>
#include <net/net_namespace.h>
#include <net/sock.h>

#define UDP_TARGET_PORT 60001
#define MY_TEST_MESSAGE "kernelsocket"

/* (1) Bien toan cuc giu socket de udp_sender_exit() co the giai phong */
static struct socket *udp_sock;

/* ------------------------------------------------------------------
 * (2) Ham gui datagram
 *
 *  Luong di chuyen cua data:
 *
 *   [MY_TEST_MESSAGE]  <-- iov.iov_base
 *        |
 *        v
 *   struct kvec iov      <-- mo ta buffer kernel-space (giong iovec user)
 *        |
 *        v
 *   kernel_sendmsg()     <-- API gui tin nhan tu kernel space
 *        |
 *        v
 *   UDP/IP stack (net/ipv4/udp.c)
 *        |
 *        v
 *   127.0.0.1:60001
 * ------------------------------------------------------------------ */
static int send_udp_message(void)
{
	/*
	 * sockaddr_in: dia chi dich, tuong duong voi:
	 *   sendto(fd, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr))
	 * trong user-space.
	 */
	struct sockaddr_in addr = { 0 };

	/*
	 * msghdr: "phong bi" chua metadata cua datagram.
	 *   - msg_name    -> con tro toi sockaddr (dia chi dich)
	 *   - msg_namelen -> kich thuoc sockaddr
	 * Phan payload KHONG nam trong msghdr, ma nam trong kvec phia duoi.
	 */
	struct msghdr msg = { 0 };

	/*
	 * kvec: tuong duong struct iovec cua user-space nhung dung cho
	 * kernel-space buffer. Ho tro scatter-gather: co the truyen mang
	 * nhieu kvec de gop nhieu buffer roi nam bo nho thanh 1 datagram.
	 *   - iov_base -> con tro toi vung nho chua data
	 *   - iov_len  -> so byte can gui
	 */
	struct kvec iov;

	size_t len = strlen(MY_TEST_MESSAGE);
	int ret;

	/* Cau hinh dia chi dich */
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 */
	addr.sin_port        = htons(UDP_TARGET_PORT);  /* 60001 */

	/* Gan dia chi dich vao msghdr */
	msg.msg_name    = &addr;
	msg.msg_namelen = sizeof(addr);

	/* Gan payload vao kvec */
	iov.iov_base = (char *)MY_TEST_MESSAGE;
	iov.iov_len  = len;

	/*
	 * kernel_sendmsg(sock, msg, vec, num_vecs, total_len)
	 *   sock      : socket da tao boi sock_create_kern()
	 *   msg       : metadata (dia chi dich, flags...)
	 *   vec       : mang kvec chua payload
	 *   num_vecs  : so luong kvec (1 vi chi co 1 buffer)
	 *   total_len : tong so byte can gui
	 *
	 * Tra ve: so byte da gui (>= 0) hoac ma loi am.
	 */
	ret = kernel_sendmsg(udp_sock, &msg, &iov, 1, len);
	if (ret < 0) {
		pr_err("udp_sender: kernel_sendmsg failed: %d\n", ret);
		return ret;
	}

	pr_info("udp_sender: sent %d bytes to 127.0.0.1:%d\n",
		ret, UDP_TARGET_PORT);
	return 0;
}

/* ------------------------------------------------------------------
 * (3) Ham init - chay khi insmod
 * ------------------------------------------------------------------ */
static int __init udp_sender_init(void)
{
	int ret;

	/*
	 * sock_create_kern(&init_net, domain, type, protocol, &sock)
	 *   &init_net  : namespace mang mac dinh (khong phai net namespace rieng)
	 *   AF_INET    : IPv4
	 *   SOCK_DGRAM : UDP (khong ket noi, huong datagram)
	 *   IPPROTO_UDP: chi dinh ro giao thuc la UDP
	 *
	 * Khac sock_create() o cho: sock_create_kern() bo qua kiem tra
	 * quyen user-space va dung cap phat nho kernel (GFP_KERNEL).
	 */
	ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP,
			       &udp_sock);
	if (ret) {
		pr_err("udp_sender: sock_create_kern failed: %d\n", ret);
		return ret;
	}

	pr_info("udp_sender: socket created, sending message...\n");

	ret = send_udp_message();
	if (ret)
		pr_err("udp_sender: send failed: %d\n", ret);

	return 0; /* luon tra 0 de module load thanh cong */
}

/* ------------------------------------------------------------------
 * (4) Ham exit - chay khi rmmod
 * ------------------------------------------------------------------ */
static void __exit udp_sender_exit(void)
{
	/*
	 * sock_release() dong socket va giai phong tai nguyen kernel.
	 * Tuong duong close(fd) trong user-space nhung cho struct socket.
	 */
	if (udp_sock) {
		sock_release(udp_sock);
		udp_sock = NULL;
	}

	pr_info("udp_sender: unloaded\n");
}

module_init(udp_sender_init);
module_exit(udp_sender_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Send a UDP datagram from kernel space");
MODULE_VERSION("0.1");
