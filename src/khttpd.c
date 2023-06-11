#include <linux/module.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/namei.h>
#include <linux/inet.h>

#include "khttpd.h"
#include "daemon.h"

static ushort port = DEFAULT_PORT;
module_param(port, ushort, S_IRUGO);

ushort backlog = DEFAULT_BACKLOG;
module_param(backlog, ushort, S_IRUGO);

char *root_dir = DEFAULT_ROOT_DIR;
module_param(root_dir, charp, S_IRUGO);
MODULE_PARM_DESC(root_dir, "Sets the root directory for requests.");

char *listen_addr = DEFAULT_LISTEN_ADDR;
module_param(listen_addr, charp, S_IRUGO);

size_t root_dir_len;
struct socket *listen_socket;

static void socket_close(struct socket *socket)
{
	kernel_sock_shutdown(socket, SHUT_RDWR);
	sock_release(socket);
}

static void socket_set_opts(struct socket *socket)
{
	sock_set_reuseaddr(socket->sk);
	sock_set_reuseport(socket->sk);

	sock_set_rcvbuf(socket->sk, 1024 * 1024);
}

static int socket_listen(ushort port, ushort backlog, struct socket **res)
{
	int err;
	struct socket *socket;
	struct sockaddr_in sin = { 0 };

	if (in4_pton(listen_addr, strlen(listen_addr), (u8 *)&sin.sin_addr, -1,
		     NULL) == 0) {
		pr_err("invalid listen address: %s\n", listen_addr);
		return -EINVAL;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	err = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &socket);
	if (err != 0) {
		pr_err("sock_create failed, err=%d\n", err);
		return err;
	}

	socket_set_opts(socket);

	err = kernel_bind(socket, (struct sockaddr *)&sin, sizeof(sin));
	if (err != 0) {
		pr_err("kernel_bind failed, err=%d\n", err);
		goto fail;
	}

	err = kernel_listen(socket, backlog);
	if (err != 0) {
		pr_err("kernel_listen failed, err=%d\n", err);
		goto fail;
	}

	*res = socket;

	return 0;

fail:
	sock_release(socket);

	return err;
}

static int path_is_directory(const char *p)
{
	int rc, mode;
	struct path path_struct;

	rc = kern_path(p, LOOKUP_FOLLOW, &path_struct);
	if (rc != 0) {
		DEBUG_PRINT(KERN_INFO, "Not found path: %s, err(%d)\n", p, rc);
		return -1;
	}

	mode = d_inode(path_struct.dentry)->i_mode;
	if (!S_ISDIR(mode)) {
		DEBUG_PRINT(KERN_WARN, "Path not a directory: %s\n", p);
		return -1;
	}

	path_put(&path_struct);

	return 0;
}

static int __init khttpd_init(void)
{
	int err;

	if (path_is_directory(root_dir) != 0) {
		return -1;
	}

	root_dir_len = strlen(root_dir);
	if (root_dir_len >= PATH_MAX) {
		pr_err("root directory too long, got length(%zu) >= PATH_MAX(%d)\n",
		       root_dir_len, PATH_MAX);
		return -EINVAL;
	}

	pr_info("use root directory: %s\n", root_dir);

	err = socket_listen(port, backlog, &listen_socket);
	if (err != 0) {
		pr_err("socket_listen failed, err=%d\n", err);
		return err;
	}

	err = http_daemon_start(listen_socket, root_dir);
	if (err != 0) {
		pr_err("run_http_daemon failed, err=%d\n", err);
		sock_release(listen_socket);
		return err;
	}

	pr_info("khttpd loaded\n");

	return 0;
}

static void __exit khttpd_exit(void)
{
	http_daemon_stop();

	socket_close(listen_socket);

	pr_info("khttpd unloaded\n");
}

module_init(khttpd_init);
module_exit(khttpd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kernel static HTTP server");
MODULE_VERSION("0.1");
