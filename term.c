/*
 * this program will attempt to draw random stuff on the screen by using the
 * termcap library.  Much of this code was evolved from term.c from the nn
 * sources.
 *
 * Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * Kim Storm deserves a fair amount of credit for this.  It may not look
 *   too much like his code, but his code was at least more understandable
 *   than the documentation I was trying to work from and I'd like to give
 *   credit for making it easier for me to write.
 *
 * I wrote a fair amount of this based on man pages, then used Kim's code as
 * a reference to make things work.  Some of this actually ended up looking
 * more like his code than I planned on.  I hope I haven't gone overboard.
 */

#include <stdlib.h>
#include <stdio.h>
#ifndef NEXT
# ifdef AMIGA
#  include <stdlib.h>
#  include "amitermcap.h"
# elif __TURBOC__
#  include <conio.h>
# else
#  include <termio.h>
# endif
#endif
#include <string.h>
#include "term.h"

char	*tgoto();
char	PC, *BC, *UP;

char		*term_name;
char	XBC[64], XUP[64];
char     cursor_home[64];
char     enter_underline_mode[64], exit_underline_mode[64];



#ifndef __TURBOC__
int Lines, Columns;	/* screen size */
int garbage_size;	/* number of garbage chars left from so */
int double_garbage;	/* space needed to enter&exit standout mode */
int STANDOUT;		/* terminal got standout mode */
int TWRAP;		/* terminal got automatic margins */

/*
 * used to get the actual terminal control string.
 */
opt_cap(cap, buf)
char           *cap, *buf;
{
	char	*tgetstr();

	*buf = '\0';
	return tgetstr(cap, &buf) != NULL;
}

/*
 * call opt_cap to get control string.  report if the terminal lacks that
 * capability.
 */
get_cap(cap, buf)
char *cap, *buf;
{
	if (!opt_cap(cap, buf))
		fprintf(stderr, "TERMCAP entry for %s has no '%s' capability\n",
			term_name, cap);
}

#endif /* __TURBOC__ */


underline(on)
{
#ifdef __TURBOC__
	return 0;
#else
	if (garbage_size)
		return 0;
	if (!HAS_CAP(enter_underline_mode))
		return 0;
	putp(on ? enter_underline_mode : exit_underline_mode);
	return 1;
#endif /* __TURBOC__ */
}

highlight(on)
{
#ifdef __TURBOC__

	if (on)
		textattr(0x4f); /* highlight on: white on red */
	else
		textattr(0x71); /* highlight off: blue on light gray */

#else
	if (on) {
		attron(A_STANDOUT);
	} else {
		attroff(A_STANDOUT);
	}
#endif /* __TURBOC__ */
	return 1;
}
