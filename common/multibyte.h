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

#define MEMCMP(to, from, n) 						    \
	memcmp(to, from, (n) * sizeof(*(to)))
#define	MEMMOVE(p, t, len)	memmove(p, t, (len) * sizeof(*(p)))
#define	MEMCPY(p, t, len)	memcpy(p, t, (len) * sizeof(*(p)))
#define STRSET(s,c,n)							    \
	sizeof(char) == sizeof(CHAR_T) ? memset(s,c,n) : v_strset(s,c,n)

#endif
