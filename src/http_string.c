#include <linux/string.h>
#include "http_string.h"

int string_cmp(const struct string *s1, const struct string *s2)
{
	if (s1->len != s2->len) {
		return s1->len - s2->len;
	}

	return memcmp(s1->data, s2->data, s1->len);
}

int string_casecmp(const struct string *s1, const struct string *s2)
{
	if (s1->len != s2->len) {
		return s1->len - s2->len;
	}

	return strncasecmp(s1->data, s2->data, s1->len);
}

int string_starts_with(const struct string *s1, const struct string *s2)
{
	if (s1->len >= s2->len) {
		return memcmp(s1->data, s2->data, s2->len);
	}

	return -1;
}

char *string_last(const struct string *s)
{
	return s->data + s->len;
}
