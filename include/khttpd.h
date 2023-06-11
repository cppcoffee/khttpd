#ifndef _KHTTPD_H_
#define _KHTTPD_H_

#include "socket.h"
#include "http_string.h"
#include "http_request.h"
#include "http_chunk.h"
#include "http_response.h"
#include "http_directory.h"

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

#define LOG_PRINT(fmt, args...) printk(KERN_NOTICE fmt, ##args)

#define MODULE_NAME "khttpd"

#define VERSION "0.0.1"
#define SERVER_NAME MODULE_NAME "/" VERSION

// Parameter default values
#define DEFAULT_LISTEN_ADDR "0.0.0.0"
#define DEFAULT_PORT 8000
#define DEFAULT_BACKLOG 1024
#define DEFAULT_ROOT_DIR "/var/www/html"

int http_uri_to_path(struct string *uri, struct string *dst, size_t max_len);

#endif
