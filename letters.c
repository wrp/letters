/*
 * letters.c: A simple game to help improve typing skills.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2018 David C Sterratt (david.c.sterratt@ed.ac.uk)
 */

/*
 * TODO
 * Build system (meson?)
 * Test suite
 * Refactor!
 * "Explode" words -- display with "----" or "****" before killing

 * Add lateral movement.  Words could randomly move left or right.
 * perhaps give each word a consistent direction when created and
 * bounce off the sides.  Maybe play on a cylinder.
 */

# define CTRL(c)  (c & 0x1f)

#include "letters.h"
#include <signal.h>
#include <sys/time.h>

static int move_words(struct state *);
static void set_timer(struct state *);
static void set_handlers(void);

void update_scores(char *, struct score *, unsigned);
int read_scores(char *);
void show_scores(struct state *S);
void putword(struct word *);
static void game(struct state *);
void erase_word(struct word *);
void status(struct state *);
void new_level(struct state *);
int banner(const char *, int);
static void maybe_add_word(struct state *);
void kill_word(struct word *, struct state *S);
int (*ding)(void); /* beep, flash, or no-op */


/*
 * There are too many globals for my taste, but I took the easy way out in
 * a few places
 */
int levels_played = -1;
int wpm = 0;
char *dictionary = DICTIONARY;
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
	switch(banner("Are you sure you want to quit?", 0)) {
	case 'y':
	case 'Y':
	case CTRL('C'):
		longjmp(S->jbuf, 1);
	default:
		erase();
		status(S);
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
		dictionary = arg + 2;
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


static void
init(struct state *S, int argc, char **argv)
{
	check_tty();
	unsetenv("COLUMNS");
	unsetenv("LINES");
	getword(NULL, 0);

	ding = no_op;
	S->handicap = 1;
	parse_cmd_line(argc, argv, S);

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
	S->words = NULL;
	S->lives = 2;
}


int
main(int argc, char **argv)
{
	struct state S[1] = {{0}};

	init(S, argc, argv);

	if (setjmp(S->jbuf)) {
		goto exit;
	}
	do {
		game(S);
	} while (S->lives > 0);

exit:
	set_timer(NULL);
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

/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(struct state *S)
{
	struct word *w = S->words;
	struct word **prev = w ? &S->words : NULL;
	struct word *next;
	int  died = 0;

	while (w != NULL) {
		if (w->next && w->y < w->next->y) {
			w = swap(prev, w);
			continue;
		}
		next = w->next;
		erase_word(w);
		w->y += w->drop;

		if (w->y >= LINES) {
			kill_word(w, S);
			died += 1;
		} else {
			putword(w);
		}
		prev = &w->next;
		w = next;
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
		} else if (died > 0) {
			new_level(S);
		}
		if (S->lives < 0) {
			S->lives = 0;
		}
	}
	return died;
}

/*
 * erase a word on the screen by printing the correct number of blanks
 */
void erase_word(struct word *w)
{
	int i;

	move(w->y, w->x);
	for(i = 0; i < w->length; i++)
		addch(' ');
}

/* write the word to the screen with already typed letters highlighted */
void
putword(struct word *w)
{
	int idx = w->matches;

	assert(idx <= w->length);
	highlight(1);
	mvaddnstr(w->y, w->x, w->word, idx);
	highlight(0);
	addstr(w->word + w->matches);
}


static int
handle_ctrl_key(struct state *S, int key)
{
	switch(key) {
	case CTRL('L'):
	case KEY_RESIZE:
		erase();
		status(S);
		break;
	case CTRL('N'):
		S->level += 1;
		S->delay = S->handicap * DELAY(S->level);
		status(S);
		break;
	case CTRL('C'):
		intrrpt(S);
		break;
	default:
		return 0;
	}
	return 1;
}


/* Process the user keystrokes until word matched or signal received */
static void
process_keys(struct state *S)
{
	int  key;
	struct word *temp_word;
	while( ((key = getch()) != ERR)) {
		if (handle_ctrl_key(S, key)) {
			continue;
		}
		for (struct word *w = S->words; w != NULL; w = w->next) {
			if (key == w->word[w->matches]) {
				w->matches += 1;
				if (w->matches == w->length) {
					S->completed = w;
				}
			} else {
				w->matches = 0;
			}
			putword(w);
		}
		if (S->completed != NULL) {
			return;
		}
	}
}


/* Word has been successfully typed.  Increment score and remove. */
static void
finalize_word(struct state *S)
{
	struct word *w = S->completed;
	assert (w->length == w->matches);
	erase_word(w);
	S->score.points += w->length + (2 * S->level);
	S->score.letters += w->length;
	S->score.words += 1;
	kill_word(w, S);
}


/* Set an interval timer in ms */
static void
set_timer(struct state *S)
{
	unsigned long delay_msec = S ? S->delay / 1000 : 0;
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
	timeout(delay_msec);
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


static void
game(struct state *S)
{
	long  i;

	S->completed = NULL;
	while(S->completed == NULL) {
		maybe_add_word(S);

		set_timer(S);
		process_keys(S);
		refresh();

		if (move_words(S)) {
			status(S);
			return;
		}
	}

	finalize_word(S);
	for (struct word *w = S->words; w != NULL; w = w->next) {
		w->matches = 0;
	}
	status(S);
	if (S->score.words % LEVEL_CHANGE == 0) {
		if (S->bonus) {
			S->score.points += 10 * S->level;
		}
		new_level(S);
	}
}


/* Update the status line. */
void
status(struct state *S)
{
	highlight(1);
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
	printw("WPM: %-4d", wpm);
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
		kill_word(w, S);
	}
}


/*
 * do stuff to change levels.  This is where special rounds can be stuck in.
 */
void
new_level(struct state *S)
{
	static time_t last_time = 0L;
	time_t  curr_time;

	/*
	 * update the words per minute
	 */
	time(&curr_time);
	wpm = (S->score.letters / 5) / ((curr_time - last_time) / 60.0);
	last_time = curr_time;
	S->score.letters  = 0;

	/*
	 * if we're inside a bonus round we don't need to change anything
	 * else so just take us out of the bonus round and exit this routine
	 */
	if (S->bonus) {
		S->bonus = false;
		banner("Bonus round finished", 3);
		erase_word_list(S);
		status(S);
		return;
	}

	levels_played++;

	/*
	 * If you start at a level other than 1, the level does not
	 * actually change until you've completed a number of levels equal
	 * to the starting level.
	 */
	if(S->level <= levels_played)
		S->level += 1;

	S->delay = S->handicap * DELAY(S->level);

	if((levels_played % LVL_PER_BONUS == 0) && (levels_played != 0)) {
		S->bonus = true;
		erase_word_list(S);
		banner("Prepare for bonus words", 3);
		S->lives += 1;
	}

	status(S);
}

/* If appropriate, create a new word and put in in play. */
static void
maybe_add_word(struct state *S)
{
	if (S->words && (random() % ADDWORD) != 0) {
		return;
	}
	int bonus = S->bonus;
	struct word *n;
	int  len;
	size_t s = sizeof n->word;

	/* TODO: stop allocating memory after initialization
	 * build a pool of words and use them.
	 */
	n = malloc(sizeof *n);

	if (n == NULL) {
		endwin();
		perror("malloc");
		exit(1);
	}

	n->length = len = bonus ? bonusword(n->word, s) : getword(n->word, s);
	n->drop = len > 6 ? 1 : len > 3 ? 2 : 3;
	n->matches = 0;
	n->x = random() % ((COLS - 1) - n->length);
	n->y = 1;
	n->next = NULL;
	putword(n);

	*lastnext(S) = n;
}


/* Delete the completed word and revise pointers  */
void
kill_word(struct word *w, struct state *S)
{
	assert(S->words);

	struct word **p = &S->words;
	struct word *n = S->words->next;

	while (*p != w) {
		p = &((*p)->next);
		n = n->next;
	}
	*p = w->next;
	free(w);
}


/*
 * momentarily display a banner message across the screen
 */
int
banner(const char *text, int delay_sec)
{
	int c = ERR;

	erase();
	mvaddstr(10, (COLS - strlen(text)) / 2, text);
	refresh();
	set_timer(NULL);
	if (delay_sec) {
		sleep(delay_sec);
	} else {
		timeout(-1);
		c = getch();
		timeout(0);
	}
	erase();
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
