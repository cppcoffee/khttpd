#include <linux/net.h>
#include <linux/tcp.h>

#include "socket.h"

int socket_recv(struct socket *sock, char *buf, size_t size)
{
	struct kvec iov = { .iov_base = (void *)buf, .iov_len = size };
	struct msghdr msg = { .msg_name = 0,
			      .msg_namelen = 0,
			      .msg_control = NULL,
			      .msg_controllen = 0,
			      .msg_flags = 0 };

	return kernel_recvmsg(sock, &msg, &iov, 1, size, msg.msg_flags);
}

int socket_send(struct socket *sock, const char *buf, size_t size)
{
	struct kvec iov;
	int n, bytes;
	struct msghdr msg = { .msg_name = NULL,
			      .msg_namelen = 0,
			      .msg_control = NULL,
			      .msg_controllen = 0,
			      .msg_flags = 0 };

	bytes = 0;

	while (bytes < size) {
		iov.iov_base = (void *)((char *)buf + bytes);
		iov.iov_len = size - bytes;

		n = kernel_sendmsg(sock, &msg, &iov, 1, iov.iov_len);
		if (n < 0) {
			pr_err("kernel_sendmsg error: %d\n", n);
			break;
		}

		bytes += n;
	}

	return bytes;
}

int socket_send_vec(struct socket *sock, struct kvec *vec, size_t count)
{
	int i, n, bytes;
	size_t total;
	struct msghdr msg = { .msg_name = NULL,
			      .msg_namelen = 0,
			      .msg_control = NULL,
			      .msg_controllen = 0,
			      .msg_flags = 0 };

	i = bytes = 0;

	total = vec_bytes(vec, count);

	while (bytes < total) {
		n = kernel_sendmsg(sock, &msg, vec + i, count - i,
				   total - bytes);
		if (n < 0) {
			pr_err("kernel_sendmsg error: %d\n", n);
			break;
		}

		bytes += n;

		while (n >= vec[i].iov_len) {
			n -= vec[i].iov_len;
			i += 1;
		}

		if (n > 0) {
			vec[i].iov_base += n;
			vec[i].iov_len -= n;
		}
	}

	return bytes;
}

size_t vec_bytes(struct kvec *vec, size_t count)
{
	int i;
	size_t res = 0;

	for (i = 0; i < count; i++) {
		res += vec[i].iov_len;
	}

	return res;
}
