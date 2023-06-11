#ifndef _STRING_H
#define _STRING_H

#include <linux/types.h>

struct string {
	char *data;
	size_t len;
};

#define STRING_INIT(str)                      \
	{                                     \
		(char *)str, sizeof(str) - 1, \
	}

int string_cmp(const struct string *s1, const struct string *s2);
int string_casecmp(const struct string *s1, const struct string *s2);
int string_starts_with(const struct string *s1, const struct string *s2);
char *string_last(const struct string *s);
int string_copy(struct string *dst, const struct string *src, size_t max_len);

#endif
