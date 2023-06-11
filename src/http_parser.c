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

#include <linux/compiler.h>
#include <linux/string.h>

#include "http_parser.h"

#define ALIGNED(n) __attribute__((aligned(n)))

#define IS_PRINTABLE_ASCII(c) ((unsigned char)(c)-040u < 0137u)

#define CHECK_EOF()           \
	if (buf == buf_end) { \
		*ret = -2;    \
		return NULL;  \
	}

#define EXPECT_CHAR_NO_CHECK(ch) \
	if (*buf++ != ch) {      \
		*ret = -1;       \
		return NULL;     \
	}

#define EXPECT_CHAR(ch) \
	CHECK_EOF();    \
	EXPECT_CHAR_NO_CHECK(ch);

#define ADVANCE_TOKEN(tok, toklen)                                        \
	do {                                                              \
		const char *tok_start = buf;                              \
		CHECK_EOF();                                              \
		while (1) {                                               \
			if (*buf == ' ') {                                \
				break;                                    \
			} else if (unlikely(!IS_PRINTABLE_ASCII(*buf))) { \
				if ((unsigned char)*buf < '\040' ||       \
				    *buf == '\177') {                     \
					*ret = -1;                        \
					return NULL;                      \
				}                                         \
			}                                                 \
			++buf;                                            \
			CHECK_EOF();                                      \
		}                                                         \
		tok = (char *)tok_start;                                  \
		toklen = buf - tok_start;                                 \
	} while (0)

static const char *token_char_map =
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\1\0\1\1\1\1\1\0\0\1\1\0\1\1\0\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0"
	"\0\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\1\1"
	"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\1\0\1\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static const char *get_token_to_eol(const char *buf, const char *buf_end,
				    const char **token, size_t *token_len,
				    int *ret)
{
	const char *token_start = buf;

	/* find non-printable char within the next 8 bytes, this is the hottest code; manually inlined */
	while (likely(buf_end - buf >= 8)) {
#define DOIT()                                           \
	do {                                             \
		if (unlikely(!IS_PRINTABLE_ASCII(*buf))) \
			goto NonPrintable;               \
		++buf;                                   \
	} while (0)
		DOIT();
		DOIT();
		DOIT();
		DOIT();
		DOIT();
		DOIT();
		DOIT();
		DOIT();
#undef DOIT
		continue;
NonPrintable:
		if ((likely((unsigned char)*buf < '\040') &&
		     likely(*buf != '\011')) ||
		    unlikely(*buf == '\177')) {
			goto FOUND_CTL;
		}
		++buf;
	}

	for (;; ++buf) {
		CHECK_EOF();
		if (unlikely(!IS_PRINTABLE_ASCII(*buf))) {
			if ((likely((unsigned char)*buf < '\040') &&
			     likely(*buf != '\011')) ||
			    unlikely(*buf == '\177')) {
				goto FOUND_CTL;
			}
		}
	}

FOUND_CTL:

	if (likely(*buf == '\015')) {
		++buf;
		EXPECT_CHAR('\012');

		*token_len = buf - 2 - token_start;
	} else if (*buf == '\012') {
		*token_len = buf - token_start;
		++buf;
	} else {
		*ret = -1;
		return NULL;
	}

	*token = token_start;

	return buf;
}

static const char *is_complete(const char *buf, const char *buf_end,
			       size_t last_len, int *ret)
{
	int ret_cnt = 0;
	buf = last_len < 3 ? buf : buf + last_len - 3;

	while (1) {
		CHECK_EOF();

		if (*buf == '\015') {
			++buf;
			CHECK_EOF();

			EXPECT_CHAR('\012');
			++ret_cnt;
		} else if (*buf == '\012') {
			++buf;
			++ret_cnt;
		} else {
			++buf;
			ret_cnt = 0;
		}

		if (ret_cnt == 2) {
			return buf;
		}
	}

	*ret = -2;
	return NULL;
}

#define PARSE_INT(valp_, mul_)          \
	if (*buf < '0' || '9' < *buf) { \
		buf++;                  \
		*ret = -1;              \
		return NULL;            \
	}                               \
	*(valp_) = (mul_) * (*buf++ - '0');

#define PARSE_INT_3(valp_)            \
	do {                          \
		int res_ = 0;         \
		PARSE_INT(&res_, 100) \
		*valp_ = res_;        \
		PARSE_INT(&res_, 10)  \
		*valp_ += res_;       \
		PARSE_INT(&res_, 1)   \
		*valp_ += res_;       \
	} while (0)

/* returned pointer is always within [buf, buf_end), or null */
static const char *parse_token(const char *buf, const char *buf_end,
			       char **token, size_t *token_len, char next_char,
			       int *ret)
{
	/* We use pcmpestri to detect non-token characters. This instruction can take no more than eight character ranges (8*2*8=128
     * bits that is the size of a SSE register). Due to this restriction, characters `|` and `~` are handled in the slow loop. */
	const char *buf_start = buf;

	CHECK_EOF();

	while (1) {
		if (*buf == next_char) {
			break;
		} else if (!token_char_map[(unsigned char)*buf]) {
			*ret = -1;
			return NULL;
		}

		++buf;
		CHECK_EOF();
	}

	*token = (char *)buf_start;
	*token_len = buf - buf_start;

	return buf;
}

/* returned pointer is always within [buf, buf_end), or null */
static const char *parse_http_version(const char *buf, const char *buf_end,
				      int *minor_version, int *ret)
{
	/* we want at least [HTTP/1.<two chars>] to try to parse */
	if (buf_end - buf < 9) {
		*ret = -2;
		return NULL;
	}

	EXPECT_CHAR_NO_CHECK('H');
	EXPECT_CHAR_NO_CHECK('T');
	EXPECT_CHAR_NO_CHECK('T');
	EXPECT_CHAR_NO_CHECK('P');
	EXPECT_CHAR_NO_CHECK('/');
	EXPECT_CHAR_NO_CHECK('1');
	EXPECT_CHAR_NO_CHECK('.');
	PARSE_INT(minor_version, 1);

	return buf;
}

static const char *parse_headers(const char *buf, const char *buf_end,
				 struct http_header *headers,
				 size_t *num_headers, size_t max_headers,
				 int *ret)
{
	const char *value;
	size_t value_len;
	const char *value_end;

	for (;; ++*num_headers) {
		CHECK_EOF();

		if (*buf == '\015') {
			++buf;
			EXPECT_CHAR('\012');
			break;
		} else if (*buf == '\012') {
			++buf;
			break;
		}

		if (*num_headers == max_headers) {
			*ret = -1;
			return NULL;
		}

		if (!(*num_headers != 0 && (*buf == ' ' || *buf == '\t'))) {
			/* parsing name, but do not discard SP before colon, see
             * http://www.mozilla.org/security/announce/2006/mfsa2006-33.html */
			if ((buf = parse_token(buf, buf_end,
					       &headers[*num_headers].name.data,
					       &headers[*num_headers].name.len,
					       ':', ret)) == NULL) {
				return NULL;
			}

			if (headers[*num_headers].name.len == 0) {
				*ret = -1;
				return NULL;
			}

			++buf;

			for (;; ++buf) {
				CHECK_EOF();

				if (!(*buf == ' ' || *buf == '\t')) {
					break;
				}
			}
		} else {
			headers[*num_headers].name.data = NULL;
			headers[*num_headers].name.len = 0;
		}

		if ((buf = get_token_to_eol(buf, buf_end, &value, &value_len,
					    ret)) == NULL) {
			return NULL;
		}

		/* remove trailing SPs and HTABs */
		value_end = value + value_len;
		for (; value_end != value; --value_end) {
			const char c = *(value_end - 1);
			if (!(c == ' ' || c == '\t')) {
				break;
			}
		}

		headers[*num_headers].value.data = (char *)value;
		headers[*num_headers].value.len = value_end - value;
	}

	return buf;
}

static const char *parse_request(const char *buf, const char *buf_end,
				 struct string *method, struct string *path,
				 int *minor_version,
				 struct http_header *headers,
				 size_t *num_headers, size_t max_headers,
				 int *ret)
{
	/* skip first empty line (some clients add CRLF after POST content) */
	CHECK_EOF();

	if (*buf == '\015') {
		++buf;
		EXPECT_CHAR('\012');
	} else if (*buf == '\012') {
		++buf;
	}

	/* parse request line */
	if ((buf = parse_token(buf, buf_end, &method->data, &method->len, ' ',
			       ret)) == NULL) {
		return NULL;
	}

	do {
		++buf;
		CHECK_EOF();
	} while (*buf == ' ');

	ADVANCE_TOKEN(path->data, path->len);

	do {
		++buf;
		CHECK_EOF();
	} while (*buf == ' ');

	if (method->len == 0 || path->len == 0) {
		*ret = -1;
		return NULL;
	}

	if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) ==
	    NULL) {
		return NULL;
	}

	if (*buf == '\015') {
		++buf;
		EXPECT_CHAR('\012');
	} else if (*buf == '\012') {
		++buf;
	} else {
		*ret = -1;
		return NULL;
	}

	return parse_headers(buf, buf_end, headers, num_headers, max_headers,
			     ret);
}

int http_request_parse(const char *buf_start, size_t len, struct string *method,
		       struct string *path, int *minor_version,
		       struct http_header *headers, size_t *num_headers,
		       size_t last_len)
{
	int r;
	const char *buf = buf_start, *buf_end = buf_start + len;
	size_t max_headers = *num_headers;

	*minor_version = -1;
	*num_headers = 0;

	/* if last_len != 0, check if the request is complete (a fast countermeasure
       againt slowloris */
	if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
		return r;
	}

	if ((buf = parse_request(buf, buf_end, method, path, minor_version,
				 headers, num_headers, max_headers, &r)) ==
	    NULL) {
		return r;
	}

	return (int)(buf - buf_start);
}

static const char *parse_response(const char *buf, const char *buf_end,
				  int *minor_version, int *status,
				  const char **msg, size_t *msg_len,
				  struct http_header *headers,
				  size_t *num_headers, size_t max_headers,
				  int *ret)
{
	/* parse "HTTP/1.x" */
	if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) ==
	    NULL) {
		return NULL;
	}

	/* skip space */
	if (*buf != ' ') {
		*ret = -1;
		return NULL;
	}

	do {
		++buf;
		CHECK_EOF();
	} while (*buf == ' ');

	/* parse status code, we want at least [:digit:][:digit:][:digit:]<other char> to try to parse */
	if (buf_end - buf < 4) {
		*ret = -2;
		return NULL;
	}

	PARSE_INT_3(status);

	/* get message including preceding space */
	if ((buf = get_token_to_eol(buf, buf_end, msg, msg_len, ret)) == NULL) {
		return NULL;
	}

	if (*msg_len == 0) {
		/* ok */
	} else if (**msg == ' ') {
		/* Remove preceding space. Successful return from `get_token_to_eol` guarantees that we would hit something other than SP
         * before running past the end of the given buffer. */
		do {
			++*msg;
			--*msg_len;
		} while (**msg == ' ');
	} else {
		/* garbage found after status code */
		*ret = -1;
		return NULL;
	}

	return parse_headers(buf, buf_end, headers, num_headers, max_headers,
			     ret);
}

int http_response_parse(const char *buf_start, size_t len, int *minor_version,
			int *status, const char **msg, size_t *msg_len,
			struct http_header *headers, size_t *num_headers,
			size_t last_len)
{
	const char *buf = buf_start, *buf_end = buf + len;
	size_t max_headers = *num_headers;
	int r;

	*minor_version = -1;
	*status = 0;
	*msg = NULL;
	*msg_len = 0;
	*num_headers = 0;

	/* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
	if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
		return r;
	}

	if ((buf = parse_response(buf, buf_end, minor_version, status, msg,
				  msg_len, headers, num_headers, max_headers,
				  &r)) == NULL) {
		return r;
	}

	return (int)(buf - buf_start);
}

int http_headers_parse(const char *buf_start, size_t len,
		       struct http_header *headers, size_t *num_headers,
		       size_t last_len)
{
	const char *buf = buf_start, *buf_end = buf + len;
	size_t max_headers = *num_headers;
	int r;

	*num_headers = 0;

	/* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
	if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
		return r;
	}

	if ((buf = parse_headers(buf, buf_end, headers, num_headers,
				 max_headers, &r)) == NULL) {
		return r;
	}

	return (int)(buf - buf_start);
}

#undef CHECK_EOF
#undef EXPECT_CHAR
#undef ADVANCE_TOKEN
