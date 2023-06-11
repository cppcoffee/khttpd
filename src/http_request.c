#include <linux/tcp.h>
#include <linux/string.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/dcache.h>

#include "khttpd.h"
#include "daemon.h"
#include "socket.h"
#include "http_parser.h"
#include "http_request.h"
#include "http_response.h"
#include "http_directory.h"

#define RECV_BUFFER_SIZE 4096

typedef int (*http_phase_handler_pt)(struct http_worker *worker);

static int http_pre_access_handler(struct http_worker *worker);
static int http_access_handler(struct http_worker *worker);
static int http_content_handler(struct http_worker *worker);
static int http_log_handler(struct http_worker *worker);

static http_phase_handler_pt http_phase_handlers[] = {
	http_pre_access_handler,
	http_access_handler,
	http_content_handler,
};

void http_request_handler(struct work_struct *ctx)
{
	struct http_worker *worker = (struct http_worker *)ctx;
	struct socket *socket = worker->socket;
	http_phase_handler_pt ph;
	size_t buflen, prevbuflen;
	int i, n, rc;
	char *buf;

	// TODO: use mempool
	buf = kmalloc(RECV_BUFFER_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("kmalloc error, no memory\n");
		goto fail;
	}

	buflen = prevbuflen = 0;

	for (;;) {
		n = socket_recv(socket, buf + buflen,
				RECV_BUFFER_SIZE - buflen);
		if (n < 0) {
			pr_err("socket_recv error: %d\n", n);
			break;
		}

		if (n == 0) {
			http_response_bad_request(worker);
			break;
		}

		prevbuflen = buflen;
		buflen += n;

		worker->num_headers = MAX_REQUEST_HEADERS;

		rc = http_request_parse(buf, buflen, &worker->method,
					&worker->path, &worker->minor_version,
					worker->headers, &worker->num_headers,
					prevbuflen);

		switch (rc) {
		case -1:
			http_response_bad_request(worker);
			goto done;

		case -2:
			// incomplete
			continue;

		default:
			break;
		}

		for (i = 0; i < ARRAY_SIZE(http_phase_handlers); i++) {
			ph = http_phase_handlers[i];
			if (ph(worker) != 0) {
				break;
			}
		}

		http_log_handler(worker);

		if (!worker->keepalive) {
			break;
		}

		// next request
		buflen = prevbuflen = 0;
	}

done:

	kfree(buf);

fail:

	free_worker(worker);
}

static int http_pre_access_handler(struct http_worker *worker)
{
	static struct string method_get = STRING_INIT("GET");
	static struct string header = STRING_INIT("Connection");
	static struct string value = STRING_INIT("keep-alive");

	int i;

	worker->keepalive = 0;

	for (i = 0; i < worker->num_headers; i++) {
		if (string_casecmp(&worker->headers[i].name, &header) == 0 &&
		    string_casecmp(&worker->headers[i].value, &value) == 0) {
			worker->keepalive = 1;
			break;
		}
	}

	if (string_cmp(&worker->method, &method_get) != 0) {
		http_response_method_not_allowed(worker);
		return -1;
	}

	return 0;
}

static int http_access_handler(struct http_worker *worker)
{
	int rc;
	umode_t mode;
	struct path path_struct;

	if (string_copy(&worker->absolute_path, &worker->root_dir,
			worker->max_path_length) != 0) {
		http_response_internal_server_error(worker);
		return -1;
	}

	if (http_uri_to_path(&worker->path, &worker->absolute_path,
			     worker->max_path_length) != 0) {
		http_response_forbidden(worker);
		return -1;
	}

	pr_info("http uri to path: %.*s\n", (u32)worker->absolute_path.len,
		worker->absolute_path.data);

	if (string_starts_with(&worker->absolute_path, &worker->root_dir) !=
	    0) {
		http_response_forbidden(worker);
		return -1;
	}

	rc = kern_path(worker->absolute_path.data, LOOKUP_FOLLOW, &path_struct);
	if (rc != 0) {
		http_response_not_found(worker);
		return -1;
	}

	mode = d_inode(path_struct.dentry)->i_mode;
	worker->is_dir = S_ISDIR(mode);

	path_put(&path_struct);

	return 0;
}

// sending response to client
static int http_content_handler(struct http_worker *worker)
{
	char *last;

	if (worker->is_dir) {
		// redirect to directory
		last = worker->absolute_path.data + worker->absolute_path.len;
		if (*(last - 1) != '/') {
			return http_response_redirect_directory(worker);
		}

		return http_response_list_directory(worker);

	} else {
		return http_response_file(worker);
	}
}

static int http_log_handler(struct http_worker *worker)
{
	// TODO: add log

	return 0;
}
