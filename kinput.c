/*
 * do non-blocking keyboard reads
 *
 * copyright 1991 by Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <term.h>
#include <sys/time.h>
#include <unistd.h>

#include "config.h"

#include "kinput.h"
#include "terms.h"
#include "turboc.h"

extern unsigned int score, word_count, level;

int key_pressed(void);

static void intrrpt();

#ifndef NOJOB
static void cont();
#endif

static void die();


/*
 * This function will return -1 if no key is available, or the key
 * that was pressed by the user.  It is checking stdin, without blocking.
 */
int
key_pressed(void)
{
	/* TODO: ensure curses is in no-delay mode */
	int c = getch();
	return c == ERR ? -1 : c;
}


static void cont(sig)
int	sig;
{
#ifdef SYSV
	signal(sig, intrrpt);
#endif /* SYSV */
	clear();
	redraw();
}

#endif /* NOJOB */

static void intrrpt(sig)
int	sig;
{
	char	c;

#ifdef __TURBOC__
	clrscr();
	puts("are you sure you want to quit? ");
	if((c = getch()) == 'y' || c == 'Y') {
		textattr_clr;
		printf("\n\nfinal: score = %u\twords = %u\t level = %d\n",
		       score, word_count, level);
		endwin();
		exit(1);
#else
	printf("\n\rare you sure you want to quit? ");
	if((c = getchar()) == 'y' || c == 'Y') {
		erase();
		printf("\n\nfinal: score = %u\twords = %u\t level = %d\n",
		       score, word_count, level);
		highlight(0);
		endwin();
		exit(1);
#endif
	} else {
#ifdef SYSV
		signal(sig, intrrpt);
#endif /* SYSV */
		erase();
		redraw();
	}
}

static void die(sig)
int	sig;
{
	textattr_clr;
#ifndef __TURBOC__
	endwin();
	erase();
	highlight(0);
	exit(1);
#endif
}
