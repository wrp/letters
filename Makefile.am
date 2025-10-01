
bin_PROGRAMS = letters
letters_SOURCES = letters.c word.c highscore.c
noinst_HEADERS = letters.h
man6_MANS = letters.man
nodist_letters_SOURCES = dict.c
BUILT_SOURCES = dict.c
EXTRA_DIST = dict.c.in
CLEANFILES = dict.c

dict.c: $(srcdir)/dict.c.in
	test -r "$(DICTIONARY)" && \
	$(AM_V_GEN)$(AWK)  'k { \
		while (getline < "$(DICTIONARY)") { \
			if (length > 3 ) { \
				printf "\t{ \"%s\", %d },\n", $$0, length + 1 \
			} \
		} \
		k=0; next } 1; \
		/^static struct string words/{k=1}' \
		$(srcdir)/dict.c.in > $@.tmp && \
		test -s $@.tmp && mv $@.tmp $@
