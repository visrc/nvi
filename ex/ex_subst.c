/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_subst.c,v 5.9 1992/05/04 11:52:08 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:52:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>
#include <regexp.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

enum which {AGAIN, FIRST};
static void substitute __P((EXCMDARG *, enum which));

int
ex_subagain(cmdp)
	EXCMDARG *cmdp;
{
	substitute(cmdp, AGAIN);
	return (0);
}

int
ex_substitute(cmdp)
	EXCMDARG *cmdp;
{
	substitute(cmdp, FIRST);
	return (0);
}

static void
substitute(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	char	*line;	/* a line from the file */
	regexp	*re;	/* the compiled search expression */
	char	*subst;	/* the substitution string */
	char	*opt;	/* substitution options */
	long	l;	/* a line number */
	char	*s, *d;	/* used during subtitutions */
	char	*conf;	/* used during confirmation */
	long	chline;	/* # of lines changed */
	long	chsub;	/* # of substitutions made */
	static	optp;	/* boolean option: print when done? */
	static	optg;	/* boolean option: substitute globally in line? */
	static	optc;	/* boolean option: confirm before subst? */
#ifndef CRUNCH
	long	oldnlines;
#endif
	char lbuf[2048];
	char *extra;

	extra = cmdp->argv[0];

	/* for now, assume this will fail */
	rptlines = -1L;

	if (cmd == AGAIN) {
#ifndef NO_MAGIC
		if (ISSET(O_MAGIC))
			subst = "~";
		else
#endif
		subst = "\\~";
		re = regcomp("");

		/* if visual "&", then turn off the "p" and "c" options */
		if (cmdp->flags & E_FORCE)
		{
			optp = optc = 0;
		}
	} else {
		/* make sure we got a search pattern */
		if (*extra != '/' && *extra != '?')
		{
			msg("Usage: s/regular expression/new text/");
			return;
		}

		/* parse & compile the search pattern */
		subst = parseptrn(extra);
		re = regcomp(extra + 1);
	}

	/* abort if RE error -- error message already given by regcomp() */
	if (!re)
	{
		return;
	}

	if (cmd == FIRST) {
		/* parse the substitution string & find the option string */
		for (opt = subst; *opt && *opt != *extra; opt++)
		{
			if (*opt == '\\' && opt[1])
			{
				opt++;
			}
		}
		if (*opt)
		{
			*opt++ = '\0';
		}

		/* analyse the option string */
		if (!ISSET(O_EDCOMPATIBLE))
		{
			optp = optg = optc = 0;
		}
		while (*opt)
		{
			switch (*opt++)
			{
			  case 'p':	optp = !optp;	break;
			  case 'g':	optg = !optg;	break;
			  case 'c':	optc = !optc;	break;
			  case ' ':
			  case '\t':			break;
			  default:
				msg("Subst options are p, c, and g -- not %c", opt[-1]);
				return;
			}
		}
	}

	/* if "c" or "p" flag was given, and we're in visual mode, then NEWLINE */
	if ((optc || optp) && mode == MODE_VI)
	{
		addch('\n');
		refresh();
	}

	ChangeText
	{
		/* reset the change counters */
		chline = chsub = 0L;

		/* for each selected line */
		for (l = markline(cmdp->addr1); l <= markline(cmdp->addr2); l++)
		{
			/* fetch the line */
			line = fetchline(l, NULL);

			/* if it contains the search pattern... */
			if (regexec(re, line, 1))
			{
				/* increment the line change counter */
				chline++;

				/* initialize the pointers */
				s = line;
				d = lbuf;

				/* do once or globally ... */
				do
				{
#ifndef CRUNCH
					/* confirm, if necessary */
					if (optc)
					{
						for (conf = line; conf < re->startp[0]; conf++)
							addch(*conf);
						standout();
						for ( ; conf < re->endp[0]; conf++)
							addch(*conf);
						standend();
						for (; *conf; conf++)
							addch(*conf);
						addch('\n');
						refresh();
						if (getkey(0) != 'y')
						{
							/* copy accross the original chars */
							while (s < re->endp[0])
								*d++ = *s++;

							/* skip to next match on this line, if any */
							goto Continue;
						}
					}
#endif /* not CRUNCH */

					/* increment the substitution change counter */
					chsub++;

					/* copy stuff from before the match */
					while (s < re->startp[0])
					{
						*d++ = *s++;
					}

					/* substitute for the matched part */
					regsub(re, subst, d);
					s = re->endp[0];
					d += strlen(d);

Continue:
					/* if this regexp could conceivably match
					 * a zero-length string, then require at
					 * least 1 unmatched character between
					 * matches.
					 */
					if (re->minlen == 0)
					{
						if (!*s)
							break;
						*d++ = *s++;
					}

				} while (optg && regexec(re, s, 0));

				/* copy stuff from after the match */
				while (*d++ = *s++)	/* yes, ASSIGNMENT! */
				{
				}

#ifndef CRUNCH
				/* NOTE: since the substitution text is allowed to have ^Ms which are
				 * translated into newlines, it is possible that the number of lines
				 * in the file will increase after each line has been substituted.
				 * we need to adjust for this.
				 */
				oldnlines = nlines;
#endif

				/* replace the old version of the line with the new */
				d[-1] = '\n';
				d[0] = '\0';
				change(MARK_AT_LINE(l), MARK_AT_LINE(l + 1), lbuf);

#ifndef CRUNCH
				l += nlines - oldnlines;
				cmdp->addr2 += MARK_AT_LINE(nlines - oldnlines);
#endif

				/* if supposed to print it, do so */
				if (optp)
				{
					addstr(lbuf);
					refresh();
				}

				/* move the cursor to that line */
				cursor = MARK_AT_LINE(l);
			}
		}
	}

	/* free the regexp */
	free(re);

	/* if done from within a ":g" command, then finish silently */
	if (doingglobal)
	{
		rptlines = chline;
		rptlabel = "changed";
		return;
	}

	/* Reporting */
	if (chsub == 0)
	{
		msg("Substitution failed");
	}
	else if (chline >= LVAL(O_REPORT))
	{
		msg("%ld substitutions on %ld lines", chsub, chline);
	}
	rptlines = 0L;

	autoprint = 1;
}
