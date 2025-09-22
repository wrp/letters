/*
 * header file for term.c
 */

extern char *tgoto();
extern char PC, *BC, *UP;

extern char    *term_name;
extern char     XBC[], XUP[];

#define outc putchar

extern int Lines, Columns; /* screen size */
extern int garbage_size;   /* number of garbage chars left from so */
extern int double_garbage; /* space needed to enter&exit standout mode */
extern int STANDOUT;       /* terminal got standout mode */
extern int TWRAP;          /* terminal got automatic margins */
