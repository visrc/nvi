#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#define TRUE	1
#define FALSE	0

#define	ESCAPE	'\033'

#define	TAB	8				/* XXX */
#define	BLKSIZE	2048
#define	REG	register
#define	UCHAR(c)	((u_char)(c))

#define	inword(ch)	(isalnum(ch) || (ch) == '_')

/*------------------------------------------------------------------------*/
/* Miscellaneous constants.						  */

#define INFINITY	2000000001L	/* a very large integer */
#ifndef MAXRCLEN
# define MAXRCLEN	1000		/* longest possible .exrc file */
#endif

/*------------------------------------------------------------------------*/
/* These describe how temporary files are divided into blocks             */

#define MAXBLKS	(BLKSIZE / sizeof(unsigned short))
typedef union
{
	char		c[BLKSIZE];	/* for text blocks */
	unsigned short	n[MAXBLKS];	/* for the header block */
}
	BLK;

/*------------------------------------------------------------------------*/
/* These are used manipulate BLK buffers.                                 */

extern BLK	hdr;		/* buffer for the header block */
extern BLK	*blkget();	/* given index into hdr.c[], reads block */
extern BLK	*blkadd();	/* inserts a new block into hdr.c[] */

/*------------------------------------------------------------------------*/
/* These are used to keep track of various flags                          */
extern struct _viflags
{
	short	file;		/* file flags */
}
	viflags;

/* file flags */
#define NEWFILE		0x0001	/* the file was just created */
#define READONLY	0x0002	/* the file is read-only */
#define HADNUL		0x0004	/* the file contained NUL characters */
#define MODIFIED	0x0008	/* the file has been modified */
#define NONAME		0x0010	/* no name is known for the current text */
#define ADDEDNL		0x0020	/* newlines were added to the file */
#define HADBS		0x0040	/* backspace chars were lost from the file */

/* macros used to set/clear/test flags */
#define setflag(x,y)	viflags.x |= y
#define clrflag(x,y)	viflags.x &= ~y
#define tstflag(x,y)	(viflags.x & y)
#define initflags()	viflags.file = 0;

/*------------------------------------------------------------------------*/
/* These help support the single-line multi-change "undo" -- shift-U      */

extern char	U_text[BLKSIZE];
extern long	U_line;

typedef long MARK;

#define markline(x)	(long)((x) / BLKSIZE)
#define markidx(x)	(int)((x) & (BLKSIZE - 1))
#define MARK_UNSET	((MARK)0)
#define MARK_FIRST	((MARK)BLKSIZE)
#define MARK_LAST	((MARK)(nlines * BLKSIZE))
#define MARK_AT_LINE(x)	((MARK)((x) * BLKSIZE))

#define NMARKS	29
extern MARK	mark[NMARKS];	/* marks a-z, plus mark ' and two temps */
extern MARK	cursor;		/* mark where line is */

/*------------------------------------------------------------------------*/
/* These are used to keep track of the current & previous files.	  */

extern long	origtime;	/* modification date&time of the current file */
extern char	origname[256];	/* name of the current file */
extern char	prevorig[256];	/* name of the preceding file */
extern long	prevline;	/* line number from preceding file */

void blkflush();
void blkdirty();
MARK paste();
char *parseptrn();
char *fetchline();
/*------------------------------------------------------------------------*/
/* misc housekeeping variables & functions				  */

extern int	tmpfd;		/* fd used to access the tmp file */
extern int	tmpnum;		/* counter used to generate unique filenames */
extern long	lnum[MAXBLKS];	/* last line# of each block */
extern long	nlines;		/* number of lines in the file */
extern long	changes;	/* counts changes, to prohibit short-cuts */
extern BLK	tmpblk;		/* a block used to accumulate changes */
extern int	exwrote;	/* used to detect verbose ex commands */
extern int	doingdot;	/* boolean: are we doing the "." command? */
extern int	doingglobal;	/* boolean: are doing a ":g" command? */
#ifndef CRUNCH
extern int	force_lnmd;	/* boolean: force a command to work in line mode? */
#endif
extern long	rptlines;	/* number of lines affected by a command */
extern char	*rptlabel;	/* description of how lines were affected */
#define		ctrl(ch) ((ch)&037)

/*------------------------------------------------------------------------*/
/* macros that are used as control structures                             */

#define BeforeAfter(before, after) for((before),bavar=1;bavar;(after),bavar=0)
#define ChangeText	BeforeAfter(beforedo(FALSE),afterdo())

extern int	bavar;		/* used only in BeforeAfter macros */

/*------------------------------------------------------------------------*/
/* These are the movement commands.  Each accepts a mark for the starting */
/* location & number and returns a mark for the destination.		  */

extern MARK	m_updnto();		/* k j G */
extern MARK	m_right();		/* h */
extern MARK	m_left();		/* l */
extern MARK	m_tocol();		/* | */
extern MARK	m_front();		/* ^ */
extern MARK	m_rear();		/* $ */
extern MARK	m_fword();		/* w */
extern MARK	m_bword();		/* b */
extern MARK	m_eword();		/* e */
extern MARK	m_paragraph();		/* { } [[ ]] */
extern MARK	m_match();		/* % */
 extern MARK	m_fsentence();		/* ) */
 extern MARK	m_bsentence();		/* ( */
extern MARK	m_tomark();		/* 'm */
#ifndef NO_EXTENSIONS
extern MARK	m_wsrch();		/* ^A */
#endif
extern MARK	m_nsrch();		/* n */
extern MARK	m_Nsrch();		/* N */
extern MARK	m_fsrch();		/* /regexp */
extern MARK	m_bsrch();		/* ?regexp */
#ifndef NO_CHARSEARCH
 extern MARK	m__ch();		/* ; , */
 extern MARK	m_fch();		/* f */
 extern MARK	m_tch();		/* t */
 extern MARK	m_Fch();		/* F */
 extern MARK	m_Tch();		/* T */
#endif
extern MARK	m_row();		/* H L M */
extern MARK	m_z();			/* z */
extern MARK	m_scroll();		/* ^B ^F ^E ^Y ^U ^D */

/* Some stuff that is used by movement functions... */

extern MARK	adjmove();		/* a helper fn, used by move fns */

/* This macro is used to set the default value of cnt for an operation */
#define SETDEFCNT(val) { \
	if (cnt < 1) \
		cnt = (val); \
}

/* These are used to minimize calls to fetchline() */
extern int	plen;	/* length of the line */
extern long	pline;	/* line number that len refers to */
extern long	pchgs;	/* "changes" level that len refers to */
extern char	*ptext;	/* text of previous line, if valid */
extern void	pfetch();
extern char	digraph();

/* This is used to build a MARK that corresponds to a specific point in the
 * line that was most recently pfetch'ed.
 */
#define buildmark(text)	(MARK)(BLKSIZE * pline + (int)((text) - ptext))


/*------------------------------------------------------------------------*/

extern void	ex();
extern void	vi();

/*----------------------------------------------------------------------*/
/* These are used to handle VI commands 				*/

extern MARK	v_1ex();	/* : */
extern MARK	v_mark();	/* m */
extern MARK	v_quit();	/* Q */
extern MARK	v_redraw();	/* ^L ^R */
extern MARK	v_undo();	/* u */
extern MARK	v_xchar();	/* x X */
extern MARK	v_replace();	/* r */
extern MARK	v_overtype();	/* R */
extern MARK	v_selcut();	/* " */
extern MARK	v_paste();	/* p P */
extern MARK	v_yank();	/* y Y */
extern MARK	v_delete();	/* d D */
extern MARK	v_join();	/* J */
extern MARK	v_insert();	/* a A i I o O */
extern MARK	v_change();	/* c C */
extern MARK	v_subst();	/* s */
extern MARK	v_lshift();	/* < */
extern MARK	v_rshift();	/* > */
extern MARK	v_reformat();	/* = */
extern MARK	v_filter();	/* ! */
extern MARK	v_status();	/* ^G */
extern MARK	v_switch();	/* ^^ */
extern MARK	v_tag();	/* ^] */
extern MARK	v_xit();	/* ZZ */
extern MARK	v_undoline();	/* U */
extern MARK	v_again();	/* & */
#ifndef NO_EXTENSIONS
 extern MARK	v_keyword();	/* ^K */
 extern MARK	v_increment();	/* * */
#endif
#ifndef NO_ERRLIST
 extern MARK	v_errlist();	/* * */
#endif
#ifndef NO_AT
 extern MARK	v_at();		/* @ */
#endif

/*----------------------------------------------------------------------*/
/* These describe what mode we're in */

#define MODE_EX		1	/* executing ex commands */
#define	MODE_VI		2	/* executing vi commands */
#define	MODE_COLON	3	/* executing an ex command from vi mode */
#define	MODE_QUIT	4
extern int	mode;

#define WHEN_VICMD	0x0001	/* getkey: reading a VI command */
#define WHEN_VIINP	0x0002	/* getkey: in VI's INPUT mode */
#define WHEN_VIREP	0x0004	/* getkey: in VI's REPLACE mode */
#define WHEN_EX		0x0008	/* getkey: in EX mode */
#define WHEN_MSG	0x0010	/* getkey: at a "more" prompt */
#define WHEN_REP1	0x0040	/* getkey: getting a single char for 'r' */
#define WHEN_CUT	0x0080	/* getkey: getting a cut buffer name */
#define WHEN_MARK	0x0100	/* getkey: getting a mark name */
#define WHEN_CHAR	0x0200	/* getkey: getting a destination for f/F/t/T */
#define WHEN_FREE	0x2000	/* free the keymap after doing it once */
#define WHENMASK	(WHEN_VICMD|WHEN_VIINP|WHEN_VIREP|WHEN_REP1|WHEN_CUT|WHEN_MARK|WHEN_CHAR)

extern MARK	V_from;
extern int	V_linemd;
extern MARK	v_start();
