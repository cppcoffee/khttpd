#ifndef _DAEMON_H
#define _DAEMON_H

#include <linux/net.h>

#include "http_request.h"

int http_daemon_start(struct socket *socket, const char *root_dir);
void http_daemon_stop(void);
void free_worker(struct http_worker *worker);

#endif
