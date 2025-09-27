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
extern int choicelen;

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

size_t
getword(char *buf, size_t bufsiz)
{
	static FILE	*fp = NULL;
	static struct stat	s_buf;

	char fmt[64];

	/*
	 * if there's a 'choice' string set, choose a selection of letters
	 * from it instead of reading the dictionary.
	 */
	if (choice) {
		return build_random_string(buf, bufsiz, choice);
	}

	/*
	 * This is stuff that only needs to get done once.
	 */
	if(fp == NULL) {
		if(
			(fp = fopen(dictionary, "r")) == NULL ||
			stat(dictionary, &s_buf) == -1
		) {
			endwin();
			perror(dictionary);
			exit(1);
		}
	}

	fseek(fp, random() % s_buf.st_size, 0);
	sprintf(fmt, "%%*s %%%lus", bufsiz - 1);
	if( fscanf(fp, fmt, buf) != 1 ){
		fseek(fp, 0L, 0);
		fscanf(fp, "%s", buf);
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
