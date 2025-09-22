/*
 * header file to be used with term.c.  defines some macros to place the
 * cursor in various places on the screen.
 *
 * Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

# define putp(str)      tputs(str, 0, putchar)
# define HAS_CAP(str)	(*str)
# define clrdisp()      tputs(clear_screen, Lines, putchar)
# define home()		putp(cursor_home)
# define goto_xy(c, l)	putp(tgoto(cursor_address, c, l))

#define SP ' '
