#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "common.h"

#include "db_int.h"
#include "db_page.h"
#include "log.h"
#include "hash.h"

/*
 * __vi_marker_recover --
 *	Recovery function for marker.
 *
 * PUBLIC: int __vi_marker_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_marker_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_marker_args *argp;
	int ret;

	REC_PRINT(__vi_marker_print);
	REC_NOOP_INTRO(__vi_marker_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

    	REC_NOOP_CLOSE;
}

/*
 * __vi_cursor_recover --
 *	Recovery function for cursor.
 *
 * PUBLIC: int __vi_cursor_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_cursor_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_cursor_args *argp;
	int ret;

	REC_PRINT(__vi_cursor_print);
	REC_NOOP_INTRO(__vi_cursor_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

    	REC_NOOP_CLOSE;
}

/*
 * __vi_mark_recover --
 *	Recovery function for mark.
 *
 * PUBLIC: int __vi_mark_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_mark_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_mark_args *argp;
	int ret;

	REC_PRINT(__vi_mark_print);
	REC_NOOP_INTRO(__vi_mark_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

    	REC_NOOP_CLOSE;
}

/*
 * __vi_change_recover --
 *	Recovery function for change.
 *
 * PUBLIC: int __vi_change_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_change_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_change_args *argp;
	int ret;

	REC_PRINT(__vi_change_print);
	REC_NOOP_INTRO(__vi_change_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

    	REC_NOOP_CLOSE;
}

