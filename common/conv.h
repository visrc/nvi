#define F_GB 'A'

#define INT9494(f,r,c)	((f) << 16) | ((r) << 8) | (c)
#define INTIS9494(c)	!!(((c) >> 16) & 0x7F)
#define INTISUCS(c)	((c & ~0x7F) && !(((c) >> 16) & 0x7F))
#define INTUCS(c)	(c)
#define INT9494F(c)	((c) >> 16) & 0x7F
#define INT9494R(c)	((c) >> 8) & 0x7F
#define INT9494C(c)	(c) & 0x7F
#define INTILL(c)	(1 << 23) | (c)

#define KEY_COL(sp, ch)							\
	(INTISWIDE(ch) ? CHAR_WIDTH(sp, ch) : KEY_LEN(sp,ch))

struct _conv_win {
    void    *bp1;
    size_t   blen1;
    void    *bp2;
    size_t   blen2;
};

typedef int (*char2wchar_t) 
    (SCR *, const char *, ssize_t, struct _conv_win *, size_t *, CHAR_T **);
typedef int (*wchar2char_t) 
    (SCR *, const CHAR_T *, ssize_t, struct _conv_win *, size_t *, char **);

struct _conv {
	char2wchar_t	char2int;
	wchar2char_t	int2char;
	char2wchar_t	file2int;
	wchar2char_t	int2file;
	wchar2char_t	int2disp;
};
