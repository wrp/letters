/*
 * find a random word in a dictionary file as part of letters.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "letters.h"

# define random lrand48

extern char *dictionary, *choice;

struct dictionary {
	struct string *index;
	size_t cap;
	size_t len;
};

typedef void *(*allocator)(size_t);
typedef void *(*reallocator)(void *, size_t);

static int push_char(struct string *, int, reallocator);

/* TODO: currently, any random string will never be freed.
 * we need to construct a proper dictionary.  (Currently,
 * the dictionary built from a words file is also never
 * freed, but it does not grow during play.  This is really
 * a purely academic concern.  The memory leak is irrelevant.)
 */
static struct string
build_random_string(const char *string)
{
	struct string p = {NULL, 0, 0};
	size_t wlen;
	size_t len = strlen(string);

	wlen = MINSTRING + (random() % (MAXSTRING - MINSTRING));
	while (wlen--) {
		push_char(&p, string[random() % len], realloc);
	}
	push_char(&p, '\0', realloc);
	return p;
}

static int
push_string(struct dictionary *d, struct string s, reallocator r)
{
	if (d->len >= d->cap) {
		void *tmp = r(d->index, (d->cap + 1024) * sizeof *d->index);
		if (tmp == NULL) {
			perror("out of memory");
			exit(1);
			return -1;
		}
		d->index = tmp;
		d->cap += 1024;
	}
	d->index[d->len++] = s;
	return 0;
}

static int
push_char(struct string *s, int c, reallocator r)
{
	if (s->len >= s->cap) {
		void *tmp = r(s->data, (s->cap + 32) * sizeof *s->data);
		if (tmp == NULL) {
			perror("out of memory");
			exit(1);
			return -1;
		}
		s->data = tmp;
		s->cap += 32;
	}
	s->data[s->len++] = c;
	return 0;
}

static void
initialize_dictionary(struct dictionary *dict, reallocator r)
{
	FILE *fp;
	struct stat s_buf;
	int c;
	struct string s = {NULL, 0, 0};
	if(
		(fp = fopen(dictionary, "r")) == NULL ||
		stat(dictionary, &s_buf) == -1
	) {
		perror(dictionary);
		exit(1);
	}
	while( (c = fgetc(fp)) != EOF ){
		if (isspace(c)) {
			if (s.data != NULL) {
				push_char(&s, 0, r);
				push_string(dict, s, r);
				s.data = NULL;
				s.cap = s.len = 0;
			}
		} else {
			push_char(&s, c, r);
		}
	}
}

struct string
getword(void)
{
	static struct dictionary dict = {NULL, 0, 0};
	static size_t len;
	struct string src;

	if (choice) {
		return build_random_string(choice);
	}

	if (dict.index == NULL) {
		initialize_dictionary(&dict, realloc);
	}

	return dict.index[random() % dict.len];
}

struct string
bonusword(void)
{
	char *values =
		"abcdefghijklmnopqrstuvwxyz"
		"qazwsxol.p;/xcvbnm,<>\"'~z"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"+!?.,@#$%^&*()-_[]{}~|\\";
	return build_random_string(values);
}
