// SPDX-License-Identifier: GPL-2.0
/*
 * Bai 4 - TCP accept trong kernel
 *
 * Module tao TCP listener o 127.0.0.1:60000 va dung kernel thread de accept
 * connection. Khong block trong module_init(), vi init bi block se lam insmod
 * treo cho den khi co client ket noi.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/net.h>
#include <net/net_namespace.h>
#include <net/sock.h>

#define TCP_ACCEPT_PORT 60000
#define TCP_ACCEPT_BACKLOG 8

static struct socket *listen_sock;
static struct task_struct *accept_task;

static int setup_listener(void)
{
	struct sockaddr_in addr = { 0 };
	int ret;

	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
			       &listen_sock);
	if (ret)
		return ret;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(TCP_ACCEPT_PORT);

	ret = kernel_bind(listen_sock, (struct sockaddr_unsized *)&addr, sizeof(addr));
	if (ret)
		goto err_release;

	ret = kernel_listen(listen_sock, TCP_ACCEPT_BACKLOG);
	if (ret)
		goto err_release;

	return 0;

err_release:
	sock_release(listen_sock);
	listen_sock = NULL;
	return ret;
}

/*
 * accept_loop() goi kernel_accept() voi O_NONBLOCK de thread co the kiem tra
 * kthread_should_stop() va thoat gon khi rmmod.
 */
static int accept_loop(void *data)
{
	(void)data;
	
	while(!kthread_should_stop()){
		struct socket *client_sock = NULL;
		struct sockaddr_in peer = { 0};
		int ret;

		ret = kernel_accept(listen_sock, &client_sock, O_NONBLOCK);
		if(ret == -EAGAIN || ret == -EWOULDBLOCK){
			msleep(200);
			continue;
		}
		if(ret){
			pr_err("tcp_accept: kernel_accept failed: %d\n", ret);
			msleep(500);
			continue;
		}
		ret = kernel_getpeername(client_sock, (struct sockaddr *)&peer);
		if(ret >= 0){
			pr_info("tcp_accept: kernel accept connection from %pI4:%u\n",
			&peer.sin_addr.s_addr, ntohs(peer.sin_port));
		}
		
		sock_release(client_sock);
		client_sock = NULL;
	}
	return 0;
}

static int __init tcp_accept_init(void)
{
	int ret;

	ret = setup_listener();
	if (ret) {
		pr_err("tcp_accept: setup_listener failed: %d\n", ret);
		return ret;
	}

	accept_task = kthread_run(accept_loop, NULL, "tcp_accept_lab");
	if (IS_ERR(accept_task)) {
		ret = PTR_ERR(accept_task);
		accept_task = NULL;
		goto err_release;
	}

	pr_info("tcp_accept: listening on 127.0.0.1:%d\n", TCP_ACCEPT_PORT);
	return 0;

err_release:
	sock_release(listen_sock);
	listen_sock = NULL;
	return ret;
}

static void __exit tcp_accept_exit(void)
{
	if (accept_task) {
		kthread_stop(accept_task);
		accept_task = NULL;
	}

	if (listen_sock) {
		sock_release(listen_sock);
		listen_sock = NULL;
	}

	pr_info("tcp_accept: unloaded\n");
}

module_init(tcp_accept_init);
module_exit(tcp_accept_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Cuoi Ky");
MODULE_DESCRIPTION("Accept TCP connections from kernel space");
MODULE_VERSION("0.1");
