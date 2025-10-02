/*
 * find a random word in a dictionary file as part of letters.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2025 William Pursell (william.r.pursell@gmail.com)
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "letters.h"

# define random lrand48

extern char *choice;

static struct dictionary word_dict = {NULL, 0, 0};
extern struct dictionary default_dict[];
static struct dictionary *dict = &word_dict;

static void push_char(struct string *, int, reallocator);

/* TODO: currently, any random string will never be freed.
 * we need to construct a proper dictionary.  (Currently,
 * the dictionary built from a words file is also never
 * freed, but it does not grow during play.  This is really
 * a purely academic concern.  The memory leak is irrelevant.)
 */
static struct string
build_random_string(const char *string)
{
	struct string p = {NULL, 0};
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

static void
push_char(struct string *s, int c, reallocator r)
{
	if (s->len % 32 == 0) {
		if (s->len >= 224) {
			fprintf(stderr, "strings cannot exceed length 223");
			exit(1);
		}
		void *tmp = r(s->data, (s->len + 32) * sizeof *s->data);
		if (tmp == NULL) {
			perror("out of memory");
			exit(1);
		}
		s->data = tmp;
	}
	s->data[s->len++] = c;
}

static void
make_perm(struct string base, const char *w, size_t len, reallocator r)
{
	const char *c = w;

	if (base.len > 3) {
		push_string(dict, base, r);
	}
	if (len == 0) {
		return;
	}
	while (*c) {
		struct string s = { NULL, 0 };
		for (int i = 0; i < base.len - 1; i += 1) {
			push_char(&s, base.data[i], r);
		}
		push_char(&s, *c++, r);
		push_char(&s, '\0', r);
		make_perm(s, w, len - 1, r);
	}
}

static void
initialize_dict_from_string(char *choice, reallocator r)
{
	struct string s = { NULL, 0 };

	push_char(&s, 0, r);
	make_perm(s, choice, MAXSTRING, r);
}


static void
initialize_dict_from_path(char *path, reallocator r)
{
	FILE *fp;
	struct stat s_buf;
	int c;
	struct string s = {NULL, 0};
	if(
		(fp = fopen(path, "r")) == NULL ||
		stat(path, &s_buf) == -1
	) {
		perror(path);
		exit(1);
	}

	dict = &word_dict;
	while( (c = fgetc(fp)) != EOF ){
		if (isspace(c)) {
			if (s.data != NULL) {
				push_char(&s, 0, r);
				push_string(dict, s, r);
				s.data = NULL;
				s.len = 0;
			}
		} else {
			push_char(&s, c, r);
		}
	}
}

void
initialize_dictionary(char *path, char *dict_string, reallocator r)
{
	if (dict_string) {
		initialize_dict_from_string(dict_string, r);
	} else if (path) {
		initialize_dict_from_path(path, r);
	} else {
		dict = default_dict;
	}
}

struct string
getword(void)
{
	return dict->index[random() % dict->len];
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
