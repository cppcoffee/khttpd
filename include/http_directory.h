#ifndef _LIST_DIR_H
#define _LIST_DIR_H

#include "http_request.h"

int http_response_redirect_directory(struct http_worker *worker);
int http_response_list_directory(struct http_worker *worker);

#endif
