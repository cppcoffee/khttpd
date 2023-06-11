#include <linux/uaccess.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/dirent.h>

#include "khttpd.h"

#define HTML_HEAD_BEGIN "<html><head>"
#define HTML_TITLE_BEGIN "<title>Directory listing for "
#define HTML_TITLE_END "</title>"
#define HTML_HEAD_END "</head>"

#define HTTP_BODY_H1_BEGIN "<body><h1>Directory listing for "
#define HTTP_BODY_H1_END "</h1><hr>"

#define HTTP_BODY_LIST_BEGIN "<ul>"
#define HTTP_BODY_LI_HREF_BEGIN "<li><a href=\""
#define HTTP_BODY_LI_HREF_END "\">"
#define HTML_BODY_LI_END "</a></li>"

#define HTML_BODY_END "</ul><hr></body></html>"

// defined in main.c
extern size_t root_dir_len;

struct read_dirent_context {
	struct dir_context ctx;
	struct http_worker *worker;
};

static int http_directory_list_begin(struct http_worker *worker);
static int http_directory_list_end(struct http_worker *worker);
static int iterate_dir_callback(struct dir_context *ctx, const char *name,
				int name_len, loff_t offset, u64 inode_no,
				unsigned int d_type);

int http_response_redirect_directory(struct http_worker *worker)
{
	char *p;
	int pos, bytes;
	struct string path;
	struct kvec vec[7];
	size_t count = ARRAY_SIZE(vec);

	pos = 0;

	path.data = worker->absolute_path.data + root_dir_len;
	path.len = worker->absolute_path.len - root_dir_len;

	KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_FOUND);

	// Server header
	KVEC_PUSH_CSTRING(vec, count, &pos, "Server: " SERVER_NAME CRLF);

	// Connection header
	p = worker->keepalive ? "Connection: keep-alive" CRLF :
				"Connection: close" CRLF;
	KVEC_PUSH_CSTRING(vec, count, &pos, p);

	KVEC_PUSH_CSTRING(vec, count, &pos, "Location: ");
	KVEC_PUSH(vec, count, &pos, path.data, path.len);
	KVEC_PUSH(vec, count, &pos, "/", 1);

	// end of headers
	KVEC_PUSH_CSTRING(vec, count, &pos, CRLF);

	bytes = socket_send_vec(worker->socket, vec, pos);
	if (bytes < 0) {
		pr_err("Failed to send response: %d\n", bytes);
		return bytes;
	}

	pr_debug("socket send %d bytes.\n", bytes);

	return bytes;
	return 0;
}

int http_response_list_directory(struct http_worker *worker)
{
	int rc;
	struct path dir_path;
	struct file *dir_filp;
	const char *path = worker->absolute_path.data;
	struct read_dirent_context context = {
		.worker = worker,
		.ctx.actor = iterate_dir_callback,
	};

	rc = -1;

	// Resolve the given path to a path struct
	rc = kern_path(path, LOOKUP_DIRECTORY, &dir_path);
	if (rc) {
		printk(KERN_ERR "Cannot resolve the directory: %s\n", path);
		return rc;
	}

	dir_filp = dentry_open(&dir_path, O_RDONLY, current_cred());
	if (IS_ERR(dir_filp)) {
		pr_err(KERN_ERR "Cannot open the directory: %s\n", path);

		http_response_internal_server_error(worker);

		path_put(&dir_path);
		return PTR_ERR(dir_filp);
	}

	rc = http_response_header(worker, HTTP_OK, -1);
	if (rc <= 0) {
		worker->keepalive = 0;
		goto out;
	}

	rc = http_directory_list_begin(worker);
	if (rc <= 0) {
		worker->keepalive = 0;
		goto out;
	}

	rc = iterate_dir(dir_filp, &context.ctx);
	if (rc < 0) {
		worker->keepalive = 0;
		goto out;
	}

	rc = http_directory_list_end(worker);
	if (rc <= 0) {
		worker->keepalive = 0;
		goto out;
	}

	rc = 0;

out:

	path_put(&dir_path);
	filp_close(dir_filp, NULL);

	return rc;
}

static int http_directory_list_begin(struct http_worker *worker)
{
	struct string path;
	struct kvec vec[8];
	int pos = 0;
	size_t count = ARRAY_SIZE(vec);

	path.data = worker->absolute_path.data + root_dir_len;
	path.len = worker->absolute_path.len - root_dir_len;

	KVEC_PUSH_CSTRING(vec, count, &pos, HTML_HEAD_BEGIN);
	KVEC_PUSH_CSTRING(vec, count, &pos, HTML_TITLE_BEGIN);
	KVEC_PUSH(vec, count, &pos, path.data, path.len);
	KVEC_PUSH_CSTRING(vec, count, &pos, HTML_TITLE_END);
	KVEC_PUSH_CSTRING(vec, count, &pos, HTML_HEAD_END);

	KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_H1_BEGIN);
	KVEC_PUSH(vec, count, &pos, path.data, path.len);
	KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_H1_END);

	return http_response_chunk_vec(worker, vec, pos);
}

static int http_directory_list_end(struct http_worker *worker)
{
	int rc;
	struct kvec iov;

	iov.iov_base = HTML_BODY_END;
	iov.iov_len = sizeof(HTML_BODY_END) - 1;

	rc = http_response_chunk_vec(worker, &iov, 1);
	if (rc < 0) {
		return rc;
	}

	return http_response_chunk_end(worker);
}

static int iterate_dir_callback(struct dir_context *ctx, const char *name,
				int name_len, loff_t offset, u64 inode_no,
				unsigned int d_type)
{
	int pos;
	struct string path;
	struct kvec vec[8];
	size_t count = ARRAY_SIZE(vec);
	struct read_dirent_context *context =
		container_of(ctx, struct read_dirent_context, ctx);
	struct http_worker *worker = context->worker;

	pos = 0;

	if ((name_len == 1 && name[0] == '.') ||
	    (name_len == 2 && strncmp(name, "..", 2) == 0)) {
		return 0;
	}

	pr_info("name: %.*s\n", name_len, name);

	if (d_type == DT_DIR) {
		path.data = worker->absolute_path.data + root_dir_len;
		path.len = worker->absolute_path.len - root_dir_len;

		KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_LI_HREF_BEGIN);
		KVEC_PUSH(vec, count, &pos, path.data, path.len);
		KVEC_PUSH(vec, count, &pos, (void *)name, name_len);
		KVEC_PUSH_CSTRING(vec, count, &pos, "/");
		KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_LI_HREF_END);
		KVEC_PUSH(vec, count, &pos, (void *)name, name_len);
		KVEC_PUSH_CSTRING(vec, count, &pos, "/");
		KVEC_PUSH_CSTRING(vec, count, &pos, HTML_BODY_LI_END);

	} else {
		KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_LI_HREF_BEGIN);
		KVEC_PUSH(vec, count, &pos, (void *)name, name_len);
		KVEC_PUSH_CSTRING(vec, count, &pos, HTTP_BODY_LI_HREF_END);
		KVEC_PUSH(vec, count, &pos, (void *)name, name_len);
		KVEC_PUSH_CSTRING(vec, count, &pos, HTML_BODY_LI_END);
	}

	if (http_response_chunk_vec(worker, vec, pos) < 0) {
		return 1;
	}

	return 0;
}
