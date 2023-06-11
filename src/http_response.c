#include <linux/kernel.h>
#include <linux/fs.h>

#include "khttpd.h"
#include "socket.h"
#include "http_response.h"

#define READ_BUFFER_SIZE 8192

int http_response_file(struct http_worker *worker)
{
	int rc;
	void *buf;
	u64 pos, file_size;
	ssize_t nbytes;
	struct file *filp;
	const size_t buf_size = READ_BUFFER_SIZE;
	const char *file_path = worker->absolute_path.data;

	filp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("Failed to open file %s, err(%ld)", file_path,
		       PTR_ERR(filp));

		http_response_internal_server_error(worker);
		return PTR_ERR(filp);
	}

	file_size = i_size_read(file_inode(filp));

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (buf == NULL) {
		http_response_internal_server_error(worker);

		filp_close(filp, NULL);
		return -ENOMEM;
	}

	rc = http_response_header(worker, HTTP_OK, file_size);
	if (rc < 0) {
		rc = -1;
		goto out;
	}

	for (pos = 0; pos < file_size; pos += nbytes) {
		nbytes = kernel_read(filp, buf, buf_size, &pos);
		if (nbytes < 0) {
			// internal error, close connect
			worker->keepalive = 0;

			http_response_internal_server_error(worker);
			break;
		}

		if (nbytes == 0) {
			break;
		}

		nbytes = socket_send(worker->socket, buf, nbytes);
		if (nbytes < 0) {
			// internal error, close connect
			worker->keepalive = 0;
			http_response_internal_server_error(worker);
			break;
		}
	}

	rc = 0;

out:

	kfree(buf);
	filp_close(filp, NULL);

	return rc;
}

int http_response_header(struct http_worker *worker, const char *status_line,
			 int content_length)
{
	int n, pos;
	char *p;
	char buf[64];
	size_t bytes;
	struct kvec vec[5];
	const int count = ARRAY_SIZE(vec);

	pos = 0;

	KVEC_PUSH_CSTRING(vec, count, &pos, status_line);

	// Server header
	KVEC_PUSH_CSTRING(vec, count, &pos, "Server: " SERVER_NAME CRLF);

	// Connection header
	p = worker->keepalive ? "Connection: keep-alive" CRLF :
				"Connection: close" CRLF;
	KVEC_PUSH_CSTRING(vec, count, &pos, p);

	// Content-Length header
	if (content_length >= 0) {
		n = snprintf(buf, sizeof(buf), "Content-Length: %d" CRLF,
			     content_length);
		KVEC_PUSH(vec, count, &pos, buf, n);

	} else {
		// Transfer-Encoding: chunked
		KVEC_PUSH_CSTRING(vec, count, &pos,
				  "Transfer-Encoding: chunked" CRLF);
	}

	// end of headers
	KVEC_PUSH_CSTRING(vec, count, &pos, CRLF);

	bytes = socket_send_vec(worker->socket, vec, pos);
	if (bytes < 0) {
		pr_err("Failed to send response: %ld\n", bytes);
		return bytes;
	}

	pr_debug("socket send %ld bytes.\n", bytes);

	return bytes;
}

int http_header_vec_push(struct kvec *vec, size_t count, int *pos, void *ptr,
			 size_t len)
{
	if (*pos >= count) {
		pr_err("kvec not enough, pos=%d, count=%ld\n", *pos, count);
		return -1;
	}

	vec[*pos].iov_base = ptr;
	vec[*pos].iov_len = len;

	*pos += 1;

	return 0;
}

int http_response_bad_request(struct http_worker *worker)
{
	return http_response_header(worker, HTTP_BAD_REQUEST, 0);
}

int http_response_method_not_allowed(struct http_worker *worker)
{
	return http_response_header(worker, HTTP_METHOD_NOT_ALLOWED, 0);
}

int http_response_internal_server_error(struct http_worker *worker)
{
	return http_response_header(worker, HTTP_INTERNAL_SERVER_ERROR, 0);
}

int http_response_forbidden(struct http_worker *worker)
{
	return http_response_header(worker, HTTP_FORBIDDEN, 0);
}

int http_response_not_found(struct http_worker *worker)
{
	return http_response_header(worker, HTTP_NOT_FOUND, 0);
}
