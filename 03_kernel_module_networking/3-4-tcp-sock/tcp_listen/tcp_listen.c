// SPDX-License-Identifier: GPL-2.0
/*
 * Bai 3 - TCP listen socket trong kernel
 *
 * Module tao socket TCP, bind vao 127.0.0.1:60000 va listen. Kernel module
 * khong co userspace libc, nen dung socket API cua kernel.
 */

 #include <linux/init.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/net.h>
#include <net/net_namespace.h>
#include <net/sock.h>

#define TCP_LISTEN_PORT 60000
#define TCP_LISTEN_BACKLOG 8

static struct socket *listen_sock;

static int __init tcp_listen_init(void)
{
	struct sockaddr_in addr = { 0 };
	int ret;

	/*
	 * sock_create_kern() tao socket trong kernel space. Tham so &init_net
	 * la network namespace mac dinh cua may lab.
	 */
	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
			       &listen_sock);
	if (ret) {
		pr_err("tcp_listen: sock_create_kern failed: %d\n", ret);
		return ret;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(TCP_LISTEN_PORT);

	/*
	 * kernel_bind() gan socket voi IP/port. Neu port da duoc dung, ham
	 * thuong tra -EADDRINUSE.
	 */
	ret = kernel_bind(listen_sock, (struct sockaddr_unsized *)&addr, sizeof(addr));
	if (ret) {
		pr_err("tcp_listen: kernel_bind failed: %d\n", ret);
		goto err_release;
	}

	ret = kernel_listen(listen_sock, TCP_LISTEN_BACKLOG);
	if (ret) {
		pr_err("tcp_listen: kernel_listen failed: %d\n", ret);
		goto err_release;
	}

	pr_info("tcp_listen: listening on 127.0.0.1:%d\n", TCP_LISTEN_PORT);
	return 0;

err_release:
	sock_release(listen_sock);
	listen_sock = NULL;
	return ret;
}

static void __exit tcp_listen_exit(void)
{
	if (listen_sock) {
		sock_release(listen_sock);
		listen_sock = NULL;
	}

	pr_info("tcp_listen: unloaded\n");
}

module_init(tcp_listen_init);
module_exit(tcp_listen_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Create a TCP listen socket from kernel space");
MODULE_VERSION("0.1");
