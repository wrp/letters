/*
 * find a random word in a dictionary file as part of letters.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#include "kinput.h"
#include "turboc.h"

#ifdef __TURBOC__
# include <conio.h>
# include <stdlib.h>
# define random rand
#endif

#ifdef SYSV
# define random lrand48
#endif

extern char *dictionary, *choice;
extern int choicelen;

char *
getword(void)
{
	static char	buf[512];
	static FILE	*fp = NULL;
	static struct stat	s_buf;

	/*
	 * if there's a 'choice' string set, choose a selection of letters
	 * from it instead of reading the dictionary.
	 */
	if (choice) {

		char *start, *p = buf;
		int   wlen;

		start = choice + (random() % choicelen);
		wlen  = (MINSTRING + (random() % (MAXSTRING - MINSTRING)))
		      % sizeof(buf);
		while(wlen--) {
			if (*start == '\0')
				start = choice;
			*p++ = *start++;
		}
		*p = '\0';
		return buf;
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
	if( fscanf(fp, "%*s%s", buf) != 1 ){
		fseek(fp, 0L, 0);
		fscanf(fp, "%s", buf);
	}

	return buf;
}


char *
bonusword(void)
{
	static char	buf[BONUSLENGTH + 1];
	int		i;

	for(i = 0; i < BONUSLENGTH; i++)
		buf[i] = choice ? *(choice + (random() % choicelen))
				: (char)(random() % 94) + 33;

	buf[BONUSLENGTH] = 0;

	return buf;
}
