/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: script.h,v 9.2 1994/12/02 10:00:29 bostic Exp $ (Berkeley) $Date: 1994/12/02 10:00:29 $
 */

struct _script {
	pid_t	 sh_pid;		/* Shell pid. */
	int	 sh_master;		/* Master pty fd. */
	int	 sh_slave;		/* Slave pty fd. */
	char	*sh_prompt;		/* Prompt. */
	size_t	 sh_prompt_len;		/* Prompt length. */
	char	 sh_name[64];		/* Pty name */
#ifdef TIOCGWINSZ
	struct winsize sh_win;		/* Window size. */
#endif
	struct termios sh_term;		/* Terminal information. */
};
