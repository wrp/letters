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
static struct dictionary bonus_dict = {NULL, 0, 0};
extern struct dictionary default_dict[];
static struct dictionary *dict = &word_dict;

static void push_char(struct string *, int, reallocator);

static struct string
build_random_string(const char *string, reallocator r)
{
	struct string p = {NULL, 0};
	size_t wlen;
	size_t len = strlen(string);

	wlen = MINSTRING + (random() % (MAXSTRING - MINSTRING));
	while (wlen--) {
		push_char(&p, string[random() % len], r);
	}
	push_char(&p, '\0', r);
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
initialize_dict_from_string(char *choice, reallocator r)
{
	for (int i = 0; i < 1024; i += 1) {
		push_string(&word_dict, build_random_string(choice, r), r);
	}
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


static void init_bonus_words(reallocator r);

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

	init_bonus_words(r);
}

struct string
getword(void)
{
	return dict->index[random() % dict->len];
}


static void
free_dict(struct dictionary *d)
{
	struct string *s = d->index;
	struct string *e = d->index + d->len;
	while ( s < e ){
		free(s++ -> data);
	}
	free(d->index);
	d->index = NULL;
	d->cap = d->len = 0;
}


void
free_dictionaries(void)
{
	free_dict(&word_dict);
	free_dict(&bonus_dict);
}

static void
init_bonus_words(reallocator r)
{
	char *values =
		"abcdefghijklmnopqrstuvwxyz"
		"qazwsxol.p;/xcvbnm,<>\"'~z"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"+!?.,@#$%^&*()-_[]{}~|\\";
	for (int i = 0; i < 1024; i += 1) {
		push_string(&bonus_dict, build_random_string(values, r), r);
	}
}


struct string
bonusword(void)
{
	return bonus_dict.index[random() % bonus_dict.len];
}
