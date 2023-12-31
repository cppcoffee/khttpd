/*
 * Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase,
 *                         Shigeo Mitsunari
 *
 * The software is licensed under either the MIT License (below) or the Perl
 * license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef picohttpparser_h
#define picohttpparser_h

#include <linux/types.h>
#include "http_string.h"

/* contains name and value of a header (name == NULL if is a continuing line
 * of a multiline header */
struct http_header {
	struct string name;
	struct string value;
};

/* returns number of bytes consumed if successful, -2 if request is partial,
 * -1 if failed */
int http_request_parse(const char *buf, size_t len, struct string *method,
		       struct string *path, int *minor_version,
		       struct http_header *headers, size_t *num_headers,
		       size_t last_len);

/* ditto */
int http_response_parse(const char *_buf, size_t len, int *minor_version,
			int *status, const char **msg, size_t *msg_len,
			struct http_header *headers, size_t *num_headers,
			size_t last_len);

/* ditto */
int http_headers_parse(const char *buf, size_t len, struct http_header *headers,
		       size_t *num_headers, size_t last_len);

#endif
