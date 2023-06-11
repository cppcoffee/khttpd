#ifndef _HTTP_CHUNK_H
#define _HTTP_CHUNK_H

#include "http_request.h"

int http_response_chunk_vec(struct http_worker *worker, struct kvec *vec,
			    int count);
int http_response_chunk_data(struct http_worker *worker, void *data,
			     size_t len);
int http_response_chunk_end(struct http_worker *worker);

#endif
