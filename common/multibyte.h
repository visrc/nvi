#ifndef MULTIBYTE_H
#define MULTIBYTE_H
#ifdef USE_WIDECHAR
typedef	u_int		CHAR_T;
#else
typedef	u_char		CHAR_T;
#endif
#define MEMCMPW(to, from, n) \
    memcmp(to, from, (n) * sizeof(CHAR_T))
#endif
