#include "letters.h"

static struct string words[] = {
	{ "foo", 0, 4 },
	{ "bar", 0, 4 },
	{ "banana", 0, 7 },
};

struct dictionary default_dict = {
	.index = words,
	.cap = 0,
	.len = sizeof words /sizeof *words
};
