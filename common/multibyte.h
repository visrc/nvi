#ifndef MULTIBYTE_H
#define MULTIBYTE_H
#ifdef USE_WIDECHAR
typedef	int		CHAR_T;
#define CHAR_T_MAX	((1 << 24)-1)
#else
typedef	char		CHAR_T;
#define CHAR_T_MAX	CHAR_MAX
#endif
#define MEMCMPW(to, from, n) \
    memcmp(to, from, (n) * sizeof(CHAR_T))
#endif
