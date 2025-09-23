/*
 * do non-blocking keyboard reads
 *
 * copyright 1991 by Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <term.h>
#include <sys/time.h>

#include "config.h"


extern unsigned int score, word_count, level;

static void die();


static void die(sig)
int	sig;
{
	endwin();
	erase();
	highlight(0);
	exit(1);
}
