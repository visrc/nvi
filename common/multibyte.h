#ifndef MULTIBYTE_H
#define MULTIBYTE_H
#ifdef USE_WIDECHAR
typedef	int		CHAR_T;
#define CHAR_T_MAX	((1 << 24)-1)
typedef	u_int		UCHAR_T;
#else
typedef	char		CHAR_T;
#define CHAR_T_MAX	CHAR_MAX
typedef	u_char		UCHAR_T;
#endif

#define MEMCMPW(to, from, n) \
    memcmp(to, from, (n) * sizeof(CHAR_T))
#define	MEMMOVE(p, t, len)	memmove(p, t, (len) * sizeof(*(p)))

#endif
