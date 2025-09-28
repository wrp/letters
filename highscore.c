/*
 * read and update high score file for Letter Invaders
 *
 * copyright 1991 by Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 */

/*
 * TODO
 * SIGWINCH and different sized screens make the high scores irrelevant
 * A player on a large screen has a huge advantage over a player with
 * fewer LINES.  Maybe track different high score files for different
 * screen sizes, and probably invalidate the game if SIGWINCH is ever
 * received.  Yes, that seems like a good idea.  Track the max value
 * of LINES over the course of the game and use the high-score list
 * for that screen size.  It's not clear how COLS impacts things. A tight
 * screen might be easier, so maybe track both.
 */


#include "letters.h"

#define SCORE_FMT "%5d %6d %6d"

static struct score_rec high_scores[10];
static struct stat	s_buf;
static time_t readtime;

int
read_scores(char *highscores)
{
	struct score_rec *h = high_scores;
	FILE		 *fp;

	/*
	 * get the last modified time so we know later if we have to reread
	 * the scores before saving a new file.  Only important on
	 * multiuser systems.
	 */
	if(
		stat(highscores, &s_buf) == -1 ||
		(fp = fopen(highscores, "r")) == NULL
	) {
		perror(highscores);
		return 1;
	}

	readtime = s_buf.st_mtime;

	while ( h - high_scores < 10 && 4 == fscanf(
		fp, "%8s %d %d %d", h->name, &h->level, &h->words, &h->score
	)) {
		h += 1;
	}

	fclose(fp);
	return 0;
}

int
write_scores(char *highscores) {
	int	i;
	FILE	*fp;

	/*
	 * check to make sure the high score list has not been modified
	 * since we read it.
	 */
	if( stat(highscores, &s_buf) == -1 ) {
		endwin();
		perror(highscores);
		exit(1);
	}

	if(s_buf.st_mtime > readtime)
		return -1;

	if((fp = fopen(highscores, "w")) == NULL) {
		endwin();
		perror(highscores);
		exit(1);
	}

	for (i = 0; i < 10; i += 1) {
		struct score_rec *h = high_scores + i;
		fprintf(fp, "%s %d %d %d\n",
			h->name,
			h->level,
			h->words,
			h->score
		);
	}

	return fclose(fp);
}



void
update_scores(char *name, struct score *score, unsigned level)
{
	int i, j;
	struct passwd *p;

	for(i = 0; i < 10; i++)
		if(score->points > high_scores[i].score) {
			for(j = 9; j > i; j--) {
				strcpy(high_scores[j].name, high_scores[j-1].name);
				high_scores[j].words = high_scores[j-1].words;
				high_scores[j].score = high_scores[j-1].score;
				high_scores[j].level = high_scores[j-1].level;
			}
			if((p = getpwuid(getuid())) == NULL)
                                strcpy(high_scores[i].name, "nobody");
			else
				strcpy(high_scores[i].name, p->pw_name);
			high_scores[i].score = score->points;
			high_scores[i].words = score->words;
			high_scores[i].level = level;
			if (write_scores(name) == -1) {
				read_scores(name);
				update_scores(name, score, level);
			}
			break;
		}
}


/* Iterate through the high scores, writing a printable string to buf */
struct score_rec *
next_score(char *buf, size_t siz)
{
	static int idx = 0;

	if (idx == sizeof high_scores / sizeof *high_scores) {
		idx = 0;
		return NULL;
	}

	struct score_rec *h = high_scores + idx;
	snprintf(buf, siz, "%3d %-10s" SCORE_FMT,
		idx + 1,
		h->name,
		h->level,
		h->words,
		h->score
	);

	idx += 1;

	return h;
}


void
show_scores(struct state *S)
{
	struct score_rec *h;
	erase();
	highlight(1);
	mvaddstr(5, 18, "Top Ten Scores for Letter Invaders");
	highlight(0);
	underline(1);
	mvaddstr(7, 20, "  name      level  words  score");
	underline(0);

	for (char s[64]; NULL != (h = next_score(s, sizeof s)); ) {
		int y = getcury(stdscr) + 1;
		mvaddstr(y, 18, s);
		if (h->score == S->score.points) {
			mvaddstr(y, 12, "---->");
			mvaddstr(y, 54, "<----");
		}
	}

	mvprintw(19, 21, "%-10s" SCORE_FMT,
		"Your score:",
		S->level,
		S->score.words,
		S->score.points
	);

	refresh();
	sleep(1);
	mvaddstr(21, 19, "Press any key to continue");
	while (getch() != ERR) {
		;
	}
	timeout(-1);
	getch();
}
