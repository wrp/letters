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
 *
 * Clear out the longjmp.  That was necessary when the code was
 * so convoluted that a clean return was impossible, but things are
 * clean enough now that we could easily exit without the longjmp.
 *
 * Make the bonus round get faster.  The bonus round used to end
 * if you succesfully entered enough words, but that no longer happens.
 * We should decrement the itimer a little bit each time a word is
 * completed.  (Or something!)  Maybe just increase probability of
 * new words in bonus round.
 *
 * Maybe retain the word list through a bonus round so that the
 * previous words come back after the round.  Mostly this would enable
 * a much cleaner, simpler code.  I am not really considering game
 * mechanics with this though.  I just want to get rid of the new_level()
 * function.  Or, do not have a discrete bonus round at all.  Just start
 * throwing in bonus words that do not decrement the lives counter
 * when they hit the bottom.
 *
 * Pause the timer during banners.  Or, rather, add a timer to calculate
 * the pause time so that WPM could be both per-level and overall.  In
 * fact, I like that idea: have two wpm counters and report them both.
 *
 * Make the decay rate dynamic.
 *
 * Perhaps instead of a dedicated bonus round, we make every Nth new
 * word a bonus word (or introduce words at a random rate) which does
 * not fall but just floats horizontally across the screen once.  Make
 * it a high scoring word.
 *
 * Maybe, instead of -l# to specify a starting level, we do --wpm to
 * specify a target wpm and compute (semi-accurately) the addwords
 * value needed that is appropriate for that typing speed.
 *
 * Add an option to dampen (or disable) lateral movement.
 */

# define CTRL(c)  (c & 0x1f)

#include "letters.h"
#include <signal.h>
#include <sys/time.h>

static volatile sig_atomic_t tick; /* total number of SIGALRM received */

static int move_words(struct state *);
static void set_timer(unsigned long);
static void set_handlers(void);
static void update_wpm(struct state *);
static void finalize_word(struct state *S, struct word *w);
static struct word word_store[256];

void update_scores(struct score *, unsigned);
void show_scores(struct state *S);
void putword(struct word *);
static void game(struct state *);
void status(struct state *);
void new_level(struct state *);
int banner(struct state *, const char *, int);
static struct word * maybe_add_word(struct state *);
static struct word * add_word(struct state *);

static void display_words(struct state *);


void
usage(const char *progname)
{
	printf("usage: %s ", progname);
	puts(" [-hH] [-l start-level] [-d dictionary] [-s string]\n");
	puts("option:");
	puts("  -h     print usage statement");
	puts("  -H     print high score list");
	puts("  -l     start the game a start-level");
	puts("  -d     initialize word list from the given path");
	puts("  -s     generate random strings from characters in string");
}

static void
handle_signal(int s, siginfo_t *i, void *v)
{
	(void)i; (void)v; (void)s;
	tick += 1;
}

/* Ensure the process is running on a tty. */
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
		display_words(S);
	}
}


static int
handle_argument(struct state *S, char **argv, char *progname)
{
	char *end;
	char *arg = *argv;

	switch(arg[1]) {
	case 'h':
		usage(progname);
		exit(0);
	case 'H':
		puts(score_header);
		for (char s[64]; next_score(s, sizeof s); ) {
			printf("%s\n", s);
		}
		exit(0);
	}

	char *v = (arg[1] && arg[2]) ? arg + 2 : argv[1];
	if (v == NULL || *v == '\0') {
		fprintf(stderr, "Option -%c requires an argument\n", arg[1]);
		exit(1);
	}

	switch(arg[1]) {
	case 'l':
		/* TODO: Convoluted spaghetti code will increment level
		 * once before the game begins, so subtract one here. */
		S->level = (int)strtol(v, &end, 0) - 1;
		for (int i = 1; i < S->level; i += 1 ){
			S->ms_per_tick *= S->decay_rate;
		}
		if (*end || S->level < 1) {
			fprintf(stderr, "Invalid level %s\n", v);
			exit(1);
		}
		break;
	case 'd':
		S->dictionary = v;
		break;
	case 's':
		S->choice = v;
		break;
	default:
		fprintf(stderr, "Unknown option: -%c\n", arg[1]);
		exit(1);
	}
	return (v == arg + 2) ? 1 : 2;
}


static void
parse_cmd_line(int argc, char **argv, struct state *S)
{
	char *progname = argv++[0];
	char *slash = strrchr(progname, '/');

	progname = slash ? slash + 1 : progname;

	for (char *arg = *argv; arg; arg = *argv) {
		if(arg[0] == '-' && arg[1] != '\0') {
			argv += handle_argument(S, argv, progname);
		} else {
			fprintf(stderr, "Unexpected argument: %s\n", arg);
			exit(1);
		}
	}
}


static void
initialize_words(struct state *S)
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
	unsetenv("COLUMNS");
	unsetenv("LINES");
	initialize_words(S);

	S->words = NULL;
	S->lives = 2;
	S->dictionary = NULL;
	S->addword = 1.0/18.0;
	S->decay_rate = .93;
	S->ms_per_tick = 250000;

	parse_cmd_line(argc, argv, S);

	initialize_dictionary(S->dictionary, S->choice, realloc);
	check_tty();

	set_handlers();
	srand48(time(NULL));
	srandom(time(NULL));
	initscr();
	raw();
	curs_set(0);
	noecho();
	keypad(stdscr, 1);
	clear();

	new_level(S);
	status(S);
	timeout(1000);
	gettimeofday(&S->start_time.game, NULL);
	gettimeofday(&S->start_time.level, NULL);
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
	banner(S, "Game Over", 3);

exit:
	free_dictionaries();
	set_timer(0);
	timeout(-1);
	if ( !S->dictionary && S->choice == NULL) {
		update_scores(&S->score, S->level);
	}
	show_scores(S);
	endwin();

	return 0;
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


static void
move_word(struct word *w)
{
	if ( ((tick % w->tick_per_move) != w->tick_mod) || w->killed) {
		return;
	}
	w->x +=  w->lateral / 3.0;
	if (w->x < 0.0) {
		w->x = 0.0;
		w->lateral *= -1;
	}
	if ((int)w->x > (COLS - w->word.len) + 1) {
		w->x = (float)(COLS - w->word.len);
		w->lateral *= -1;
	}
	w->y += 1;
}


/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(struct state *S)
{
	struct word *w = S->words;
	int  died = 0;

	while (w != NULL) {
		move_word(w);
		if (w->y > LINES - 1) {
			if (!w->killed) {
				w->killed = -3;
				died += 1;
			}
			w->y = LINES - 1;
		}
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

	move(w->y, (int)w->x);
	if (! w->killed ){
		assert(idx < w->word.len);
		attron(A_STANDOUT);
		addnstr(w->word.data, idx);
		attroff(A_STANDOUT);
		addstr(w->word.data + w->matches);
	} else {
		for (int i = 0; i < w->word.len - 1; i += 1) {
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
		S->ms_per_tick *= S->decay_rate;
		set_timer(S->ms_per_tick / 1000);
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
			if (w->matches == w->word.len - 1) {
				finalize_word(S, w);
				return;
			}
		} else {
			w->matches = key == w->word.data[0];
		}
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
		display_words(S);
	}
}


/* Word has been successfully typed.  Increment score and mark killed. */
static void
finalize_word(struct state *S, struct word *w)
{
	assert (w->word.data[w->matches] == '\0');
	S->score.points += w->word.len - 1 + (2 * S->level);
	S->score.words += 1;
	S->letters.game += w->word.len - 1;
	S->letters.level += w->word.len - 1;
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
	char *msg;
	struct timeval tp = {
		.tv_sec = 0,
		.tv_usec = delay_msec * 1000
	};
	while (tp.tv_usec > 999000) {
		tp.tv_sec += 1;
		tp.tv_usec -= 1000000;
	}
	struct itimerval old;
	struct itimerval t = {
		.it_interval = tp,
		.it_value = tp
	};
	if (getitimer(ITIMER_REAL, &old)) {
		msg = "getitimer";
		goto fail;
	}
	if (timerisset(&old.it_value) && timercmp(&old.it_value, &tp, <)) {
		t.it_value = old.it_value;
	}
	if (setitimer(ITIMER_REAL, &t, NULL)) {
		msg = "setitimer";
		goto fail;
	}
	return;
fail:
	endwin();
	perror(msg);
	exit(1);
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
				display_words(S);
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
	attron(A_STANDOUT);
	update_wpm(S);
#define STATUS_WIDTH ( 0\
	+ sizeof "Score:" + 7 \
	+ sizeof "Level:" + 3 \
	+ sizeof "Words:" + 6 \
	+ sizeof "Lives:" + 3 \
	+ sizeof "WPM:" + 9 \
	)
	move(0, COLS / 2 - (STATUS_WIDTH / 2));
#undef STATUS_WIDTH
	printw("Score: %-7u", S->score.points);
	printw("Level: %-3u", S->level);
	printw("Words: %-6u", S->score.words);
	printw("Lives: %-3d", S->lives);
	printw("WPM: %4d/%-4d", S->wpm.level, S->wpm.game);
	clrtoeol();
	attroff(A_STANDOUT);
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

/* return true iff (e - s ) > N */
static inline bool
timeval_span_gt(const struct timeval *e, const struct timeval *s, time_t N)
{
	time_t dsec  = e->tv_sec  - s->tv_sec;
	long   dusec = e->tv_usec - s->tv_usec;

	while (dusec < 0) {
		dsec  -= 1;
		dusec += 1E9L;
	}
	return dsec > N;
}

static int
compute_wpm(const struct timeval *end, const struct timeval *start, int k)
{
	struct timeval diff;

	if ( ! timeval_span_gt(end, start, 5)) {
		return 0;
	}
	timersub(end, start, &diff);
	double minutes = (diff.tv_sec + (diff.tv_usec / 1E9)) / 60.0;
	return (k / 5) / minutes;
}

static void
update_wpm(struct state *S)
{
	struct timeval now, diff;

	gettimeofday(&now, NULL);
	S->wpm.game = compute_wpm(&now, &S->start_time.game, S->letters.game);
	S->wpm.level = compute_wpm(&now, &S->start_time.level, S->letters.level);
}


/*
 * do stuff to change levels.  This is where special rounds can be stuck in.
 */
void
new_level(struct state *S)
{
	update_wpm(S);
	gettimeofday(&S->start_time.level, NULL);
	S->letters.level = 0;

	/*
	 * if we're inside a bonus round we don't need to change anything
	 * else so just take us out of the bonus round and exit this routine
	 */
	if (S->bonus) {
		S->bonus = false;
		banner(S, "Bonus round finished", 3);
		erase_word_list(S);
		status(S);
		return;
	}

	/*
	 * If you start at a level other than 1, the level does not
	 * actually change until you've completed a number of levels equal
	 * to the starting level.
	 */
	if(S->level <= S->levels_completed++)
		S->level += 1;

	S->ms_per_tick *= S->decay_rate;
	set_timer(S->ms_per_tick / 1000);

	display_words(S);
	if (S->score.words && ! ((S->levels_completed - 1) % LVL_PER_BONUS )) {
		S->bonus = true;
		erase_word_list(S);
		banner(S, "Prepare for bonus words", 3);
		S->lives += 1;
	}
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
static struct word *
maybe_add_word(struct state *S)
{
	if (! S->free) {
		return NULL;
	} else if (! word_in_play(S)) {
		return add_word(S);
	} else if ( (double)random() / RAND_MAX < S->addword) {
		return add_word(S);
	}
	return NULL;
}


static struct word *
add_word(struct state *S)
{
	struct word *n = S->free;
	S->free = n->next;
	int  len;

	n->word = S->bonus ? bonusword() : getword();
	len = n->word.len - 1;
	n->tick_per_move = len > 6 ? 3 : len > 3 ? 2 : 1;
	n->tick_mod = tick % n->tick_per_move;
	n->matches = 0;
	n->x = (float)(random() % ((COLS - 1) - (n->word.len - 1)));
	n->y = 1;
	n->lateral = random() % 19 - 9;
	n->next = NULL;
	n->killed = 0;
	putword(n);

	*lastnext(S) = n;
	return n;
}


/* momentarily display a banner message across the screen */
int
banner(struct state *S, const char *text, int delay_sec)
{
	int c = ERR;
	int len = strlen(text);

	set_timer(0);
#define HEIGHT 3
	WINDOW *boxw = newwin(HEIGHT, 6 + len, LINES / 3, (COLS - len) / 2 );
#undef HEIGHT

	box(boxw, 0, 0);

	mvwaddstr(boxw, 1, 3, text);
	wrefresh(boxw);
	refresh();
	if (delay_sec) {
		sleep(delay_sec);
	} else {
		timeout(-1);
		c = wgetch(boxw);
		timeout(1000);
	}
	set_timer(S->ms_per_tick / 1000);
	delwin(boxw);
	display_words(S);
	return c;
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
