#define F_GB 'A'

#define INT9494(f,r,c)	((f) << 16) | ((r) << 8) | (c)
#define INTIS9494(c)	!!(((c) >> 16) & 0x7F)
#define INTISUCS(c)	((c & ~0x7F) && !(((c) >> 16) & 0x7F))
#define INTUCS(c)	(c)
#define INT9494F(c)	((c) >> 16) & 0x7F
#define INT9494R(c)	((c) >> 8) & 0x7F
#define INT9494C(c)	(c) & 0x7F
#define INTILL(c)	(1 << 23) | (c)
#ifdef USE_WIDECHAR
#define INTISWIDE(c)	(!!(c >> 8))
#define CHAR_WIDTH(sp, ch)						\
	(INTISUCS(ch) && ucswidth(ch) > 0 ? ucswidth(ch) : INTISWIDE(ch) ? 2 : 1)
#else
#define INTISWIDE(c)	    0
#define CHAR_WIDTH(sp, ch)  1
#endif

#define KEY_COL(sp, ch)							\
	(INTISWIDE(ch) ? CHAR_WIDTH(sp, ch) : KEY_LEN(sp,ch))

struct _conv_win {
    void    *bp1;
    size_t   blen1;
    void    *bp2;
    size_t   blen2;
};

struct _conv {
	int	(*char2int) (SCR *, const char *, ssize_t, struct _conv_win *, size_t *);
	int	(*int2char) (SCR *, const CHAR_T *, ssize_t, struct _conv_win *, size_t *);
	int	(*file2int) (SCR *, const char *, ssize_t, struct _conv_win *, size_t *);
	int	(*int2file) (SCR *, const CHAR_T *, ssize_t, struct _conv_win *, size_t *);
	int	(*int2disp) (SCR *, const CHAR_T *, ssize_t, struct _conv_win *, size_t *);
};
