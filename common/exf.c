/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.3 1992/05/03 08:27:16 bostic Exp $ (Berkeley) $Date: 1992/05/03 08:27:16 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

static EXFLIST exfhdr;				/* List of files. */
EXF *curf;					/* Current file. */

#ifdef FILE_LIST_DEBUG
void file_show __P((char *));
#endif

/*
 * file_init --
 *	Initialize the file list.
 */
void
file_init()
{
	exfhdr.next = exfhdr.prev = (EXF *)&exfhdr;
}

/*
 * file_default --
 *	Add the default "no-name" structure to the file list.
 */
int
file_default()
{
	EXF *ep;

	if ((ep = malloc(sizeof(EXF))) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}
	
	ep->name = NULL;
	ep->pos = MARK_FIRST;
	ep->flags = F_NONAME;
	insexf(ep, &exfhdr);

	curf = ep;
	return (0);
}

/*
 * file_ins --
 *	Insert a new file into the list, after the current file.
 */
int
file_ins(name)
	char *name;
{
	EXF *ep;

	if ((ep = malloc(sizeof(EXF))) == NULL)
		goto err;
	if ((ep->name = strdup(name)) == NULL) {
		free(ep);
err:		msg("Error: %s", strerror(errno));
		return (1);
	}

	ep->pos = MARK_FIRST;
	ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0;
	insexf(ep, curf);
	return (0);
}

/*
 * file_set --
 *	Set the file list from an argc/argv.
 */
int
file_set(argc, argv)
	int argc;
	char *argv[];
{
	EXF *ep;

	/* Free up any previous list. */
	while (exfhdr.next != (EXF *)&exfhdr) {
		ep = exfhdr.next;
		rmexf(ep);
		if (ep->name)
			free(ep->name);
		free(ep);
		file_show("remove: ");
	}

	/* Insert new entries at the tail of the list. */
	for (; *argv; ++argv) {
		if ((ep = malloc(sizeof(EXF))) == NULL)
			goto err;
		if ((ep->name = strdup(*argv)) == NULL) {
			free(ep);
err:			msg("Error: %s", strerror(errno));
			return (1);
		}
		ep->pos = MARK_FIRST;
		ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0;
		instailexf(ep, &exfhdr);
	}

	curf = file_first();
	return (0);
}

/*
 * file_iscurrent --
 *	Return if the filename is the same as the current one.
 */
int
file_iscurrent(name)
	char *name;
{
	char *p;

	/*
	 * XXX
	 * Compare the dev/ino , not just the name; then, quit looking at
	 * just the file name (see parse_err in ex_errlist.c).
	 */
	if ((p = rindex(curf->name, '/')) == NULL)
		p = curf->name;
	else
		++p;
	return (!strcmp(p, name));
}


/*
 * file_first --
 *	Return the first file.
 */
EXF *
file_first()
{
	return (exfhdr.next);
}

/*
 * file_next --
 *	Return the next file, if any.
 */
EXF *
file_next(ep)
	EXF *ep;
{
	return (ep->next != (EXF *)&exfhdr ? ep->next : NULL);
}

/*
 * file_prev --
 *	Return the previous file, if any.
 */
EXF *
file_prev(ep)
	EXF *ep;
{
	return (ep->prev != (EXF *)&exfhdr ? ep->prev : NULL);
}

#ifdef FILE_LIST_DEBUG
void
file_show(s)
	char *s;
{
	register EXF *ep;

	(void)printf("%s", s);
	for (ep = exfhdr.next; ep != (EXF *)&exfhdr; ep = ep->next)
		if (ep == curf)
			(void)printf("[%s] ", ep->name ? ep->name : "NONAME");
		else
			(void)printf("{%s} ", ep->name ? ep->name : "NONAME");
	(void)printf("\n");
}
#endif

/* This function creates the temp file and copies the original file into it.
 * Returns if successful, or stops execution if it fails.
 */
int tmpstart(filename)
	char		*filename; /* name of the original file */
{
	int		origfd;	/* fd used for reading the original file */
	struct stat	statb;	/* stat buffer, used to examine inode */
	REG BLK		*this;	/* pointer to the current block buffer */
	REG BLK		*next;	/* pointer to the next block buffer */
	int		inbuf;	/* number of characters in a buffer */
	int		nread;	/* number of bytes read */
	REG int		j, k;
	int		i;
	long		nbytes;
	char tmpname[1024];

	/* switching to a different file certainly counts as a change */
	++changes;

	/* open the original file for reading */
	*origname = '\0';
	if (filename && *filename)
	{
		strcpy(origname, filename);
		origfd = open(origname, O_RDONLY);
		if (origfd < 0 && errno != ENOENT)
		{
			msg("Can't open \"%s\"", origname);
			return tmpstart("");
		}
		if (origfd >= 0)
		{
			if (stat(origname, &statb) < 0)
			{
				mode = MODE_EX;
				msg("Can't stat \"%s\"", origname);
				endwin();
				exit(1);
			}
			if (origfd >= 0 && (statb.st_mode & S_IFMT) != S_IFREG)
			{
				msg("\"%s\" is not a regular file", origname);
				return tmpstart("");
			}
		}
		else
		{
			stat(".", &statb);
		}
		if (origfd >= 0)
		{
			origtime = statb.st_mtime;
			if (ISSET(O_READONLY) || !(statb.st_mode &
				  (statb.st_uid != geteuid() ? 0022 : 0200)))
			{
				setflag(file, READONLY);
			}
		}
		else
		{
			origtime = 0L;
		}
	}
	else
	{
		setflag(file, NONAME);
		origfd = -1;
		origtime = 0L;
		stat(".", &statb);
	}

	/* make a name for the tmp file */
	tmpnum++;
	sprintf(tmpname, _PATH_TMPNAME, PVAL(O_DIRECTORY), getpid(), tmpnum);

	/* make sure nobody else is editing the same file */
	if (access(tmpname, 0) == 0)
	{
		mode = MODE_EX;
		msg("Temp file \"%s\" already exists?", tmpname);
		endwin();
		exit(1);
	}

	/* create the temp file */
	close(creat(tmpname, S_IRUSR|S_IWUSR));	/* only we can read it */
	tmpfd = open(tmpname, O_RDWR);
	if (tmpfd < 0)
	{
		mode = MODE_EX;
		msg("Can't create temp file... Does directory \"%s\" exist?",
		    PVAL(O_DIRECTORY));
		endwin();
		exit(1);
	}

	/* allocate space for the header in the file */
	write(tmpfd, hdr.c, (unsigned)BLKSIZE);
	write(tmpfd, tmpblk.c, (unsigned)BLKSIZE);

	/* initialize the block allocator */
	/* This must already be done here, before the first attempt
	 * to write to the new file! GB */
	garbage();

	/* initialize lnum[] */
	for (i = 1; i < MAXBLKS; i++)
	{
		lnum[i] = INFINITY;
	}
	lnum[0] = 0;

	/* if there is no original file, then create a 1-line file */
	if (origfd < 0)
	{
		hdr.n[0] = 0;	/* invalid inode# denotes new file */

		this = blkget(1); 	/* get the new text block */
		strcpy(this->c, "\n");	/* put a line in it */

		lnum[1] = 1L;	/* block 1 ends with line 1 */
		nlines = 1L;	/* there is 1 line in the file */
		nbytes = 1L;

		if (*origname)
		{
			msg("\"%s\" [NEW FILE]  1 line, 1 char", origname);
		}
		else
		{
			msg("\"[NO FILE]\"  1 line, 1 char");
		}
	}
	else /* there is an original file -- read it in */
	{
		nbytes = nlines = 0;

		/* preallocate 1 "next" buffer */
		i = 1;
		next = blkget(i);
		inbuf = 0;

		/* loop, moving blocks from orig to tmp */
		for (;;)
		{
			/* "next" buffer becomes "this" buffer */
			this = next;

			/* read [more] text into this block */
			nread = read(origfd, &this->c[inbuf], BLKSIZE - 1 - inbuf);
			if (nread < 0)
			{
				close(origfd);
				close(tmpfd);
				tmpfd = -1;
				unlink(tmpname);
				mode = MODE_EX;
				msg("Error reading \"%s\"", origname);
				endwin();
				exit(1);
			}

			/* convert NUL characters to something else */
			for (j = k = inbuf; k < inbuf + nread; k++)
			{
				if (!this->c[k])
				{
					setflag(file, HADNUL);
					this->c[j++] = 0x80;
				}
				else if (ISSET(O_BEAUTIFY) && this->c[k] < ' ' && this->c[k] > 0)
				{
					if (this->c[k] == '\t'
					 || this->c[k] == '\n'
					 || this->c[k] == '\f')
					{
						this->c[j++] = this->c[k];
					}
					else if (this->c[k] == '\b')
					{
						/* delete '\b', but complain */
						setflag(file, HADBS);
					}
					/* else silently delete control char */
				}
				else
				{
					this->c[j++] = this->c[k];
				}
			}
			inbuf = j;

			/* if the buffer is empty, quit */
			if (inbuf == 0)
			{
				goto FoundEOF;
			}

			/* search backward for last newline */
			for (k = inbuf; --k >= 0 && this->c[k] != '\n';)
			{
			}
			if (k++ < 0)
			{
				if (inbuf >= BLKSIZE - 1)
				{
					k = 80;
				}
				else
				{
					k = inbuf;
				}
			}

			/* allocate next buffer */
			next = blkget(++i);

			/* move fragmentary last line to next buffer */
			inbuf -= k;
			for (j = 0; k < BLKSIZE; j++, k++)
			{
				next->c[j] = this->c[k];
				this->c[k] = 0;
			}

			/* if necessary, add a newline to this buf */
			for (k = BLKSIZE - inbuf; --k >= 0 && !this->c[k]; )
			{
			}
			if (this->c[k] != '\n')
			{
				setflag(file, ADDEDNL);
				this->c[k + 1] = '\n';
			}

			/* count the lines in this block */
			for (k = 0; k < BLKSIZE && this->c[k]; k++)
			{
				if (this->c[k] == '\n')
				{
					nlines++;
				}
				nbytes++;
			}
			lnum[i - 1] = nlines;
		}
FoundEOF:

		/* if this is a zero-length file, add 1 line */
		if (nlines == 0)
		{
			this = blkget(1); 	/* get the new text block */
			strcpy(this->c, "\n");	/* put a line in it */

			lnum[1] = 1;	/* block 1 ends with line 1 */
			nlines = 1;	/* there is 1 line in the file */
			nbytes = 1;
		}

		/* report the number of lines in the file */
		msg("\"%s\" %s %ld line%s, %ld char%s",
			origname,
			(tstflag(file, READONLY) ? "[READONLY]" : ""),
			nlines,
			nlines == 1 ? "" : "s",
			nbytes,
			nbytes == 1 ? "" : "s");
	}

	/* initialize the cursor to start of line 1 */
	cursor = MARK_FIRST;

	/* close the original file */
	close(origfd);

	/* any other messages? */
	if (tstflag(file, HADNUL))
	{
		msg("This file contained NULs.  They've been changed to \\x80 chars");
	}
	if (tstflag(file, ADDEDNL))
	{
		msg("Newline characters have been inserted to break up long lines");
	}
	if (tstflag(file, HADBS))
	{
		msg("Backspace characters deleted due to ':set beautify'");
	}

	/* force all blocks out onto the disk, to support file recovery */
	blksync();

	return 0;
}



/* This function copies the temp file back onto an original file.
 * Returns TRUE if successful, or FALSE if the file could NOT be saved.
 */
int tmpsave(filename, bang)
	char	*filename;	/* the name to save it to */
	int	bang;		/* forced write? */
{
	int		fd;	/* fd of the file we're writing to */
	REG int		len;	/* length of a text block */
	REG BLK		*this;	/* a text block */
	long		bytes;	/* byte counter */
	REG int		i;

	/* if no filename is given, assume the original file name */
	if (!filename || !*filename)
	{
		filename = origname;
	}

	/* if still no file name, then fail */
	if (!*filename)
	{
		msg("Don't know a name for this file -- NOT WRITTEN");
		return FALSE;
	}

	/* can't rewrite a READONLY file */
	if (!strcmp(filename, origname) && ISSET(O_READONLY) && !bang)
	{
		msg("\"%s\" [READONLY] -- NOT WRITTEN", filename);
		return FALSE;
	}

	/* open the file */
	if (*filename == '>' && filename[1] == '>')
	{
		filename += 2;
		while (*filename == ' ' || *filename == '\t')
		{
			filename++;
		}
		fd = open(filename, O_WRONLY|O_APPEND);
	}
	else
	{
		/* either the file must not exist, or it must be the original
		 * file, or we must have a bang, or "writeany" must be set.
		 */
		if (strcmp(filename, origname) && access(filename, 0) == 0 && !bang
		    && !ISSET(O_WRITEANY)
				   )
		{
			msg("File already exists - Use :w! to overwrite");
			return FALSE;
		}
		fd = creat(filename, DEFFILEMODE);
	}
	if (fd < 0)
	{
		msg("Can't write to \"%s\" -- NOT WRITTEN", filename);
		return FALSE;
	}

	/* write each text block to the file */
	bytes = 0L;
	for (i = 1; i < MAXBLKS && (this = blkget(i)) && this->c[0]; i++)
	{
		for (len = 0; len < BLKSIZE && this->c[len]; len++)
		{
		}
		write(fd, this->c, len);
		bytes += len;
	}

	/* reset the "modified" flag */
	clrflag(file, MODIFIED);

	/* report lines & characters */
	msg("Wrote \"%s\"  %ld lines, %ld characters", filename, nlines, bytes);

	/* close the file */
	close(fd);

	return TRUE;
}


/* This function deletes the temporary file.  If the file has been modified
 * and "bang" is FALSE, then it returns FALSE without doing anything; else
 * it returns TRUE.
 *
 * If the "autowrite" option is set, then instead of returning FALSE when
 * the file has been modified and "bang" is false, it will call tmpend().
 */
int tmpabort(bang)
	int	bang;
{
	/* if there is no file, return successfully */
	if (tmpfd < 0)
	{
		return TRUE;
	}

	/* see if we must return FALSE -- can't quit */
	if (!bang && tstflag(file, MODIFIED))
	{
		/* if "autowrite" is set, then act like tmpend() */
		if (ISSET(O_AUTOWRITE))
			return tmpend(bang);
		else
			return FALSE;
	}

	/* delete the tmp file */
	cutswitch();
	strcpy(prevorig, origname);
	prevline = markline(cursor);
	*origname = '\0';
	origtime = 0L;
	blkinit();
	nlines = 0;
	initflags();
	return TRUE;
}

/* This function saves the file if it has been modified, and then deletes
 * the temporary file. Returns TRUE if successful, or FALSE if the file
 * needs to be saved but can't be.  When it returns FALSE, it will not have
 * deleted the tmp file, either.
 */
int tmpend(bang)
	int	bang;
{
	/* save the file if it has been modified */
	if (tstflag(file, MODIFIED) && !tmpsave((char *)0, FALSE) && !bang)
	{
		return FALSE;
	}

	/* delete the tmp file */
	tmpabort(TRUE);

	return TRUE;
}
