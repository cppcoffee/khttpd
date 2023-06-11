#ifndef _HTTP_RESPONSE_H
#define _HTTP_RESPONSE_H

#include "http_request.h"

#define CRLF "\r\n"

// 2xx
#define HTTP_OK "HTTP/1.1 200 OK" CRLF
#define HTTP_PARTIAL_CONTENT "HTTP/1.1 206 Partial Content" CRLF

// 302
#define HTTP_FOUND "HTTP/1.1 302 Found" CRLF

// 4xx
#define HTTP_BAD_REQUEST "HTTP/1.1 400 Bad Request" CRLF
#define HTTP_FORBIDDEN "HTTP/1.1 403 Forbidden" CRLF
#define HTTP_NOT_FOUND "HTTP/1.1 404 Not Found" CRLF
#define HTTP_METHOD_NOT_ALLOWED "HTTP/1.1 405 Method Not Allowed" CRLF

// 5xx
#define HTTP_INTERNAL_SERVER_ERROR "HTTP/1.1 500 Internal Server Error" CRLF

int http_response_header(struct http_worker *worker, const char *status_line,
			 int content_length);
int http_response_file(struct http_worker *worker);

// simplify the response functions
int http_response_bad_request(struct http_worker *worker);
int http_response_method_not_allowed(struct http_worker *worker);
int http_response_forbidden(struct http_worker *worker);
int http_response_not_found(struct http_worker *worker);
int http_response_internal_server_error(struct http_worker *worker);

// util functions
int http_header_vec_push(struct kvec *vec, size_t count, int *pos, void *ptr,
			 size_t len);

#define KVEC_PUSH(vec, count, pos, ptr, size)                                \
	do {                                                                 \
		if (http_header_vec_push(vec, count, pos, ptr, size) != 0) { \
			return -1;                                           \
		}                                                            \
	} while (0)

#define KVEC_PUSH_CSTRING(vec, count, pos, pstr) \
	KVEC_PUSH(vec, count, pos, (void *)pstr, strlen(pstr))

#endif
