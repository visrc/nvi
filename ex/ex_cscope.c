/*
 * Copyright (c) 1994 Rob Mayoff.  All rights reserved.
 *
 * This code was created by mayoff@tkg.com on Tue May 10 21:59:22 CDT 1994
 */

#ifndef lint
static char sccsid[] = "@(#)ex_cscope.c";
#endif /* not lint */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include "vi.h"
#include "excmd.h"
#include "cscope.h"

#define CSCOPE_CMD_FMT "cd %s && exec cscope -dql"
#define CSCOPE_DBFILE "cscope.out"
#define CSCOPE_PATHS "cscope.tpath"
#define CSCOPE_PROMPT ">> "
#define CSCOPE_QUERIES "sgdct efi"
#define CSCOPE_NQUERIES ((sizeof CSCOPE_QUERIES) - 1)
#define CSCOPE_NLINES_FMT "cscope: %d lines%1[\n]"

/*
 * Each space in the source line printed by cscope represents an
 * arbitrary sequence of spaces, tabs, and comments.
 */
#define CSCOPE_RE_SPACE "([ \t]|/\\*([^*]|\\*/)*\\*/)*"

/* 0name	find all uses of name */
/* 1name	find definition of name */
/* 2name	find all function calls made from name */
/* 3name	find callers of name */
/* 4string	find text string (cscope 12.9) */
/* 4name	find assignments to name (cscope 13.3) */
/* 5pattern	change pattern - NOT USED */
/* 6pattern	find pattern */
/* 7name	find files with name as substring */
/* 8name	find files #including name */

static int cscope_help __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_add __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_reset __P((SCR *, EXF *, EXCMDARG *));
static int cscope_find __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_push __P((SCR *, EXF *, EXCMDARG *));
static int cscope_next __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_pop __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_show __P((SCR *, EXF *, EXCMDARG *, char *));
static int cscope_kill __P((SCR *, EXF *, EXCMDARG *, char *));
static int kill_by_number __P((SCR *, EXF *, int));

typedef struct {
  char *name;
  int (*function)();
  char *help_msg;
  char *usage_msg;
}	cscope_cmd;

cscope_cmd cscope_cmds[] =
{
  { "add",   cscope_add, "Add a new C-Scope database", "add db-name" },
  { "find",  cscope_find,
	"Query the databases for a pattern", "find g|s|f|i|c|d|e|t pattern" },
  { "help",  cscope_help,
	"Show help for cscope subcommands", "help [command]" },
  { "kill", cscope_kill,
  	"Kill a C-Scope connection", "kill number" },
  { "next",  cscope_next,
    "Go to another result from the latest query", "next [ [+-]number]" },
  { "pop",   cscope_pop,
    "Return to the previous location on the cscope stack",
    "pop [number]" },
  { "push",  cscope_push, "Push the current location onto the stack", "push" },
  { "reset", cscope_reset,
    "Destroy all current C-Scope connections", "reset" },
  { "show",  cscope_show,
    "Show current C-Scope connections, query stack, or query results",
    "show connections|stack|results" },
};
#define NCMDS ((sizeof cscope_cmds)/(sizeof(cscope_cmd)))

cscope_cmd *
lookup_ccmd(name)
  char *name;
{
  int i, l = strlen(name);
  for(i = 0; i < NCMDS; i++)
    if(strncmp(name, cscope_cmds[i].name, l) == 0)
      return cscope_cmds+i;

  return 0;
}

static int
start_cscopes(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  char *cscopes, *p;
  char cscope_path[PATH_MAX];

  sp->gp->cs_started = 1;

  cscopes = getenv("CSCOPE_DIRS");
  if(!cscopes) return 0;

  while(*cscopes)
  {
    p = strchr(cscopes, ' ');
    if(p)
    {
      strncpy(cscope_path, cscopes, p - cscopes);
      cscope_path[p-cscopes] = 0;
      cscopes = p + 1;
    }
    else
    {
      strcpy(cscope_path, cscopes);
      cscopes = "";
    }
    if(cscope_path[0])
      cscope_add(sp, ep, cmdp, cscope_path);
  }

  return 0;
}

/*
 * ex_cscope --
 *	Perform a C-Scope operation.
 */
int
ex_cscope(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  char *cmd, *p;
  int i, ch, clen;
  cscope_cmd *cc;

  if(!sp->gp->cs_started && start_cscopes(sp, ep, cmdp))
      return 1;

  /* First, figure out what cscope subcommand is being issued. */
  /* cmdp->argc will always be 1 for this command. */

  if(cmdp->argv[0]->len == 0)
  {
usage:
    msgq(sp, M_ERR, "Use \"cscope help\" for help");
    return 1;
  }

  for(p = cmd = cmdp->argv[0]->bp, i = cmdp->argv[0]->len; i; i--, p++)
  {
    ch = *p;
    if(isspace(ch))
      break;
  }
  if(*p)
  {
    *p++ = 0;
    while(*p && isspace(*p))
      p++;
  }

  cc = lookup_ccmd(cmd);
  if(cc)
    return cc->function(sp, ep, cmdp, p);

  msgq(sp, M_ERR, "Usage \"cscope help\" for help");
  return 1;
}

static int
stat_db(sp, csc)
  SCR *sp;
  CSC *csc;
{
  struct stat sb;
  int rc, fn_len;
  char *fn;

  /* Figure out the name of the database file. */
  fn_len = strlen(csc->name) + 1 + sizeof CSCOPE_DBFILE;
  MALLOC_RET(sp, fn, char *, fn_len);
  snprintf(fn, fn_len, "%s/%s", csc->name, CSCOPE_DBFILE);

  rc = stat(fn, &sb);
  if(rc < 0)
  {
    msgq(sp, M_SYSERR, fn);
    return 1;
  }

  csc->db_time = sb.st_mtime;
  free(fn);
  return 0;
}

static int
get_paths(sp, csc)
  SCR *sp;
  CSC *csc;
{
  char *fn;
  int fn_len, rc;
  FILE *fp = 0;
  struct stat sb;
  char *rawpaths, *p;
  int raw_len;
  char cwd[PATH_MAX];

  /*
   * Look in the cscope directory for a file named CSCOPE_PATHS.  It will
   * contain a colon-separated list of paths in which to search for
   * files returned by C-Scope.  If the file doesn't exist, use the
   * C-Scope directory as the search path.
   *
   * If the cscope directory is a relative path, absolutify it and
   * use it as the search path.
   */

  if(csc->name[0] == '/')
  {
    /* Absolute path. */
    fn_len = strlen(csc->name) + 1 + sizeof CSCOPE_PATHS;
    MALLOC_RET(sp, fn, char *, fn_len);
    snprintf(fn, fn_len, "%s/%s", csc->name, CSCOPE_PATHS);

    rc = stat(fn, &sb);
    if(rc < 0)
    {
      if(errno == ENOENT)
      {
        /* No CSCOPE_PATHS file?  Just use the cscope dir. */
        MALLOC_RET(sp, csc->path, char **, sizeof(char *));
        csc->path[0] = strdup(csc->name);
        if(!csc->path[0])
        {
          msgq(sp, M_SYSERR, NULL);
          return 1;
        }
        csc->n_paths = 1;

        return 0;
      }
      else
      {
        msgq(sp, M_SYSERR, fn);
        return 1;
      }
    }

    raw_len = sb.st_size + 1;
    MALLOC_RET(sp, rawpaths, char *, raw_len);

    fp = fopen(fn, "r");
    if(!fp) goto badfile;

    if(!fgets(rawpaths, raw_len, fp))
    {
badfile:
      msgq(sp, M_SYSERR, fn);
      if(fp) fclose(fp);
      return 1;
    }

    fclose(fp); fp = 0;

    /*
     * It's easier to just allocate what we know will be more than
     * enough space than to try to figure out exactly how much space
     * to allocate.
     */
    MALLOC_RET(sp, csc->path, char **, sizeof(char *) * raw_len);

    for(csc->n_paths = 0, p = strtok(rawpaths, ": \t\n");
	p; csc->n_paths++, p = strtok(0, ": \t\n"))
      csc->path[csc->n_paths] = p;

    /* Do *NOT* free rawpaths - we used its storage for csc->path! */
  }
  else
  {
    /* Relative path.  Prepend current dir to csc->name. */
    if(!getwd(cwd))
    {
      msgq(sp, M_ERR, "getwd: %s", cwd);
      return 1;
    }

    fn_len = strlen(cwd) + 1 + strlen(csc->name) + 1;
    MALLOC_RET(sp, fn, char *, fn_len);
    snprintf(fn, fn_len, "%s/%s", cwd, csc->name);
    MALLOC_RET(sp, csc->path, char **, sizeof(char *));
    csc->path[0] = fn;
    csc->n_paths = 1;
  }

  return 0;
}

static int
spawn_cscope(sp, csc)
  SCR *sp;
  CSC *csc;
{
  char *cmd;
  int cmd_len, rc, i;
  int to_cs[2], from_cs[2];

  /* Set up the C-Scope command. */
  cmd_len = sizeof CSCOPE_CMD_FMT + strlen(csc->name);
  MALLOC_RET(sp, cmd, char *, cmd_len);
  snprintf(cmd, cmd_len, CSCOPE_CMD_FMT, csc->name);

  if(pipe(to_cs) < 0 || pipe(from_cs) < 0)
  {
    msgq(sp, M_SYSERR, "pipe");
    return 1;
  }
  
  switch(csc->pid = vfork())
  {
    case -1:
      msgq(sp, M_SYSERR, "vfork");
      close(to_cs[0]);
      close(to_cs[1]);
      close(from_cs[0]);
      close(from_cs[1]);
      return 1;

    case 0:		/* child side - run C-Scope. */
      close(to_cs[1]);
      close(from_cs[0]);
      dup2(to_cs[0], 0);
      dup2(from_cs[1], 1);
      dup2(from_cs[1], 2);
      for(i = 3; i < 100; i++)
        close(i);

      rc = execl("/bin/sh", "/bin/sh", "-c", cmd);
      msgq(sp, M_SYSERR, "execl");
      /*NOTREACHED*/

    default:		/* parent side. */
      close(to_cs[0]);
      close(from_cs[1]);
      rc = fcntl(to_cs[1], F_SETFL, FNDELAY);
      if(rc < 0)
      {
        msgq(sp, M_SYSERR, "FNDELAY");
        close(to_cs[1]);
	close(from_cs[0]);
        return 1;
      }
      csc->to_fp = fdopen(to_cs[1], "w");
      csc->from_fp = fdopen(from_cs[0], "r");
      break;
  }

  free(cmd);
  return 0;
}

static int
read_prompt(sp, csc)
  SCR *sp;
  CSC *csc;
{
  char prompt_buf[sizeof CSCOPE_PROMPT];

do_read:
  if(!fgets(prompt_buf, sizeof prompt_buf, csc->from_fp))
  {
    if(errno == EINTR && !F_ISSET(sp, S_INTERRUPTED))
    {
      /* Interrupted by busy msg.  Try again. */
      goto do_read;
    }
    msgq(sp, M_SYSERR, csc->name);
    return 1;
  }

  if(strcmp(prompt_buf, CSCOPE_PROMPT) != 0)
  {
    msgq(sp, M_ERR, "%s: bad prompt", csc->name);
    return 1;
  }
}

int
cscope_add(sp, ep, cmdp, db_name)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *db_name;
{
  GS *gp;
  EX_PRIVATE *exp;
  CSC *csc;

  exp = EXP(sp);
  gp = sp->gp;

  /* Allocate a new C-Scope connection struct. */
  MALLOC_RET(sp, csc, CSC *, sizeof(CSC));
  csc->to_fp = csc->from_fp = 0;
  csc->pid = -1;
  csc->n_paths = 0; csc->path = 0;
  if(!(csc->name = strdup(db_name)))
  {
nomem:
    msgq(sp, M_SYSERR, NULL);
    return 1;
  }

  /* Stat the database file to get it's timestamp. */
  if(stat_db(sp, csc)) return 1;

  /* Get the search paths for the C-Scope. */
  if(get_paths(sp, csc)) return 1;

  /* Spawn the C-Scope process. */
  if(spawn_cscope(sp, csc)) return 1;

  /* Add the C-Scope to the list. */
  LIST_INSERT_HEAD(&gp->cscq, csc, q);

  /* Read the initial prompt from the C-Scope to make sure it's okay. */
  if(read_prompt(sp, csc))
  {
    kill_by_number(sp, ep, 1);
    return 1;
  }

  return 0;
}

static int
create_cs_cmd(sp, ep, pattern)
  SCR *sp;
  EXF *ep;
  char *pattern;
{
  EX_PRIVATE *exp;
  char *p, *q;
  int search_type = 0;
  CB *cb;
  int p_len, csq_len;

  exp = EXP(sp);

  if(!pattern[0])
  {
    msgq(sp, M_ERR, "Please specify a search type and pattern");
    return 1;
  }

  /*
   * C-Scope supports a "change pattern" command which we never use.
   * It's C-Scope command 5.  We set CSCOPE_QUERIES[5] to " " since
   * the user can't pass " " as the first character of pattern.
   * That way the user can't ask for pattern 5 so we don't need any
   * special-case code.
   */
  p = strchr(q = CSCOPE_QUERIES, pattern[0]);
  if(!p)
  {
    msgq(sp, M_ERR, "Unknown search type %c", pattern[0]);
    return 1;
  }
  search_type = p - q;

  /* Skip on to the pattern. */
  p = pattern + 1;
  while(*p && *p == ' ')
    p++;
  if(!*p)
  {
    msgq(sp, M_ERR, "Specify a search pattern as well as a type");
    return 1;
  }

  /* See if the user specified a buffer (i.e., doublequote+letter). */
  if(*p == '"')
  {
    /* Yep.  Fetch the buffer contents as the pattern. */
    p++;
    if(!*p)
    {
      msgq(sp, M_ERR, "Specify a buffer name after `\"'");
      return 1;
    }
    CBNAME(sp, cb, p[0]);
    if(!cb)
    {
      msgq(sp, M_ERR, "No cut buffer named `%c'", *p);
      return 1;
    }

    p = cb->textq.cqh_first->lb;
    p_len = cb->textq.cqh_first->len;
  }
  else
  {
    /* p points to the pattern. */
    p_len = strlen(p);
  }

  /* Create the C-Scope command. */
  csq_len = p_len + 3;
  if(exp->csqlast) free(exp->csqlast);
  MALLOC_RET(sp, exp->csqlast, char *, csq_len);
  snprintf(exp->csqlast, csq_len, "%1d", search_type);
  memcpy(exp->csqlast + 1, p, p_len);
  exp->csqlast[1+p_len] = '\n';
  exp->csqlast[2+p_len] = 0;

  return 0;
}

static int
parse_nlines(buf)
  char *buf;
{
  int n, rc;
  char dummy[2];

  rc = sscanf(buf, CSCOPE_NLINES_FMT, &n, dummy);
  if(rc != 2)
    return -1;
  
  return n;
}

static char *
make_regexp(sp, str)
  SCR *sp;
  char *str;
/* Turn the C-Scope pattern in str into a regexp. */
{
  int i, l, nspaces, re_len, eflag;
  char *re, *p;
  char *re_magic;

  eflag = O_ISSET(sp, O_EXTENDED);
  l = strlen(str);

  for(i = nspaces = 0; i < l; i++)
    if(str[i] == ' ') nspaces++;

  /* Allocate plenty of space. */
  re_len = 1 + sizeof CSCOPE_RE_SPACE * (nspaces+2)
    + l*2 + 2;
  MALLOC(sp, re, char *, re_len);
  if(!re) return 0;

  p = re;
  *p++ = '^';
  strcpy(p, CSCOPE_RE_SPACE);
  p += sizeof CSCOPE_RE_SPACE - 1;
  for(i = 0; i < l; i++)
  {
    if(str[i] == ' ')
    {
      strcpy(p, CSCOPE_RE_SPACE);
      p += sizeof CSCOPE_RE_SPACE - 1;
    }
    else if(eflag)
    {
      if(!isalnum(str[i]))
        *p++ = '\\';
      *p++ = str[i];
    }
    else
    {
      if(strchr("\\^$*.[]", str[i]))
        *p++ = '\\';
      *p++ = str[i];
    }
  }
  strcpy(p, CSCOPE_RE_SPACE);
  p += sizeof CSCOPE_RE_SPACE - 1;
  *p++ = '$';
  *p = 0;

  return re;
}

static char *
find_file(sp, csc, short_fn)
  SCR *sp;
  CSC *csc;
  char *short_fn;
{
  char *fn;
  int i;

  MALLOC(sp, fn, char *, PATH_MAX);
  for(i = 0; i < csc->n_paths; i++)
  {
    snprintf(fn, PATH_MAX, "%s/%s", csc->path[i], short_fn);
    if(access(fn, F_OK) == 0) return fn;
  }

  strcpy(fn, short_fn);
  return fn;
}

static int
read_results(sp, ep, csc, nlines, csq)
  SCR *sp;
  EXF *ep;
  CSC *csc;
  int nlines;
  CSQ *csq;
{
  CSR *csr;
  int i, rc, rlen;
  char rbuf[BUFSIZ], *p;
  char short_fn[PATH_MAX];

  for(i = 0; i < nlines; i++)
  {
do_read:
    if(!fgets(rbuf, sizeof rbuf, csc->from_fp))
    {
      if(errno == EINTR && !F_ISSET(sp, S_INTERRUPTED))
      {
        /* Interrupted by busy msg.  Try again. */
        goto do_read;
      }
      msgq(sp, M_SYSERR, csc->name);
      return 1;
    }
    p = strchr(rbuf, '\n');
    if(p) *p = 0;
    rlen = strlen(rbuf) + 1;

    MALLOC_RET(sp, csr, CSR *, sizeof(CSR));
    csr->db_time = csc->db_time;
    MALLOC_RET(sp, csr->raw, char *, rlen);
    strcpy(csr->raw, rbuf);
    csr->number = csq->n_results + 1;
    csr->fn = 0;
    MALLOC_RET(sp, csr->context, char *, rlen);
    csr->regexp = 0;

    rc = sscanf(csr->raw, "%s %s %d %[^\n]", short_fn, csr->context,
      &csr->lno, rbuf);
    if(rc != 4)
    {
      msgq(sp, M_ERR, "%s: unexpected output `%s'", csc->name,
        csr->raw);
      return 1;
    }

    csr->fn = find_file(sp, csc, short_fn);
    if(!csr->fn) return 1;

    csr->regexp = make_regexp(sp, rbuf);
    if(!csr->regexp) return 1;

    CIRCLEQ_INSERT_TAIL(&csq->csrq, csr, q);
    csq->n_results++;
  }

  return 0;
}

int
cscope_find(sp, ep, cmdp, pattern)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *pattern;
{
  EX_PRIVATE *exp;
  CSC *csc, *csc_next;
  CSQ *csq;
  int i, clen, nlines;
  char nlines_buf[BUFSIZ];
  int btear, itear;

  exp = EXP(sp);

  if(!sp->gp->cscq.lh_first)
  {
    msgq(sp, M_INFO, "No C-Scopes to query");
    return 0;
  }

  btear = F_ISSET(sp, S_EXSILENT)? 0:  !busy_on(sp, "Querying C-Scopes...");
#if 0
  itear = intr_init(sp);
#endif

  /* We send the same command to all C-Scopes. */
  if(create_cs_cmd(sp, ep, pattern))
  {
badreturn:
    if(btear) busy_off(sp);
#if 0
    if(itear) intr_end(sp);
#endif
    return 1;
  }
  clen = strlen(exp->csqlast);

  /* Set up an entry on the query stack. */
  if(cscope_push(sp, ep, cmdp)) goto badreturn;
  csq = exp->csqq.lh_first;
  /* We take advantage of the fact that csqlast will be digit+realpattern.  */
  MALLOC(sp, csq->query, char *, strlen(exp->csqlast) + 2);
  if(!csq->query) goto badreturn;
  csq->query[0] = pattern[0];
  csq->query[1] = ' ';
  strcpy(csq->query + 2, exp->csqlast + 1);
  /* Get rid of the trailing newline. */
  csq->qlen = strlen(csq->query) - 1;
  csq->query[csq->qlen] = 0;

  for(i = 1, csc = sp->gp->cscq.lh_first; csc; csc = csc_next, i++)
  {
    /* Grab csc->q.lh_next here in case csc gets killed. */
    csc_next = csc->q.le_next;

    if(fputs(exp->csqlast, csc->to_fp) != clen)
    {
      msgq(sp, M_SYSERR, csc->name);
      kill_by_number(sp, ep, i);
      continue;
    }
    if(fflush(csc->to_fp))
    {
      msgq(sp, M_SYSERR, csc->name);
      kill_by_number(sp, ep, i);
      continue;
    }

do_read:
    if(!fgets(nlines_buf, sizeof nlines_buf, csc->from_fp))
    {
      if(errno == EINTR && !F_ISSET(sp, S_INTERRUPTED))
      {
        /* Interrupted by busy msg.  Try again. */
        goto do_read;
      }

      msgq(sp, M_SYSERR, csc->name);
      kill_by_number(sp, ep, i);
      continue;
    }

    nlines = parse_nlines(nlines_buf);
    if(nlines < 0)
    {
      msgq(sp, M_ERR, "%s: unexpected output `%s'", csc->name, nlines_buf);
      kill_by_number(sp, ep, i);
      continue;
    }

    if(read_results(sp, ep, csc, nlines, csq))
    {
      kill_by_number(sp, ep, i);
      continue;
    }

    if(read_prompt(sp, csc))
    {
      kill_by_number(sp, ep, i);
      continue;
    }
  }

  if(btear) busy_off(sp);
#if 0
  if(itear) intr_end(sp);
#endif

  if(!csq->n_results)
  {
    msgq(sp, M_ERR, "C-Scope query failed for `%s'", csq->query);
    cscope_pop(sp, ep, cmdp, "1");
    return 1;
  }

  msgq(sp, M_INFO, "C-Scope query returned %d match%s", csq->n_results,
    (csq->n_results>1)?"es":"");
  return cscope_next(sp, ep, cmdp, "1");
}

int
cscope_help(sp, ep, cmdp, subcmd)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *subcmd;
{
  cscope_cmd *cc;
  int i;

  if(subcmd && *subcmd)
  {
    cc = lookup_ccmd(subcmd);
    if(cc)
    {
      ex_printf(EXCOOKIE, "Subcommand: %s (%s)\n", cc->name, cc->help_msg);
	  ex_printf(EXCOOKIE, "     Usage: %s\n", cc->usage_msg);
      return 0;
    }
    else
    {
      ex_printf(EXCOOKIE, "The %s subcommand is unknown.\n", subcmd);
      return 1;
    }
  }

  ex_printf(EXCOOKIE, "cscope subcommands:\n");
  for(i = 0; i < NCMDS; i++)
  {
    ex_printf(EXCOOKIE, "  %s: %s\n", cscope_cmds[i].name,
      cscope_cmds[i].help_msg);
  }

  return 0;
}

static int
file_time(sp, fn)
  SCR *sp;
  char *fn;
{
  struct stat sb;

  if(stat(fn, &sb))
  {
    msgq(sp, M_SYSERR, fn);
    return 0;
  }

  return sb.st_mtime;
}

int
cscope_next(sp, ep, cmdp, count)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *count;
{
  EX_PRIVATE *exp;
  FREF *frp;
  CSQ *csq;
  CSR *prev, *csr;
  char *sign;
  int n, sflags, rc;
  MARK m;
  char *fn;
  int f_time;
  int show_info;

  exp = EXP(sp);
  csq = exp->csqq.lh_first;
  if(!csq)
  {
    msgq(sp, M_ERR, "No C-Scope query on the stack");
    return 1;
  }
  if(!csq->n_results)
  {
    msgq(sp, M_ERR, "No C-Scope query performed since last push");
    return 1;
  }

  prev = csq->current;

  if(!*count) count = "+1";

  if(*count == '+' || *count == '-') sign = count++;
  else sign = 0;

  if(*count) n = atoi(count);
  else n = 1;

  if(sign)
  {
    show_info = 1;
    csr = prev;

    if(*sign == '+')
    {
      while(n--)
      {
        csr = csr->q.cqe_next;
        if(csr == (void*)&csq->csrq)
          csr = csq->csrq.cqh_first;
      }
    }
    else
    {
      while(n--)
      {
        csr = csr->q.cqe_prev;
        if(csr == (void*)&csq->csrq)
          csr = csq->csrq.cqh_last;
      }
    }
  }
  else
  {
    show_info = 0;

    if(n < 1 || n > csq->n_results)
    {
      msgq(sp, M_ERR, "There is no C-Scope result #%s", count);
      return 1;
    }

    for(csr = csq->csrq.cqh_first; csr != (void*)&csq->csrq;
	csr = csr->q.cqe_next)
      if(csr->number == n) break;

    if(csr == (void*)&csq->csrq)
    {
      msgq(sp, M_ERR, "Could not find result #%s", count);
      return 1;
    }
  }

  csq->current = csr;

  frp = file_add(sp, csr->fn);
  if(!frp) return 1;

  if(sp->frp != frp)
  {
    if(file_m1(sp, ep, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
      return 1;

    if(file_init(sp, frp, NULL, 0)) return 1;
    F_SET(sp, S_FSWITCH);
  }

  f_time = file_time(sp, csr->fn);
  if(f_time == 0) return 1;

  if(f_time < csr->db_time)
  {
    /* The file is older than the database.  Use the line number. */
    sp->lno = frp->lno = csr->lno;
    sp->cno = frp->cno = 0;
    F_SET(frp, FR_CURSORSET);
  }
  else
  {
    /* The file is younger than the database.  Use the regexp. */
    sflags = SEARCH_FILE;
    m.lno = 1; m.cno = 0;
    rc = f_search(sp, sp->ep, &m, &m, csr->regexp, NULL, &sflags);
    if(rc)
    {
      msgq(sp, M_ERR, "Tag out of date");
      return 1;
    }
    sp->lno = frp->lno = m.lno;
    sp->cno = frp->cno = m.cno;
    F_SET(frp, FR_CURSORSET);
  }

  if(show_info) msgq(sp, M_VINFO, "C-Scope hit %d of %d",
    csr->number, csq->n_results);
  return 0;
}

int
cscope_pop(sp, ep, cmdp, count)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *count;
{
  EX_PRIVATE *exp;
  CSQ *csq;
  CSR *csr;

  exp = EXP(sp);
  if(!(csq = exp->csqq.lh_first))
  {
    msgq(sp, M_INFO, "C-Scope query stack empty");
    return 1;
  }

  if(csq->frp == sp->frp)
  {
    sp->lno = csq->lno;
    sp->cno = csq->cno;
  }
  else
  {
    if(file_m1(sp, ep, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
      return 1;
    if(file_init(sp, csq->frp, NULL, 0))
      return 1;
    csq->frp->lno = csq->lno;
    csq->frp->cno = csq->cno;
    F_SET(sp->frp, FR_CURSORSET);
    F_SET(sp, S_FSWITCH);
  }

  /* Now free up the stack entry. */

  while((void*)(csr = csq->csrq.cqh_first) != (void*)&csq->csrq)
  {
    if(csr->raw) free(csr->raw);
    if(csr->fn) free(csr->fn);
    if(csr->context) free(csr->context);
    if(csr->regexp) free(csr->regexp);
    CIRCLEQ_REMOVE(&csq->csrq, csr, q);
    free(csr);
  }

  if(csq->query) free(csq->query);
  LIST_REMOVE(csq, q);

  return 0;
}

int
cscope_push(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  EX_PRIVATE *exp;
  CSQ *csq;

  exp = EXP(sp);

  MALLOC_RET(sp, csq, CSQ *, sizeof(CSQ));

  csq->query = 0;
  csq->qlen = 0;
  csq->frp = sp->frp;
  csq->lno = sp->lno;
  csq->cno = sp->cno;
  CIRCLEQ_INIT(&csq->csrq);
  csq->n_results = 0;
  csq->current = 0;

  LIST_INSERT_HEAD(&exp->csqq, csq, q);

  return 0;
}

int
cscope_reset(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  while(sp->gp->cscq.lh_first)
    if(cscope_kill(sp, ep, cmdp, "1")) return 1;

  return 0;
}

static int
show_connections(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  EX_PRIVATE *exp;
  GS *gp;
  CSC *csc;
  int i;

  exp = EXP(sp);
  gp = sp->gp;

  if(!gp->cscq.lh_first)
  {
    ex_printf(EXCOOKIE, "No C-Scope connections.\n");
    return 0;
  }

  for(i = 1, csc = gp->cscq.lh_first; csc; csc = csc->q.le_next, i++)
    ex_printf(EXCOOKIE, "%3d %s (%d)\n", i, csc->name, csc->pid);

  return 0;
}

static int
show_stack(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  EX_PRIVATE *exp;
  CSQ *csq;
  int i;

  exp = EXP(sp);

  if(!exp->csqq.lh_first)
  {
    ex_printf(EXCOOKIE, "C-Scope query stack empty.\n");
    return 0;
  }

  ex_printf(EXCOOKIE, "%3d %s:%d\n", 0, sp->frp->name, sp->lno);
  for(i = 1, csq = exp->csqq.lh_first; csq; csq = csq->q.le_next, i++)
  {
    if(csq->query)
    {
      ex_printf(EXCOOKIE,
	"    `find %s' returned %d match%s.\n", csq->query,
        csq->n_results, (csq->n_results>1)?"es":"");
    }
    ex_printf(EXCOOKIE, "%3d %s:%d\n", i, csq->frp->name, csq->lno);
  }

  return 0;
}

static int
show_results(sp, ep, cmdp)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
{
  EX_PRIVATE *exp;
  CSQ *csq;
  CSR *csr;

  exp = EXP(sp);
  csq = exp->csqq.lh_first;

  if(!csq)
  {
    ex_printf(EXCOOKIE, "C-Scope query stack empty.\n");
    return 0;
  }

  if(!csq->query)
  {
    ex_printf(EXCOOKIE, "No C-Scope query performed since last push.\n");
    return 0;
  }

  if(!csq->n_results)
  {
    ex_printf(EXCOOKIE, "Last C-Scope query returned no matches.\n");
    return 0;
  }

  F_SET(sp, S_INTERRUPTIBLE);
  for(csr = csq->csrq.cqh_first; (void*)csr != (void*)&csq->csrq;
    csr = csr->q.cqe_next)
  {
    ex_printf(EXCOOKIE, "%3d%c%s\n", csr->number,
      (csq->current == csr)?'*':' ', csr->raw);
    if(F_ISSET(cmdp, E_FORCE))
      ex_printf(EXCOOKIE, "   %s\n", csr->regexp);
  }

  return 0;
}

int
cscope_show(sp, ep, cmdp, list)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *list;
{
  switch(list[0])
  {
    case 'c':		/* Show connections. */
      return show_connections(sp, ep, cmdp);

    case 's':		/* Show query stack. */
      return show_stack(sp, ep, cmdp);

    case 'r':		/* Show query results. */
      return show_results(sp, ep, cmdp);

    case 0:		/* No arg specified! */
      msgq(sp, M_ERR, "Specify c[onnections], s[tack], or r[esults]");
      break;

    default:
      msgq(sp, M_ERR, "Don't know how to display %s", list);
      return 1;
  }
}

static int
kill_by_number(sp, ep, n)
  SCR *sp;
  EXF *ep;
  int n;
{
  EX_PRIVATE *exp;
  int i;
  CSC *csc;

  if(n < 1)
  {
bad:
    msgq(sp, M_ERR, "Connection number `%d' is invalid", n);
    return 1;
  }

  for(i = 1, csc = sp->gp->cscq.lh_first; csc; csc = csc->q.le_next, i++)
    if(i == n)
      break;

  if(!csc) goto bad;

  kill(csc->pid, SIGTERM);
  /* waitpid(csc->pid, 0, 0); ? what if c-scope is really wedged? */
  fclose(csc->to_fp);
  fclose(csc->from_fp);
  LIST_REMOVE(csc, q);

  msgq(sp, M_INFO, "C-Scope %s detached", csc->name);
  free(csc->name);
  if(csc->path)
  {
    for(i = 0; i < csc->n_paths; i++)
      free(csc->path[i]);
    free(csc->path);
  }
  free(csc);

  return 0;
}

int
cscope_kill(sp, ep, cmdp, cn)
  SCR *sp;
  EXF *ep;
  EXCMDARG *cmdp;
  char *cn;
{
  return kill_by_number(sp, ep, atoi(cn));
}

/**********************************************************************/

/* Various global functions. */

int
ex_csqcopy(orig, sp)
  SCR *orig, *sp;
{
  EX_PRIVATE *oexp, *nexp;
  CSQ *ocsq, *ncsq, *tcsq;
  CSR *ocsr, *ncsr;

  oexp = EXP(orig);
  nexp = EXP(sp);

  if(oexp->csqlast)
  {
    nexp->csqlast = strdup(oexp->csqlast);
    if(!nexp->csqlast)
    {
      msgq(sp, M_SYSERR, NULL);
      return 1;
    }
  }

  for(ocsq = oexp->csqq.lh_first, tcsq = 0; ocsq; ocsq = ocsq->q.le_next)
  {
    MALLOC_RET(sp, ncsq, CSQ *, sizeof(CSQ));

    *ncsq = *ocsq;
    if(ocsq->query && !(ncsq->query = strdup(ocsq->query)))
    {
nomem:
      msgq(sp, M_SYSERR, NULL);
      return 1;
    }

    CIRCLEQ_INIT(&ncsq->csrq);
    for(ocsr = ocsq->csrq.cqh_first; ocsr && (void*)ocsr != (void*)&ocsq->csrq;
      ocsr = ocsr->q.cqe_next)
    {
      MALLOC_RET(sp, ncsr, CSR *, sizeof(CSR));
      *ncsr = *ocsr;
      if(ocsr->raw) ncsr->raw = strdup(ocsr->raw);
      if(!ncsr->raw) goto nomem;
      if(ocsr->fn) ncsr->fn = strdup(ocsr->fn);
      if(!ncsr->fn) goto nomem;
      if(ocsr->context) ncsr->fn = strdup(ocsr->context);
      if(!ncsr->context) goto nomem;
      if(ocsr->regexp) ncsr->fn = strdup(ocsr->regexp);
      if(!ncsr->regexp) goto nomem;
      CIRCLEQ_INSERT_TAIL(&ncsq->csrq, ncsr, q);
    }

    if(tcsq) { LIST_INSERT_AFTER(tcsq, ncsq, q); }
    else { LIST_INSERT_HEAD(&nexp->csqq, ncsq, q); }
    tcsq = ncsq;
  }

  return 0;
}

int
ex_csqfirst(sp, csqarg)
  SCR *sp;
  char *csqarg;
{
  /* XXX */
  abort();
  return 0;
}

int
ex_csqfree(sp)
  SCR *sp;
{
  EX_PRIVATE *exp;
  CSQ *tp;

  exp = EXP(sp);
  while(tp = exp->csqq.lh_first)
  {
    LIST_REMOVE(tp, q);
    if(tp->query) free(tp->query);
    free(tp);
  }

  return 0;
}
