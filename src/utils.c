#include <linux/kernel.h>
#include <linux/string.h>
#include "http_string.h"

static int check_path_len(struct string *components, int n, size_t length)
{
	int i, len;

	len = 0;

	for (i = 0; i < n; i++) {
		len += components[i].len + 1;
	}

	if (len >= length - 1) {
		return -1;
	}

	return 0;
}

// TODO: add unit test, use kunit
int http_uri_to_path(struct string *uri, struct string *dst, size_t max_len)
{
	static struct string parent = STRING_INIT("..");
	static struct string dot = STRING_INIT(".");
	static struct string root = STRING_INIT("/");

	bool has_slash;
	int i, count, pos;
	size_t rest_len;
	char *p, *last;
	struct string components[32];

	has_slash = false;
	components[0] = root;
	count = 1;

	p = uri->data;
	last = uri->data + uri->len;

	if (*(last - 1) == '/') {
		has_slash = true;
	}

	while (p < last) {
		if (*p == '/') {
			p++;
			continue;
		}

		if (count >= ARRAY_SIZE(components)) {
			return -1;
		}

		components[count].data = p;

		while (p < last && *p != '/') {
			p++;
		}

		components[count].len = p - components[count].data;

		if (string_cmp(&components[count], &parent) == 0) {
			count -= 1;

			if (count <= 0) {
				return -1;
			}

			continue;

		} else if (string_cmp(&components[count], &dot) == 0) {
			continue;
		}

		count++;
	}

	if (count <= 0) {
		return -1;
	}

	// prepare append path to dst buffer
	pos = dst->len;
	rest_len = max_len - pos - (has_slash ? 1 : 0);

	if (check_path_len(components, count, rest_len) != 0) {
		return -1;
	}

	// copy to dst buffer, skip root path.
	for (i = 1; i < count; i++) {
		if (pos + components[i].len + 1 >= rest_len - 1) {
			return -1;
		}

		dst->data[pos] = '/';
		pos += 1;

		memcpy(dst->data + pos, components[i].data, components[i].len);
		pos += components[i].len;
	}

	if (has_slash) {
		dst->data[pos] = '/';
		pos += 1;
	}

	dst->data[pos] = '\0';
	dst->len = pos;

	return 0;
}

int string_copy(struct string *dst, const struct string *src, size_t max_len)
{
	if (src->len >= max_len) {
		return -1;
	}

	memcpy(dst->data, src->data, src->len);
	dst->len = src->len;

	return 0;
}
