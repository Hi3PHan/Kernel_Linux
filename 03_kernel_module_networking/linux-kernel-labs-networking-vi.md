# Networking

Nguồn: https://linux-kernel-labs.github.io/refs/heads/master/labs/networking.html

## Mục tiêu bài lab

- Hiểu kiến trúc networking trong Linux kernel.
- Có kỹ năng thực hành xử lý gói IP bằng packet filter/firewall.
- Làm quen với cách sử dụng socket ở cấp Linux kernel.

## Tổng quan

Sự phát triển của Internet làm số lượng ứng dụng mạng tăng rất nhanh, kéo theo yêu cầu ngày càng cao về tốc độ và năng suất của phân hệ networking trong hệ điều hành.

Phân hệ networking không phải là thành phần bắt buộc của kernel hệ điều hành. Linux kernel có thể được biên dịch mà không có hỗ trợ networking. Tuy vậy, trong thực tế rất hiếm khi một hệ thống tính toán, kể cả thiết bị nhúng, lại chạy một hệ điều hành không có mạng vì nhu cầu kết nối gần như luôn tồn tại. Các hệ điều hành hiện đại thường dùng TCP/IP stack.

Kernel thực thi các giao thức đến tầng transport, còn các giao thức tầng application thường được cài đặt trong user space, ví dụ HTTP, FTP, SSH.

## Networking trong user space

Trong user space, trừu tượng cho giao tiếp mạng là socket. Socket trừu tượng hóa một kênh giao tiếp và là giao diện tương tác với TCP/IP stack nằm trong kernel. Một IP socket được gắn với địa chỉ IP, giao thức tầng transport được dùng như TCP, UDP, và một port.

Những lời gọi hàm thường gặp khi dùng socket gồm: tạo socket (`socket`), khởi tạo/gắn địa chỉ (`bind`), kết nối (`connect`), chờ kết nối (`listen`, `accept`) và đóng socket (`close`). Việc giao tiếp mạng được thực hiện qua `read`/`write` hoặc `recv`/`send` với TCP socket, và `recvfrom`/`sendto` với UDP socket.

Các thao tác gửi/nhận trong suốt đối với ứng dụng; việc đóng gói và truyền trên mạng do kernel quyết định.

Tuy nhiên, cũng có thể cài đặt TCP/IP stack trong user space bằng raw socket, dùng tùy chọn `PF_PACKET` khi tạo socket, hoặc cài đặt một giao thức tầng application trong kernel, ví dụ TUX web server. Để biết thêm về lập trình socket trong user space, xem Beej's Guide to Network Programming Using Internet Sockets.

## Networking trong Linux

Linux kernel cung cấp ba cấu trúc cơ bản để làm việc với gói mạng: `struct socket`, `struct sock` và `struct sk_buff`.

Hai cấu trúc đầu là các trừu tượng của socket:

- `struct socket` là trừu tượng rất gần với user space, cụ thể là BSD sockets dùng để lập trình ứng dụng mạng.
- `struct sock`, hay INET socket trong thuật ngữ Linux, là biểu diễn mạng của một socket.

Hai cấu trúc này có liên hệ với nhau: `struct socket` chứa một trường INET socket, và `struct sock` có một BSD socket giữ nó. Cấu trúc `struct sk_buff` là biểu diễn của một gói mạng và trạng thái của gói đó. Cấu trúc này được tạo khi kernel nhận một gói, từ user space hoặc từ network interface.

## Cấu trúc `struct socket`

`struct socket` là biểu diễn của BSD socket trong kernel. Các thao tác có thể thực hiện trên nó tương tự các thao tác được kernel cung cấp qua system call. Các thao tác socket thông dụng như tạo, khởi tạo/bind, đóng socket sẽ dẫn đến các system call cụ thể; chúng làm việc với cấu trúc `struct socket`.

Các thao tác của `struct socket` được mô tả trong file `net/socket.c` và độc lập với kiểu giao thức. Vì vậy `struct socket` là giao diện tổng quát nằm trên các cài đặt thao tác mạng cụ thể. Tên của các thao tác này thường bắt đầu bằng tiền tố `sock_`.

### Thao tác trên `struct socket`

#### Tạo socket

Tạo socket tương tự việc gọi hàm `socket` trong user space, nhưng `struct socket` được tạo sẽ được lưu trong tham số `res`:

- `int sock_create(int family, int type, int protocol, struct socket **res)` tạo socket sau system call `socket`.
- `int sock_create_kern(struct net *net, int family, int type, int protocol, struct socket **res)` tạo kernel socket.
- `int sock_create_lite(int family, int type, int protocol, struct socket **res)` tạo kernel socket mà không kiểm tra đầy đủ tính hợp lệ của tham số.

Các tham số:

- `net`, nếu có, là tham chiếu đến network namespace được dùng; thường khởi tạo bằng `init_net`.
- `family` là họ giao thức dùng trong việc truyền thông tin; thường bắt đầu bằng `PF_` (Protocol Family). Các hằng số nằm trong `linux/socket.h`; hay gặp nhất là `PF_INET` cho TCP/IP.
- `type` là kiểu socket. Các hằng số nằm trong `linux/net.h`; hay dùng là `SOCK_STREAM` cho giao tiếp có kết nối từ nguồn đến đích và `SOCK_DGRAM` cho giao tiếp không kết nối.
- `protocol` là giao thức được dùng và liên quan chặt chẽ tới `type`. Các hằng số nằm trong `linux/in.h`; hay dùng là `IPPROTO_TCP` cho TCP và `IPPROTO_UDP` cho UDP.

Tạo TCP socket trong kernel space:

```c
struct socket *sock;
int err;

err = sock_create_kern(&init_net, PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
if (err < 0) {
	/* handle error */
}
```

Tạo UDP socket:

```c
struct socket *sock;
int err;

err = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
if (err < 0) {
	/* handle error */
}
```

Ví dụ sử dụng nằm trong handler của system call `sys_socket`:

```c
SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)
{
	int retval;
	struct socket *sock;
	int flags;

	/* Check the SOCK_* constants for consistency. */
	BUILD_BUG_ON(SOCK_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON((SOCK_MAX | SOCK_TYPE_MASK) != SOCK_TYPE_MASK);
	BUILD_BUG_ON(SOCK_CLOEXEC & SOCK_TYPE_MASK);
	BUILD_BUG_ON(SOCK_NONBLOCK & SOCK_TYPE_MASK);

	flags = type & ~SOCK_TYPE_MASK;
	if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return -EINVAL;
	type &= SOCK_TYPE_MASK;

	if (SOCK_NONBLOCK != O_NONBLOCK && (flags & SOCK_NONBLOCK))
		flags = (flags & ~SOCK_NONBLOCK) | O_NONBLOCK;

	retval = sock_create(family, type, protocol, &sock);
	if (retval < 0)
		goto out;

	return sock_map_fd(sock, flags & (O_CLOEXEC | O_NONBLOCK));
}
```

#### Đóng socket

Đóng kết nối, với socket có kết nối, và giải phóng các tài nguyên liên quan:

- `void sock_release(struct socket *sock)` gọi hàm `release` trong trường `ops` của cấu trúc socket:

```c
void sock_release(struct socket *sock)
{
	if (sock->ops) {
		struct module *owner = sock->ops->owner;

		sock->ops->release(sock);
		sock->ops = NULL;
		module_put(owner);
	}
	//...
}
```

#### Gửi/nhận thông điệp

Thông điệp được gửi/nhận bằng các hàm:

- `int sock_recvmsg(struct socket *sock, struct msghdr *msg, int flags);`
- `int kernel_recvmsg(struct socket *sock, struct msghdr *msg, struct kvec *vec, size_t num, size_t size, int flags);`
- `int sock_sendmsg(struct socket *sock, struct msghdr *msg);`
- `int kernel_sendmsg(struct socket *sock, struct msghdr *msg, struct kvec *vec, size_t num, size_t size);`

Những hàm gửi/nhận thông điệp sau đó sẽ gọi hàm `sendmsg`/`recvmsg` trong trường `ops` của socket.

Các hàm có tiền tố `kernel_` được dùng khi socket được sử dụng trong kernel. Tham số quan trọng:

- `msg` là cấu trúc `struct msghdr`, chứa thông điệp cần gửi/nhận. Các thành phần quan trọng gồm `msg_name` và `msg_namelen`; với UDP socket, chúng phải được điền bằng địa chỉ đích của thông điệp (`struct sockaddr_in`).
- `vec` là cấu trúc `struct kvec`, chứa con trỏ tới buffer dữ liệu và kích thước của nó. Cấu trúc này tương tự `struct iovec`; `struct iovec` ứng với dữ liệu user space, còn `struct kvec` ứng với dữ liệu kernel space.

Ví dụ trong handler của system call `sys_sendto`:

```c
SYSCALL_DEFINE6(sendto, int, fd, void __user *, buff, size_t, len,
		unsigned int, flags, struct sockaddr __user *, addr,
		int, addr_len)
{
	struct socket *sock;
	struct sockaddr_storage address;
	int err;
	struct msghdr msg;
	struct iovec iov;
	int fput_needed;

	err = import_single_range(WRITE, buff, len, &iov, &msg.msg_iter);
	if (unlikely(err))
		return err;

	sock = sockfd_lookup_light(fd, &err, &fput_needed);
	if (!sock)
		goto out;

	msg.msg_name = NULL;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_namelen = 0;

	if (addr) {
		err = move_addr_to_kernel(addr, addr_len, &address);
		if (err < 0)
			goto out_put;
		msg.msg_name = (struct sockaddr *)&address;
		msg.msg_namelen = addr_len;
	}

	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;
	msg.msg_flags = flags;
	err = sock_sendmsg(sock, &msg);

out_put:
	fput_light(sock->file, fput_needed);
out:
	return err;
}
```

### Các trường của `struct socket`

```c
/**
 * struct socket - general BSD socket
 * @state: socket state (%SS_CONNECTED, etc)
 * @type: socket type (%SOCK_STREAM, etc)
 * @flags: socket flags (%SOCK_NOSPACE, etc)
 * @ops: protocol specific socket operations
 * @file: File back pointer for gc
 * @sk: internal networking protocol agnostic socket representation
 * @wq: wait queue for several uses
 */
struct socket {
	socket_state state;
	short type;
	unsigned long flags;
	struct socket_wq __rcu *wq;
	struct file *file;
	struct sock *sk;
	const struct proto_ops *ops;
};
```

Hai trường đáng chú ý:

- `ops`: cấu trúc lưu các con trỏ hàm phụ thuộc giao thức.
- `sk`: INET socket liên kết với socket này.

### Cấu trúc `struct proto_ops`

`struct proto_ops` chứa các cài đặt thao tác cụ thể theo giao thức (TCP, UDP, ...). Các hàm này sẽ được gọi từ hàm tổng quát thông qua `struct socket`, ví dụ `sock_release`, `sock_sendmsg`.

Vì vậy `struct proto_ops` chứa nhiều con trỏ hàm cho các cài đặt giao thức cụ thể:

```c
struct proto_ops {
	int family;
	struct module *owner;
	int (*release) (struct socket *sock);
	int (*bind) (struct socket *sock, struct sockaddr *myaddr,
		      int sockaddr_len);
	int (*connect) (struct socket *sock, struct sockaddr *vaddr,
			int sockaddr_len, int flags);
	int (*socketpair)(struct socket *sock1, struct socket *sock2);
	int (*accept) (struct socket *sock, struct socket *newsock,
		       int flags, bool kern);
	int (*getname) (struct socket *sock, struct sockaddr *addr,
			int peer);
	//...
}
```

Trường `ops` của `struct socket` được khởi tạo trong hàm `__sock_create`, bằng cách gọi hàm `create` riêng của từng giao thức. Một đoạn tương đương trong `__sock_create`:

```c
//...
err = pf->create(net, sock, protocol, kern);
if (err < 0)
	goto out_module_put;
//...
```

Lệnh này sẽ gán các con trỏ hàm với các lời gọi cụ thể theo kiểu giao thức gắn với socket.

Hai hàm `sock_register` và `sock_unregister` được dùng để điền vector `net_families`. Với các thao tác socket khác ngoài tạo, đóng và gửi/nhận thông điệp đã nói ở trên, kernel sẽ gọi các hàm được truyền qua con trỏ trong cấu trúc này.

Ví dụ, với `bind`, thao tác gắn một socket với một socket trên máy cục bộ, ta có đoạn code:

```c
#define MY_PORT 60000

struct sockaddr_in addr = {
	.sin_family = AF_INET,
	.sin_port = htons(MY_PORT),
	.sin_addr = { htonl(INADDR_LOOPBACK) }
};

//...
err = sock->ops->bind(sock, (struct sockaddr *)&addr, sizeof(addr));
if (err < 0) {
	/* handle error */
}
//...
```

Có thể thấy, để truyền thông tin địa chỉ và port sẽ gắn với socket, ta điền một `struct sockaddr_in`.

## Cấu trúc `struct sock`

`struct sock` mô tả một INET socket. Cấu trúc này liên kết với socket trong user space và ngầm liên kết với `struct socket`. Nó được dùng để lưu thông tin về trạng thái của kết nối. Các trường và thao tác liên quan thường bắt đầu bằng chuỗi `sk_`.

Một số trường:

```c
struct sock {
	//...
	unsigned int sk_padding : 1,
		     sk_no_check_tx : 1,
		     sk_no_check_rx : 1,
		     sk_userlocks : 4,
		     sk_protocol : 8,
		     sk_type : 16;
	//...
	struct socket *sk_socket;
	//...
	struct sk_buff *sk_send_head;
	//...
	void (*sk_state_change)(struct sock *sk);
	void (*sk_data_ready)(struct sock *sk);
	void (*sk_write_space)(struct sock *sk);
	void (*sk_error_report)(struct sock *sk);
	int (*sk_backlog_rcv)(struct sock *sk, struct sk_buff *skb);
	void (*sk_destruct)(struct sock *sk);
};
```

- `sk_protocol` là kiểu giao thức được socket sử dụng.
- `sk_type` là kiểu socket (`SOCK_STREAM`, `SOCK_DGRAM`, ...).
- `sk_socket` là BSD socket giữ nó.
- `sk_send_head` là danh sách các cấu trúc `struct sk_buff` để truyền.
- Các con trỏ hàm ở cuối cấu trúc là callback cho nhiều tình huống khác nhau.

Khởi tạo `struct sock` và gắn nó vào BSD socket được thực hiện bằng callback tạo ra từ `net_families`, được gọi trong `__sock_create`.

Ví dụ khởi tạo `struct sock` cho giao thức IP trong hàm `inet_create`:

```c
/*
 * Create an inet socket.
 */
static int inet_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	struct sock *sk;
	//...
	err = -ENOBUFS;
	sk = sk_alloc(net, PF_INET, GFP_KERNEL, answer_prot, kern);
	if (!sk)
		goto out;

	err = 0;
	if (INET_PROTOSW_REUSE & answer_flags)
		sk->sk_reuse = SK_CAN_REUSE;
	//...
	sock_init_data(sock, sk);
	sk->sk_destruct = inet_sock_destruct;
	sk->sk_protocol = protocol;
	sk->sk_backlog_rcv = sk->sk_prot->backlog_rcv;
	//...
}
```

## Cấu trúc `struct sk_buff`

`struct sk_buff` (socket buffer) mô tả một gói mạng.

Những trường trong cấu trúc chứa thông tin về header và nội dung gói, các giao thức được dùng, thiết bị mạng được dùng và con trỏ tới các `struct sk_buff` khác.

Tóm tắt nội dung của cấu trúc:

```c
struct sk_buff {
	union {
		struct {
			/* These two members must be first. */
			struct sk_buff *next;
			struct sk_buff *prev;
			union {
				struct net_device *dev;
				unsigned long dev_scratch;
			};
		};
		struct rb_node rbnode;
	};
	struct sock *sk;
	union {
		ktime_t tstamp;
		u64 skb_mstamp;
	};
	char cb[48] __aligned(8);
	unsigned long _skb_refdst;
	void (*destructor)(struct sk_buff *skb);
	union {
		struct {
			unsigned long _skb_refdst;
			void (*destructor)(struct sk_buff *skb);
		};
		struct list_head tcp_tsorted_anchor;
	};
	/* ... */
	unsigned int len, data_len;
	__u16 mac_len, hdr_len;
	/* ... */
	__be16 protocol;
	__u16 transport_header;
	__u16 network_header;
	__u16 mac_header;
	/* private: */
	__u32 headers_end[0];
	/* public: */
	sk_buff_data_t tail;
	sk_buff_data_t end;
	unsigned char *head, *data;
	unsigned int truesize;
	refcount_t users;
};
```

Trong đó:

- `next` và `prev` là con trỏ tới phần tử kế tiếp và phần tử trước trong danh sách buffer.
- `dev` là thiết bị gửi hoặc nhận buffer.
- `sk` là socket liên kết với buffer.
- `destructor` là callback hủy/giải phóng buffer.
- `transport_header`, `network_header`, `mac_header` là offset từ đầu gói đến đầu các header tương ứng trong gói.

Các trường header này được các tầng xử lý bên trong kernel cập nhật khi gói đi qua từng tầng. Để lấy con trỏ tới header, dùng các hàm như `tcp_hdr`, `udp_hdr`, `ip_hdr`. Về nguyên tắc, mỗi giao thức cung cấp một hàm để lấy tham chiếu tới header của giao thức đó trong gói vừa nhận.

Lưu ý rằng trường `network_header` chưa được gán cho đến khi gói tới tầng network, và `transport_header` chưa được gán cho đến khi gói tới tầng transport.

Cấu trúc IP header (`struct iphdr`) có các trường:

```c
struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 ihl:4, version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8 version:4, ihl:4;
#else
#error "Please fix"
#endif
	__u8 tos;
	__be16 tot_len;
	__be16 id;
	__be16 frag_off;
	__u8 ttl;
	__u8 protocol;
	__sum16 check;
	__be32 saddr;
	__be32 daddr;
	/* The options start here. */
};
```

Trong đó:

- `protocol` là giao thức tầng transport được dùng.
- `saddr` là địa chỉ IP nguồn.
- `daddr` là địa chỉ IP đích.

Cấu trúc TCP header (`struct tcphdr`) có các trường:

```c
struct tcphdr {
	__be16 source;
	__be16 dest;
	__be32 seq;
	__be32 ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u16 res1:4, doff:4, fin:1, syn:1, rst:1, psh:1,
	      ack:1, urg:1, ece:1, cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u16 doff:4, res1:4, cwr:1, ece:1, urg:1, ack:1,
	      psh:1, rst:1, syn:1, fin:1;
#else
#error "Adjust your defines"
#endif
	__be16 window;
	__sum16 check;
	__be16 urg_ptr;
};
```

Trong đó:

- `source` là port nguồn.
- `dest` là port đích.
- `syn`, `ack`, `fin` là các cờ TCP được dùng; xem diagram TCP để có góc nhìn chi tiết hơn.

Cấu trúc UDP header (`struct udphdr`) có các trường:

```c
struct udphdr {
	__be16 source;
	__be16 dest;
	__be16 len;
	__sum16 check;
};
```

Trong đó:

- `source` là port nguồn.
- `dest` là port đích.

Ví dụ truy cập thông tin trong header của một gói mạng:

```c
struct sk_buff *skb;
struct iphdr *iph = ip_hdr(skb);	/* IP header */

/* iph->saddr - source IP address */
/* iph->daddr - destination IP address */

if (iph->protocol == IPPROTO_TCP) {
	/* TCP protocol */
	struct tcphdr *tcph = tcp_hdr(skb);	/* TCP header */

	/* tcph->source - source TCP port */
	/* tcph->dest - destination TCP port */
} else if (iph->protocol == IPPROTO_UDP) {
	/* UDP protocol */
	struct udphdr *udph = udp_hdr(skb);	/* UDP header */

	/* udph->source - source UDP port */
	/* udph->dest - destination UDP port */
}
```

## Chuyển đổi byte order

Trên các hệ thống khác nhau có nhiều cách sắp xếp byte trong một word (endianness), gồm Big Endian, byte có ý nghĩa lớn nhất đứng trước, và Little Endian, byte có ý nghĩa nhỏ nhất đứng trước. Vì mạng kết nối nhiều hệ thống với nhiều nền tảng khác nhau, Internet áp đặt một thứ tự chuẩn để lưu trữ dữ liệu số, gọi là network byte-order.

Ngược lại, thứ tự byte để biểu diễn dữ liệu số trên máy host được gọi là host byte-order. Dữ liệu nhận từ/gửi tới mạng nằm ở dạng network byte-order và cần được chuyển đổi qua lại với host byte-order.

Các macro chuyển đổi:

- `u16 htons(u16 x)` chuyển số nguyên 16 bit từ host byte-order sang network byte-order (host to network short).
- `u32 htonl(u32 x)` chuyển số nguyên 32 bit từ host byte-order sang network byte-order (host to network long).
- `u16 ntohs(u16 x)` chuyển số nguyên 16 bit từ network byte-order sang host byte-order (network to host short).
- `u32 ntohl(u32 x)` chuyển số nguyên 32 bit từ network byte-order sang host byte-order (network to host long).

## netfilter

Netfilter là tên của giao diện kernel dùng để bắt các gói mạng nhằm sửa đổi/phân tích chúng, ví dụ filtering, NAT. Giao diện netfilter được user space sử dụng qua iptables. Trong Linux kernel, việc bắt gói bằng netfilter được thực hiện bằng cách gắn hook. Hook có thể được đặt ở nhiều vị trí khác nhau trên đường đi của gói mạng trong kernel, tùy nhu cầu.

Header cần include khi dùng netfilter là `linux/netfilter.h`. Một hook được định nghĩa bằng cấu trúc `struct nf_hook_ops`:

```c
struct nf_hook_ops {
	/* User fills in from here down. */
	nf_hookfn *hook;
	struct net_device *dev;
	void *priv;
	u_int8_t pf;
	unsigned int hooknum;
	/* Hooks are ordered in ascending priority. */
	int priority;
};
```

Trong đó:

- `pf` là kiểu gói (`PF_INET`, ...).
- `priority` là độ ưu tiên. Các độ ưu tiên được định nghĩa trong `uapi/linux/netfilter_ipv4.h`:

```c
enum nf_ip_hook_priorities {
	NF_IP_PRI_FIRST = INT_MIN,
	NF_IP_PRI_CONNTRACK_DEFRAG = -400,
	NF_IP_PRI_RAW = -300,
	NF_IP_PRI_SELINUX_FIRST = -225,
	NF_IP_PRI_CONNTRACK = -200,
	NF_IP_PRI_MANGLE = -150,
	NF_IP_PRI_NAT_DST = -100,
	NF_IP_PRI_FILTER = 0,
	NF_IP_PRI_SECURITY = 50,
	NF_IP_PRI_NAT_SRC = 100,
	NF_IP_PRI_SELINUX_LAST = 225,
	NF_IP_PRI_CONNTRACK_HELPER = 300,
	NF_IP_PRI_CONNTRACK_CONFIRM = INT_MAX,
	NF_IP_PRI_LAST = INT_MAX,
};
```

- `dev` là thiết bị, network interface, nơi muốn bắt gói.
- `hooknum` là loại hook được dùng.

Khi một gói bị bắt, cách xử lý được xác định bởi các trường `hooknum` và `hook`. Với IP, các loại hook được định nghĩa trong `linux/netfilter.h`:

```c
enum nf_inet_hooks {
	NF_INET_PRE_ROUTING,
	NF_INET_LOCAL_IN,
	NF_INET_FORWARD,
	NF_INET_LOCAL_OUT,
	NF_INET_POST_ROUTING,
	NF_INET_NUMHOOKS
};
```

- `hook` là handler được gọi khi bắt một gói mạng. Gói được truyền vào dưới dạng `struct sk_buff`. Trường `private` là thông tin riêng truyền cho handler.

Prototype của capture handler được định nghĩa bởi kiểu `nf_hookfn`:

```c
struct nf_hook_state {
	unsigned int hook;
	u_int8_t pf;
	struct net_device *in;
	struct net_device *out;
	struct sock *sk;
	struct net *net;
	int (*okfn)(struct net *, struct sock *, struct sk_buff *);
};

typedef unsigned int nf_hookfn(void *priv, struct sk_buff *skb,
			      const struct nf_hook_state *state);
```

Với hàm bắt gói `nf_hookfn`, tham số `priv` là thông tin riêng mà `struct nf_hook_ops` đã được khởi tạo cùng. `skb` là con trỏ tới gói mạng bị bắt. Dựa trên thông tin trong `skb`, ta đưa ra quyết định lọc gói.

Tham số `state` của hàm chứa thông tin trạng thái liên quan đến việc bắt gói, gồm interface đầu vào, interface đầu ra, độ ưu tiên và số hook. Độ ưu tiên và số hook hữu ích khi muốn cùng một hàm được gọi bởi nhiều hook.

Capture handler có thể trả về một trong các hằng số `NF_*`:

```c
/* Responses from hook functions. */
#define NF_DROP 0
#define NF_ACCEPT 1
#define NF_STOLEN 2
#define NF_QUEUE 3
#define NF_REPEAT 4
#define NF_STOP 5
#define NF_MAX_VERDICT NF_STOP
```

`NF_DROP` được dùng để lọc/bỏ qua một gói; `NF_ACCEPT` được dùng để chấp nhận gói và chuyển tiếp nó.

Đăng ký/hủy đăng ký hook được thực hiện bằng các hàm định nghĩa trong `linux/netfilter.h`:

```c
/* Function to register/unregister hook points. */
int nf_register_net_hook(struct net *net, const struct nf_hook_ops *ops);
void nf_unregister_net_hook(struct net *net, const struct nf_hook_ops *ops);
int nf_register_net_hooks(struct net *net, const struct nf_hook_ops *reg,
			  unsigned int n);
void nf_unregister_net_hooks(struct net *net, const struct nf_hook_ops *reg,
			     unsigned int n);
```

Chú ý: Trước Linux kernel 3.11-rc2 có một số hạn chế liên quan đến việc dùng các hàm trích xuất header từ `struct sk_buff` khi cấu trúc này được truyền làm tham số vào netfilter hook. IP header có thể lấy mọi lúc bằng `ip_hdr`, nhưng TCP và UDP header chỉ có thể lấy bằng `tcp_hdr` và `udp_hdr` với các gói đến từ bên trong hệ thống, không phải các gói nhận từ bên ngoài.

Trong trường hợp sau, bạn phải tự tính offset của header trong gói:

```c
// For TCP packets (iph->protocol == IPPROTO_TCP)
tcph = (struct tcphdr *)((__u32 *)iph + iph->ihl);

// For UDP packets (iph->protocol == IPPROTO_UDP)
udph = (struct udphdr *)((__u32 *)iph + iph->ihl);
```

Đoạn code này hoạt động trong mọi tình huống lọc, nên nên dùng nó thay cho các hàm truy cập header.

Ví dụ sử dụng netfilter hook:

```c
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

static unsigned int my_nf_hookfn(void *priv, struct sk_buff *skb,
				 const struct nf_hook_state *state)
{
	/* process packet */
	//...
	return NF_ACCEPT;
}

static struct nf_hook_ops my_nfho = {
	.hook = my_nf_hookfn,
	.hooknum = NF_INET_LOCAL_OUT,
	.pf = PF_INET,
	.priority = NF_IP_PRI_FIRST
};

int __init my_hook_init(void)
{
	return nf_register_net_hook(&init_net, &my_nfho);
}

void __exit my_hook_exit(void)
{
	nf_unregister_net_hook(&init_net, &my_nfho);
}

module_init(my_hook_init);
module_exit(my_hook_exit);
```

## netcat

Khi phát triển ứng dụng có code networking, một trong các công cụ được dùng nhiều nhất là netcat. Netcat còn được gọi là "Swiss-army knife for TCP/IP".

Netcat cho phép:

- Khởi tạo kết nối TCP.
- Chờ một kết nối TCP.
- Gửi và nhận gói UDP.
- Hiển thị traffic ở định dạng hexdump.
- Chạy một chương trình sau khi thiết lập kết nối, ví dụ shell.
- Đặt các tùy chọn đặc biệt trong gói được gửi.

Khởi tạo kết nối TCP:

```console
nc hostname port
```

Lắng nghe trên một TCP port:

```console
nc -l -p port
```

Gửi và nhận gói UDP bằng cách thêm tùy chọn dòng lệnh `-u`.

Ghi chú: Lệnh là `nc`; thường `netcat` là alias của lệnh này. Có nhiều cài đặt khác nhau của netcat và một số cài đặt có tham số hơi khác với bản classic. Chạy `man nc` hoặc `nc -h` để xem cách sử dụng.

## Đọc thêm

1. Understanding Linux Network Internals
2. Linux IP networking
3. The TUX Web Server
4. Beej's Guide to Network Programming Using Internet Sockets
5. Kernel Korner - Network Programming in the Kernel
6. Hacking the Linux Kernel Network Stack
7. The netfilter.org project
8. A Deep Dive Into Iptables and Netfilter Architecture
9. Linux Foundation Networking Page

## Bài tập

Quan trọng: Cần đảm bảo hỗ trợ `netfilter` đang bật trong kernel. Nó được bật bằng `CONFIG_NETFILTER`.

Để kích hoạt, chạy `make menuconfig` trong thư mục `linux` và kiểm tra tùy chọn `Network packet filtering framework (Netfilter)` trong `Networking support -> Networking options`. Nếu chưa bật, hãy bật nó ở dạng builtin, không phải external module; tùy chọn phải được đánh dấu bằng `*`.

### 1. Hiển thị gói trong kernel space

Viết một kernel module hiển thị địa chỉ nguồn và port nguồn của các gói TCP khởi tạo kết nối đi ra ngoài.

Bắt đầu từ code trong `1-2-netfilter` và điền các vùng được đánh dấu `TODO 1`, dựa trên các ghi chú dưới đây. Bạn cần đăng ký một netfilter hook kiểu `NF_INET_LOCAL_OUT` như đã giải thích trong phần netfilter.

`struct sk_buff` cho phép truy cập các packet header bằng các hàm cụ thể. Hàm `ip_hdr` trả về IP header dưới dạng con trỏ tới `struct iphdr`. Hàm `tcp_hdr` trả về TCP header dưới dạng con trỏ tới `struct tcphdr`.

Diagram TCP giải thích cách thiết lập kết nối TCP. Gói khởi tạo kết nối có cờ `SYN` được bật trong TCP header và cờ `ACK` được tắt.

Ghi chú: Để hiển thị địa chỉ IP nguồn, dùng format `%pI4` của hàm `printk`. Ví dụ:

```c
printk("IP address is %pI4\n", &iph->saddr);
```

Khi dùng format `%pI4`, tham số truyền cho `printk` là một con trỏ. Vì vậy cần dùng `&iph->saddr` thay vì `iph->saddr`.

Port TCP nguồn trong TCP header nằm ở dạng network byte-order. Đọc phần chuyển đổi byte order và dùng `ntohs` để chuyển đổi.

Để test, dùng file `1-2-netfilter/user/test-1.sh`. Test tạo một kết nối tới localhost; kết nối này sẽ bị kernel module chặn/bắt và hiển thị. Script được copy vào máy ảo bằng lệnh `make copy` chỉ khi nó được đánh dấu executable. Script dùng công cụ `netcat` biên dịch tĩnh nằm trong `skels/networking/netcat`; chương trình này phải có quyền thực thi.

Sau khi chạy checker, output nên tương tự:

```console
# ./test-1.sh
[ 229.783512] TCP connection initiated from 127.0.0.1:44716
Should show up in filter.
Check dmesg output.
```

### 2. Lọc theo địa chỉ đích

Mở rộng module từ bài 1 để có thể chỉ định địa chỉ đích thông qua lời gọi ioctl `MY_IOCTL_FILTER_ADDRESS`. Bạn chỉ hiển thị các gói có địa chỉ đích đã chỉ định. Để giải quyết bài này, điền các vùng được đánh dấu `TODO 2` và làm theo đặc tả dưới đây.

Để cài đặt ioctl routine, cần điền hàm `my_ioctl`. Xem lại phần ioctl. Địa chỉ gửi từ user space nằm ở dạng network byte-order, nên **không cần** chuyển đổi.

Ghi chú: Địa chỉ IP gửi qua `ioctl` được gửi theo địa chỉ, không phải theo giá trị. Địa chỉ phải được lưu trong biến `ioctl_set_addr`. Khi copy, dùng `copy_from_user`.

Để so sánh địa chỉ, điền hàm `test_daddr`. Địa chỉ ở network byte-order được dùng trực tiếp không cần chuyển đổi; nếu bằng nhau từ trái sang phải thì khi đảo ngược cũng vẫn bằng nhau.

Hàm `test_daddr` phải được gọi từ netfilter hook để hiển thị các gói khởi tạo kết nối có địa chỉ đích là địa chỉ được gửi qua ioctl. Gói khởi tạo kết nối có cờ `SYN` bật và cờ `ACK` tắt.

Cần kiểm tra hai thứ:

- Các cờ TCP.
- Địa chỉ đích của gói, dùng `test_addr`.

Để test, dùng script `1-2-netfilter/user/test-2.sh`. Script này cần biên dịch file `1-2-netfilter/user/test.c` thành executable test. Việc biên dịch được thực hiện tự động trên hệ thống vật lý khi chạy `make build`. Script test chỉ được copy vào máy ảo nếu nó được đánh dấu executable.

Script dùng `netcat` biên dịch tĩnh trong `skels/networking/netcat`; executable này phải có quyền thực thi.

Sau khi chạy checker, output nên tương tự:

```console
# ./test-2.sh
[ 797.673535] TCP connection initiated from 127.0.0.1:44721
Should show up in filter.
Should NOT show up in filter.
Check dmesg output.
```

Test yêu cầu lọc gói đầu tiên cho địa chỉ IP `127.0.0.1`, sau đó cho địa chỉ IP `127.0.0.2`. Gói khởi tạo kết nối đầu tiên, tới `127.0.0.1`, được filter bắt và hiển thị; gói thứ hai, tới `127.0.0.2`, không bị bắt.

### 3. Lắng nghe trên TCP socket

Viết một kernel module tạo TCP socket lắng nghe kết nối trên port `60000` của loopback interface, trong `init_module`.

Bắt đầu từ code trong `3-4-tcp-sock` và điền các vùng được đánh dấu `TODO 1`, dựa trên các ghi chú dưới đây.

Đọc phần thao tác trên `struct socket` và `struct proto_ops`. Socket `sock` là `server socket` và phải được đưa vào trạng thái lắng nghe. Tức là phải áp dụng các thao tác `bind` và `listen` lên socket. Với bản tương đương của `bind` và `listen` trong kernel space, cần gọi `sock->ops->...`; ví dụ có thể gọi `sock->ops->bind`, `sock->ops->listen`.

Ghi chú: Xem cách gọi `sock->ops->bind` và `sock->ops->listen` trong handler của system call `sys_bind` và `sys_listen`. Tìm các system call handler trong file `net/socket.c` của cây mã nguồn Linux kernel.

Ghi chú: Với tham số thứ hai của lời gọi `listen` (backlog), dùng `LISTEN_BACKLOG`.

Nhớ giải phóng socket trong hàm exit của module và trong các vùng được đánh dấu error label; dùng `sock_release`.

Để test, chạy script `3-4-tcp_sock/test-3.sh`. Script được copy vào máy ảo bằng `make copy` chỉ khi nó được đánh dấu executable. Sau khi chạy test, sẽ có một TCP socket lắng nghe kết nối trên port `60000`.

### 4. Chấp nhận kết nối trong kernel space

Mở rộng module từ bài trước để cho phép một kết nối bên ngoài. Không cần gửi thông điệp nào, chỉ cần accept kết nối mới.

Điền các vùng được đánh dấu `TODO 2`. Đọc phần thao tác trên `struct socket` và `struct proto_ops`. Để xem bản tương đương của `accept` trong kernel space, xem handler system call `sys_accept4`. Làm theo cài đặt `lnet_sock_accept` và cách lời gọi `sock->ops->accept` được dùng. Dùng `0` cho tham số gần cuối (`flags`) và `true` cho tham số cuối (`kern`).

Ghi chú: Tìm các system call handler trong file `net/socket.c` của cây mã nguồn Linux kernel.

Ghi chú: Socket mới (`new_sock`) phải được tạo bằng hàm `sock_create_lite`, sau đó các thao tác của nó phải được cấu hình bằng:

```console
newsock->ops = sock->ops;
```

In địa chỉ và port của socket đích. Để lấy peer name của socket, tức địa chỉ của nó, tham khảo handler system call `sys_getpeername`.

Ghi chú: Tham số đầu tiên của hàm `sock->ops->getname` sẽ là connection socket, tức `new_sock`, socket được khởi tạo bởi lời gọi `accept`. Tham số cuối của hàm `sock->ops->getname` là `1`, nghĩa là muốn biết endpoint/peer, remote end.

Hiển thị địa chỉ peer, biến `raddr`, bằng macro `print_sock_address` được định nghĩa trong file.

Giải phóng socket mới tạo sau khi accept kết nối trong hàm exit của module và sau error label. Sau khi thêm code `accept` vào hàm khởi tạo module, thao tác `insmod` sẽ bị khóa cho đến khi có kết nối được thiết lập. Có thể mở khóa bằng `netcat` trên port đó. Do đó, script test của bài trước sẽ không còn hoạt động.

Để test, chạy script `3-4-tcp_sock/test-4.sh`. Script được copy vào máy ảo bằng `make copy` chỉ khi nó được đánh dấu executable. Sẽ không có gì đặc biệt được hiển thị trong kernel buffer. Thành công của test được xác định bởi việc thiết lập kết nối. Sau đó dùng `Ctrl+c` để dừng script test, rồi có thể remove kernel module.

### 5. UDP socket sender

Viết một kernel module tạo UDP socket và gửi thông điệp từ macro `MY_TEST_MESSAGE` qua socket tới địa chỉ loopback trên port `60001`.

Bắt đầu từ code trong `5-udp-sock`. Đọc phần thao tác trên `struct socket` và `struct proto_ops`. Để xem cách gửi thông điệp trong kernel space, xem handler system call `sys_send` hoặc phần gửi/nhận thông điệp.

Gợi ý: Trường `msg_name` của `struct msghdr` phải được khởi tạo bằng địa chỉ đích, con trỏ tới `struct sockaddr`, và trường `msg_namelen` bằng kích thước địa chỉ. Khởi tạo trường `msg_flags` của `struct msghdr` bằng `0`. Khởi tạo `msg_control` và `msg_controllen` lần lượt bằng `NULL` và `0`.

Dùng `kernel_sendmsg` để gửi thông điệp. Các tham số truyền thông điệp được lấy từ kernel space. Ép kiểu con trỏ `struct iovec` thành con trỏ `struct kvec` trong lời gọi `kernel_sendmsg`.

Gợi ý: Hai tham số cuối của `kernel_sendmsg` là `1`, số lượng I/O vector, và `len`, kích thước thông điệp.

Để test, dùng file `test-5.sh`. Script được copy vào máy ảo bằng lệnh `make copy` chỉ khi nó được đánh dấu executable. Script dùng công cụ `netcat` biên dịch tĩnh nằm trong `skels/networking/netcat`; executable này phải có quyền thực thi.

Nếu cài đặt đúng, chạy script `test-5.sh` sẽ làm thông điệp `kernelsocket` được hiển thị như output dưới đây:

```console
/root # ./test-5.sh
+ pid=1059
+ sleep 1
+ nc -l -u -p 60001
+ insmod udp_sock.ko
kernelsocket
+ rmmod udp_sock
+ kill 1059
```
