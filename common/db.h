#include <db.h>

#ifdef USE_DYNAMIC_LOADING
#define db_create   	nvi_db_create
#define db_env_create   nvi_db_env_create
#define db_strerror 	nvi_db_strerror

extern int   (*nvi_db_create) __P((DB **, DB_ENV *, u_int32_t));
extern int   (*nvi_db_env_create) __P((DB_ENV **, u_int32_t));
extern char *(*nvi_db_strerror) __P((int));
#endif
