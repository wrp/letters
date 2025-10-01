/*
 * letters.c: A simple game to help improve typing skills.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2018 David C Sterratt (david.c.sterratt@ed.ac.uk)
 * copyright 2025 William Pursell (william.r.pursell@gmail.com)
 */

/*
 * TODOs:
 * Build system (meson?)
 * Test suite
 * Refactor!

 * Add lateral movement.  Words could randomly move left or right.
 * perhaps give each word a consistent direction when created and
 * bounce off the sides.  Maybe play on a cylinder.
 *
 * Clear out the longjmp.  That was necessary when the code was
 * so convoluted that a clean return was impossible, but things are
 * clean enough now that we could easily exit without the longjmp.
 *
 * Make the bonus round get faster.  The bonus round used to end
 * if you succesfully entered enough words, but that no longer happens.
 * We should decrement the itimer a little bit each time a word is
 * completed.  (Or something!)
 */

# define CTRL(c)  (c & 0x1f)

#include "letters.h"
#include <signal.h>
#include <sys/time.h>

static int move_words(struct state *);
static void set_timer(unsigned long);
static void set_handlers(void);
static void update_wpm(struct state *);
static void finalize_word(struct state *S, struct word *w);

void update_scores(char *, struct score *, unsigned);
int read_scores(char *);
void show_scores(struct state *S);
void putword(struct word *);
static void game(struct state *);
void status(struct state *);
void new_level(struct state *);
int banner(struct state *, const char *, int);
static void maybe_add_word(struct state *);
int (*ding)(void); /* beep, flash, or no-op */

static void display_words(struct state *);


/*
 * There are too many globals for my taste, but I took the easy way out in
 * a few places
 */
char *choice = NULL;
int newdict = 0;

void
usage(const char *progname)
{
	printf(
		"Usage: %s [-q|-b] [-h] [-H#] [-l#] [-ddictionary] "
		"[-sstring]\n",
		progname
	);
	exit(0);
}

/* Do nothing functions */
int no_op(void) { return 0; }
void handle_signal(int s, siginfo_t *i, void *v) { (void)i; (void)v; (void)s; }

/* Ensure the process is running on a tty. */
/* TODO: perhaps read the dictionary from stdin */
static void
check_tty(void)
{
	if (! isatty(STDIN_FILENO) || ! isatty(STDOUT_FILENO)) {
		fputs("This game can only be played on a terminal!\n", stderr);
		exit(EXIT_FAILURE);
	}
}


static void
intrrpt(struct state *S)
{
	switch(banner(S, "Are you sure you want to quit?", 0)) {
	case 'y':
	case 'Y':
	case CTRL('C'):
		longjmp(S->jbuf, 1);
	default:
		erase();
		display_words(S);
	}
}


/* milliseconds to pause before moving words down */
int
DELAY(unsigned level)
{
	/* START_DELAY - level * (DELAY_CHANGE - (level * DECEL)) */
	long lut[] = {
		750000, 691200, 634800, 580800, 529200, 480000,
		433200, 388800, 346800, 307200, 270000, 235200,
		202800, 172800, 145200, 120000, 97200, 76800,
		58800, 43200, 30000, 19200, 10800
	};
	return level < sizeof lut / sizeof *lut ? lut[level] : 10000;
}


static void
handle_argument(struct state *S, char **argv)
{
	char *end;
	char *arg = *argv;

	switch(arg[1]) {
	case 'b': ding = beep; break;
	case 'q': ding = flash; break;
	case 'H':
		S->handicap = (int)strtol(arg + 2, &end, 0);
		if (*end || S->handicap < 1 || S->handicap > 99) {
			fprintf(stderr, "Invalid handicap %s\n", arg + 2);
			exit(1);
		}
		break;
	case 'h':
		read_scores(HIGHSCORES);
		puts(score_header);
		for (char s[64]; next_score(s, sizeof s); ) {
			printf("%s\n", s);
		}
		exit(0);
		break;
	case 'l':
		S->level = (int)strtol(arg + 2, &end, 0);
		if (*end || S->level < 1) {
			fprintf(stderr, "Invalid level %s\n", arg + 2);
			exit(1);
		}
		break;
	case 'd':
		S->dictionary = arg + 2;
		newdict = 1;
		if (! arg[2]) {
			fprintf(stderr, "-d option requires an argument\n");
			exit(1);
		}
		break;
	case 's':
		if (! arg[2]) {
			fprintf(stderr, "-s option requires an argument\n");
			exit(1);
		}
		choice = arg + 2;
		break;
	default:
		fprintf(stderr, "Unknown option: -%c\n", arg[1]);
		exit(1);
	}
}


static void
parse_cmd_line(int argc, char **argv, struct state *S)
{
	char *progname;
	progname = argv[0];
	while(*++argv) {
		if((*argv)[0] == '-') {
			handle_argument(S, argv);
		} else {
			usage(progname);
		}
	}
}


struct word word_store[256];
static void
allocate_words(struct state *S)
{
	struct word *e = word_store + sizeof word_store / sizeof *word_store;

	S->free = word_store;
	for (struct word *w = word_store; w < e; w += 1) {
		w->next = w + 1;
	}
}


static void
init(struct state *S, int argc, char **argv)
{
	check_tty();
	unsetenv("COLUMNS");
	unsetenv("LINES");
	allocate_words(S);

	ding = no_op;
	S->handicap = 1;
	S->words = NULL;
	S->lives = 2;
	S->dictionary = NULL;
	parse_cmd_line(argc, argv, S);
	initialize_dictionary(S->dictionary, realloc);

	set_handlers();
	srand48(time(NULL));
	initscr();
	raw();
	curs_set(0);
	noecho();
	keypad(stdscr, 1);
	clear();

	new_level(S);
	status(S);
	timeout(1000);
}


int
main(int argc, char **argv)
{
	struct state S[1] = {{0}};

	init(S, argc, argv);

	if (setjmp(S->jbuf)) {
		goto exit;
	}

	game(S);

	display_words(S);
	set_timer(0);
	banner(S, "Game Over", 3);

exit:
	set_timer(0);
	timeout(-1);
	read_scores(HIGHSCORES);
	if (S->handicap == 1 && newdict == 0 && choice == NULL)
		update_scores(HIGHSCORES, &S->score, S->level);
	show_scores(S);
	endwin();

	return 0;
}


/* Exchange w and w->next in the list */
static struct word *
swap(struct word **p, struct word *w)
{
	*p = w->next;
	w->next = w->next->next;
	(*p)->next = w;
	return *p;
}


static void
display_words(struct state *S)
{
	erase();
	status(S);
	for (struct word *w = S->words; w; w = w->next) {
		putword(w);
	}
	refresh();
}

/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(struct state *S)
{
	struct word *w = S->words;
	struct word **prev = w ? &S->words : NULL;
	int  died = 0;

	while (w != NULL) {
		if (w->next && w->y < w->next->y) {
			w = swap(prev, w);
			continue;
		}
		w->y += w->drop;
		if (w->y > LINES - 1) {
			if (!w->killed) {
				w->killed = -3;
				died += 1;
			}
			w->y = LINES - 1;
		}
		prev = &w->next;
		w = w->next;
	}

	if (died > 0) {
		/*
		 * subtract lives if a word reaches the
		 * bottom in a normal round.  If a word reaches
		 * bottom during bonus play, just end the bonus
		 * round.
		 */
		if (! S->bonus) {
			S->lives -= died;
		}
		if (S->lives < 0) {
			S->lives = 0;
		}
	}
	return died;
}


/* write the word to the screen with already typed letters highlighted */
void
putword(struct word *w)
{
	int idx = w->matches;

	move(w->y, w->x);
	if (! w->killed ){
		assert(idx <= w->length);
		highlight(1);
		addnstr(w->word.data, idx);
		highlight(0);
		addstr(w->word.data + w->matches);
	} else {
		for (int i = 0; i < w->length; i += 1) {
			char t[] = "*#+  --";
			addch(t[3 + w->killed]);
		}
	}
}


/* Process a CTRL key. */
static void
process_ctrl_key(struct state *S, int key)
{
	switch(key) {
	case CTRL('L'):
	case KEY_RESIZE:
		display_words(S);
		break;
	case CTRL('N'):
		S->level += 1;
		S->delay = S->handicap * DELAY(S->level);
		set_timer(S->delay / 1000);
		status(S);
		break;
	case CTRL('C'):
		intrrpt(S);
		break;
	}
}

/*
 * Check the key against each word and upate the "matches" member.
 * Update the curses window.  Return the number of words that match.
 */
static void
check_matches(struct state *S, int key)
{
	for (struct word *w = S->words; w != NULL; w = w->next) {
		if (w->killed) {
			continue;
		}
		if (key == w->word.data[w->matches]) {
			w->matches += 1;
			if (w->matches == w->length) {
				finalize_word(S, w);
			}
		} else {
			w->matches = 0;
		}
		putword(w);
	}
}


/* Process the user keystrokes until signal is received */
static void
process_keys(struct state *S)
{
	int  key;
	while( ((key = getch()) != ERR)) {
		if (key == CTRL(key)) {
			process_ctrl_key(S, key);
		} else {
			check_matches(S, key);
		}
		status(S);
		refresh();
	}
}


/* Word has been successfully typed.  Increment score and mark killed. */
static void
finalize_word(struct state *S, struct word *w)
{
	assert (w->length == w->matches);
	S->score.points += w->length + (2 * S->level);
	S->score.words += 1;
	S->letters += w->length;
	w->killed = 3;

	for (struct word *w = S->words; w != NULL; w = w->next) {
		w->matches = 0;
	}
	if (S->score.words % LEVEL_CHANGE == 0) {
		if (S->bonus) {
			S->score.points += 10 * S->level;
		} else {
			new_level(S);
		}
	}
}


/* Set an interval timer in ms */
static void
set_timer(unsigned long delay_msec)
{
	struct timeval tp = {
		.tv_sec = 0,
		.tv_usec = delay_msec * 1000
	};
	while (tp.tv_usec > 999000) {
		tp.tv_sec += 1;
		tp.tv_usec -= 1000000;
	}
	struct itimerval t = {
		.it_interval = tp,
		.it_value = tp
	};

	if (setitimer(ITIMER_REAL, &t, NULL)) {
		endwin();
		perror("setitimer");
		exit(1);
	}
}


static struct word **
lastnext(struct state *S)
{
	if (S->words == NULL) {
		return &S->words;
	} else {
		struct word *w;
		for (w = S->words; w->next; w = w->next) {
			;
		}
		return &w->next;
	}
}


/* Walk the word list, decrementing killed or freeing dead words */
static void
garbage_collect(struct state *S)
{
	struct word *w = S->words;
	struct word **p = &S->words;
	struct word *next;

	for (; w; w = next) {
		next = w->next;
		if (! w->killed) {
			p = &(*p)->next;
			continue;
		}

		assert(w->killed != 0);
		w->killed += (w->killed < 0) ? +1 : -1;
		if (w->killed == 0) {
			*p = next;
			w->next = S->free;
			S->free = w;
		}
	}
}


static void
game(struct state *S)
{
	long  i;

	while (S->lives > 0) {
		maybe_add_word(S);

		process_keys(S);

		if (move_words(S)) {
			if (S->bonus) {
				new_level(S);
			}
		}
		display_words(S);
		garbage_collect(S);
	}
}


/* Update the status line. */
void
status(struct state *S)
{
	highlight(1);
	update_wpm(S);
#define STATUS_WIDTH ( 0\
	+ sizeof "Score:" + 7 \
	+ sizeof "Level:" + 3 \
	+ sizeof "Words:" + 6 \
	+ sizeof "Lives:" + 3 \
	+ sizeof "WPM:" + 4 \
	)
	move(0, COLS / 2 - (STATUS_WIDTH / 2));
#undef STATUS_WIDTH
	printw("Score: %-7u", S->score.points);
	printw("Level: %-3u", S->level);
	printw("Words: %-6u", S->score.words);
	printw("Lives: %-3d", S->lives);
	printw("WPM: %-4d", S->wpm);
	clrtoeol();
	highlight(0);
}


/* erase all existing words */
static void
erase_word_list(struct state *S)
{
	struct word *next;
	for (struct word *w = S->words; w != NULL; w = next) {
		next = w->next;
		w->killed = 1;
	}
}


/* compute words per minute for the given level */
static void
update_wpm(struct state *S)
{
	time_t curr_time;

	time(&curr_time);
	if (curr_time - S->start_time < 5) {
		return;
	}
	S->wpm = (S->letters / 5) / ((curr_time - S->start_time) / 60.0);
}


/*
 * do stuff to change levels.  This is where special rounds can be stuck in.
 */
void
new_level(struct state *S)
{
	update_wpm(S);
	time(&S->start_time);
	S->letters = 0;

	/*
	 * if we're inside a bonus round we don't need to change anything
	 * else so just take us out of the bonus round and exit this routine
	 */
	if (S->bonus) {
		S->bonus = false;
		set_timer(0);
		banner(S, "Bonus round finished", 3);
		erase_word_list(S);
		status(S);
		set_timer(S->delay / 1000);
		return;
	}

	/*
	 * If you start at a level other than 1, the level does not
	 * actually change until you've completed a number of levels equal
	 * to the starting level.
	 */
	if(S->level <= S->levels_completed++)
		S->level += 1;

	S->delay = S->handicap * DELAY(S->level);
	set_timer(S->delay / 1000);

	if (S->score.words && ! ((S->levels_completed - 1) % LVL_PER_BONUS )) {
		S->bonus = true;
		erase_word_list(S);
		set_timer(0);
		banner(S, "Prepare for bonus words", 3);
		set_timer(850);
		S->lives += 1;
	}

	status(S);
}


/* Return true if at least one word is currently active */
static int
word_in_play(struct state *S)
{
	for (struct word *w = S->words; w; w = w->next) {
		if (! w->killed) {
			return 1;
		}
	}
	return 0;
}


/* If appropriate, create a new word and put in in play. */
static void
maybe_add_word(struct state *S)
{
	if (word_in_play(S) && (random() % ADDWORD) != 0) {
		return;
	}
	if (! S->free) {
		return;
	}
	int bonus = S->bonus;
	struct word *n = S->free;
	S->free = n->next;
	int  len;

	n->word = bonus ? bonusword() : getword();
	n->length = len = n->word.len - 1;
	n->drop = len > 6 ? 1 : len > 3 ? 2 : 3;
	n->matches = 0;
	n->x = random() % ((COLS - 1) - n->length);
	n->y = 1;
	n->next = NULL;
	n->killed = 0;
	putword(n);

	*lastnext(S) = n;
}


/* momentarily display a banner message across the screen */
int
banner(struct state *S, const char *text, int delay_sec)
{
	int c = ERR;
	int len = strlen(text);
#define HEIGHT 3
	WINDOW *boxw = newwin(HEIGHT, 6 + len, LINES / 3, (COLS - len) / 2 );
#undef HEIGHT

	box(boxw, 0, 0);

	mvwaddstr(boxw, 1, 3, text);
	wrefresh(boxw);
	refresh();
	set_timer(0);
	if (delay_sec) {
		sleep(delay_sec);
	} else {
		timeout(-1);
		c = wgetch(boxw);
		set_timer(S->delay / 1000);
		timeout(1000);
	}
	delwin(boxw);
	display_words(S);
	return c;
}


void
highlight(int s)
{
	switch(s) {
	case 0: attroff(A_STANDOUT); break;
	case 1: attron(A_STANDOUT); break;
	}
}


int
underline(int on)
{
	return on ? attron(A_UNDERLINE) : attroff(A_UNDERLINE);
}


static void
set_handlers(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof act);
	act.sa_sigaction = handle_signal;
	if (sigaction(SIGALRM, &act, NULL)) {
		perror("sigaction");
		exit(1);
	}
}
