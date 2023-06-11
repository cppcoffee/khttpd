#ifndef _WORKER_H
#define _WORKER_H

#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/net.h>

#include "http_parser.h"

#define MAX_REQUEST_HEADERS 20

struct http_worker {
	struct work_struct work;
	struct socket *socket;
	struct kmem_cache *worker_cachep;

	// http parser
	struct string method;
	struct string path;
	int minor_version;
	struct http_header headers[MAX_REQUEST_HEADERS];
	size_t num_headers;

	// file path
	struct string absolute_path;
	struct string root_dir;
	size_t max_path_length;

	// flags
	unsigned long keepalive : 1;
	unsigned long is_dir : 1;
};

void http_request_handler(struct work_struct *work);

#endif
