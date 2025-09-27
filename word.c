/*
 * find a random word in a dictionary file as part of letters.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "letters.h"

# define random lrand48

extern char *dictionary, *choice;

static size_t
build_random_string(char *buf, size_t siz, const char *string)
{
	char *p = buf;
	size_t wlen;
	size_t len = strlen(string);

	wlen = MINSTRING + (random() % (MAXSTRING - MINSTRING));
	if (wlen > siz - 1) {
		wlen = siz - 1;
	}
	while (wlen--) {
		*p++ = string[random() % len];
	}
	*p = '\0';
	return p - buf;
}


static size_t
initialize_dictionary(char **dictp)
{
	FILE *fp;
	struct stat s_buf;
	char *dict;
	if(
		(fp = fopen(dictionary, "r")) == NULL ||
		stat(dictionary, &s_buf) == -1
	) {
		perror(dictionary);
		exit(1);
	}
	if ((dict = malloc(s_buf.st_size)) == NULL) {
		perror("malloc");
		exit(1);
	}
	if (fread(dict, 1, s_buf.st_size, fp) != s_buf.st_size) {
		if (ferror(fp)) {
			perror(dictionary);
		} else {
			fprintf(stderr, "Unexpected EOF reading %s\n",
				dictionary
			);
		}
		exit(1);
	}
	*dictp = dict;
	return s_buf.st_size;
}

size_t
getword(char *buf, size_t bufsiz)
{
	static char *dict = NULL;
	static size_t len;
	char fmt[64];

	if (choice) {
		return build_random_string(buf, bufsiz, choice);
	}

	if (dict == NULL) {
		len = initialize_dictionary(&dict);
	}

	if (buf == NULL ){
		return 0;
	}
	sprintf(fmt, "%%*s %%%lus", bufsiz - 1);
	if( sscanf(dict + random() % len, fmt, buf) != 1 ){
		return getword(buf, bufsiz);
	}

	return strlen(buf);
}

size_t
bonusword(char *buf, size_t bufsiz)
{
	char *values =
		"abcdefghijklmnopqrstuvwxyz"
		"qazwsxol.p;/xcvbnm,<>\"'~z"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"+!?.,@#$%^&*()-_[]{}~|\\";
	return build_random_string(buf, bufsiz, values);
}
