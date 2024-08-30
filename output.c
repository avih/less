/*
 * Copyright (C) 1984-2024  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * High level routines dealing with the output to the screen.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include "windows.h"
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif
#endif

public int errmsgs;    /* Count of messages displayed by error() */
public int need_clr;
public int final_attr;
public int at_prompt;

extern int sigs;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int is_tty;
extern int oldbot;
extern int utf_mode;
extern char intr_char;

#if MSDOS_COMPILER==WIN32C || MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
extern int ctldisp;
extern int nm_fg_color, nm_bg_color;
extern int bo_fg_color, bo_bg_color;
extern int ul_fg_color, ul_bg_color;
extern int so_fg_color, so_bg_color;
extern int bl_fg_color, bl_bg_color;
extern int sgr_mode;
#if MSDOS_COMPILER==WIN32C
extern int vt_enabled;
#endif
#endif

/*
 * Display the line which is in the line buffer.
 */
public void put_line(void)
{
	int c;
	size_t i;
	int a;

	if (ABORT_SIGS())
	{
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed();
		return;
	}

	final_attr = AT_NORMAL;

	for (i = 0;  (c = gline(i, &a)) != '\0';  i++)
	{
		at_switch(a);
		final_attr = a;
		if (c == '\b')
			putbs();
		else
			putchr(c);
	}

	at_exit();
}

/*
 * win_flush has at least one non-critical issue when an escape sequence
 * begins at the last char of the buffer, and possibly more issues.
 * as a temporary measure to reduce likelyhood of encountering end-of-buffer
 * issues till the SGR parser is replaced, OUTBUF_SIZE is 8K on Windows.
 */
static char obuf[OUTBUF_SIZE];
static char *ob = obuf;
static int outfd = 2; /* stderr */

#if MSDOS_COMPILER==WIN32C || MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC


#if 0  /* original colors parser */

typedef unsigned t_attr;

#define A_BOLD      (1u<<0)
#define A_ITALIC    (1u<<1)
#define A_UNDERLINE (1u<<2)
#define A_BLINK     (1u<<3)
#define A_INVERSE   (1u<<4)
#define A_CONCEAL   (1u<<5)

/* long is guaranteed 32 bits, and we reserve bits for type + RGB */
typedef unsigned long t_color;

#define T_DEFAULT   0ul
#define T_ANSI      1ul  /* colors 0-7 */

#define CGET_ANSI(c) ((c) & 0x7)

#define C_DEFAULT    (T_DEFAULT <<24) /* 0 */
#define C_ANSI(c)   ((T_ANSI    <<24) | (c))

/* attr/fg/bg/all 0 is the default attr/fg/bg/all, respectively */
typedef struct t_sgr {
	t_attr attr;
	t_color fg;
	t_color bg;
} t_sgr;

static constant t_sgr SGR_DEFAULT; /* = {0} */

/* returns 0 on success, non-0 on unknown SGR code */
static int update_sgr(t_sgr *sgr, long code)
{
	switch (code)
	{
	case  0: *sgr = SGR_DEFAULT; break;

	case  1: sgr->attr |=  A_BOLD; break;
	case 22: sgr->attr &= ~A_BOLD; break;

	case  3: sgr->attr |=  A_ITALIC; break;
	case 23: sgr->attr &= ~A_ITALIC; break;

	case  4: sgr->attr |=  A_UNDERLINE; break;
	case 24: sgr->attr &= ~A_UNDERLINE; break;

	case  6: /* fast-blink, fallthrough */
	case  5: sgr->attr |=  A_BLINK; break;
	case 25: sgr->attr &= ~A_BLINK; break;

	case  7: sgr->attr |=  A_INVERSE; break;
	case 27: sgr->attr &= ~A_INVERSE; break;

	case  8: sgr->attr |=  A_CONCEAL; break;
	case 28: sgr->attr &= ~A_CONCEAL; break;

	case 39: sgr->fg = C_DEFAULT; break;
	case 49: sgr->bg = C_DEFAULT; break;

	case 30: case 31: case 32: case 33:
	case 34: case 35: case 36: case 37:
		sgr->fg = C_ANSI(code - 30);
		break;

	case 40: case 41: case 42: case 43:
	case 44: case 45: case 46: case 47:
		sgr->bg = C_ANSI(code - 40);
		break;
	default:
		return 1;
	}

	return 0;
}

static void set_win_colors(t_sgr *sgr)
{
#if MSDOS_COMPILER==WIN32C
	/* Screen colors used by 3x and 4x SGR commands. */
	static unsigned char screen_color[] = {
		0, /* BLACK */
		FOREGROUND_RED,
		FOREGROUND_GREEN,
		FOREGROUND_RED|FOREGROUND_GREEN,
		FOREGROUND_BLUE,
		FOREGROUND_BLUE|FOREGROUND_RED,
		FOREGROUND_BLUE|FOREGROUND_GREEN,
		FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED
	};
#else
	static enum COLORS screen_color[] = {
		BLACK, RED, GREEN, BROWN,
		BLUE, MAGENTA, CYAN, LIGHTGRAY
	};
#endif

	int fg, bg, tmp;  /* Windows colors */

	/* Not "SGR mode": apply -D<x> to default fg+bg with one attribute */
	if (!sgr_mode && sgr->fg == C_DEFAULT && sgr->bg == C_DEFAULT)
	{
		switch (sgr->attr)
		{
		case A_BOLD:
			WIN32setcolors(bo_fg_color, bo_bg_color);
			return;
		case A_UNDERLINE:
			WIN32setcolors(ul_fg_color, ul_bg_color);
			return;
		case A_BLINK:
			WIN32setcolors(bl_fg_color, bl_bg_color);
			return;
		case A_INVERSE:
			WIN32setcolors(so_fg_color, so_bg_color);
			return;
		/*
		 * There's no -Di so italic should not be here, but to
		 * preserve legacy behavior, apply -Ds to italic too.
		 */
		case A_ITALIC:
			WIN32setcolors(so_fg_color, so_bg_color);
			return;
		}
	}

	/* generic application of the SGR state as Windows colors */

	fg = sgr->fg == C_DEFAULT ? nm_fg_color
	                          : screen_color[CGET_ANSI(sgr->fg)];

	bg = sgr->bg == C_DEFAULT ? nm_bg_color
	                          : screen_color[CGET_ANSI(sgr->bg)];

	if (sgr->attr & A_BOLD)
		fg |= 8;

	if (sgr->attr & (A_BLINK | A_UNDERLINE))
		bg |= 8;  /* TODO: can be illegible */

	if (sgr->attr & (A_INVERSE | A_ITALIC))
	{
		tmp = fg;
		fg = bg;
		bg = tmp;
	}

	if (sgr->attr & A_CONCEAL)
		fg = bg ^ 8;

	WIN32setcolors(fg, bg);
}

/* like is_ansi_end, but doesn't assume c != 0  (returns 0 for c == 0) */
static int is_ansi_end_0(char c)
{
	return c && is_ansi_end((unsigned char)c);
}

static void win_flush(void)
{
	if (ctldisp != OPT_ONPLUS
#if MSDOS_COMPILER==WIN32C
	    || (vt_enabled && sgr_mode)
#endif
	   )
		WIN32textout(obuf, ptr_diff(ob, obuf));
	else
	{
		/*
		 * Digest text, apply embedded SGR sequences as Windows-colors.
		 * By default - when -Da ("SGR mode") is unset - also apply
		 * translation of -D command-line options (at set_win_colors)
		 */
		char *anchor, *p, *p_next;
		static t_sgr sgr;

		/* when unsupported SGR value is encountered, like 38/48 for
		 * 256/true colors, then we abort processing this sequence,
		 * because it may expect followup values, but we don't know
		 * how many, so we've lost sync of this sequence parsing.
		 * Without VT enabled it's OK because we can't do much anyway,
		 * but with VT enabled we choose to passthrough this sequence
		 * to the terminal - which can handle it better than us.
		 * however, this means that our "sgr" var is no longer in sync
		 * with the actual terminal state, which can lead to broken
		 * colors with future sequences which we _can_ fully parse.
		 * in such case, once it happens, we keep passthrough sequences
		 * until we know we're in sync again - on a valid reset.
		 */
		static int sgr_bad_sync;

		for (anchor = p_next = obuf;
			 (p_next = memchr(p_next, ESC, ob - p_next)) != NULL; )
		{
			p = p_next;
			if (p[1] == '[')  /* "ESC-[" sequence */
			{
				/*
				* unknown SGR code ignores the rest of the seq,
				* and allows ignoring sequences such as
				* ^[[38;5;123m or ^[[38;2;5;6;7m
				* (prior known codes at the same seq do apply)
				*/
				int bad_code = 0;

				if (p > anchor)
				{
					/*
					 * If some chars seen since
					 * the last escape sequence,
					 * write them out to the screen.
					 */
					WIN32textout(anchor, ptr_diff(p, anchor));
					anchor = p;
				}
				p += 2;  /* Skip the "ESC-[" */
				if (is_ansi_end_0(*p))
				{
					/*
					 * Handle null escape sequence
					 * "ESC[m" as if it was "ESC[0m"
					 */
					p++;
					anchor = p_next = p;
					update_sgr(&sgr, 0);
					set_win_colors(&sgr);
					sgr_bad_sync = 0;
					continue;
				}
				p_next = p;

				/*
				 * Parse and apply SGR values to the SGR state
				 * based on the escape sequence. 
				 */
				while (!is_ansi_end_0(*p))
				{
					char *q;
					long code = strtol(p, &q, 10);

					if (*q == '\0')
					{
						/*
						 * Incomplete sequence.
						 * Leave it unprocessed
						 * in the buffer.
						 */
						size_t slop = ptr_diff(q, anchor);
						memmove(obuf, anchor, slop);
						ob = &obuf[slop];
						return;
					}

					if (q == p ||
						(!is_ansi_end_0(*q) && *q != ';'))
					{
						/*
						 * can't parse. passthrough
						 * till the end of the buffer
						 */
						p_next = q;
						break;
					}
					if (*q == ';')
						q++;

					if (!bad_code)
						bad_code = update_sgr(&sgr, code);

					if (bad_code)
						sgr_bad_sync = 1;
					else if (code == 0)
						sgr_bad_sync = 0;

					p = q;
				}
				if (!is_ansi_end_0(*p) || p == p_next)
					break;

				if (sgr_bad_sync && vt_enabled) {
					/* this or a prior sequence had unknown
					 * SGR value. passthrough all sequences
					 * until we're in-sync again
					 */
					WIN32textout(anchor, ptr_diff(p+1, anchor));
				} else {
					set_win_colors(&sgr);
				}
				p_next = anchor = p + 1;
			} else
				p_next++;
		}

		/* Output what's left in the buffer.  */
		WIN32textout(anchor, ptr_diff(ob, anchor));
	}
	ob = obuf;
}

#else /* avih colors parser - includes 256/true colors to 8, alt winflush */

/* color uses 26 bits (type:2, up to rgb:24). we use long - guaranteed 32+ */
enum color_type {
	T_DEFAULT = 0,
	T_ANSI,  /* 0-7 */
	T_256,   /* 0-255, 8-15 also mapped from aixterm bright-only SGR 9x/10x */
	T_RGB
};

#define CGET_TYPE(c) ((c) >>24)
#define CGET_ANSI(c) ((c) & 0x7)
#define CGET_256(c)  ((c) & 0xff)
#define CGET_R(c)    (((c) >>16) & 0xff)
#define CGET_G(c)    (((c) >>8) & 0xff)
#define CGET_B(c)    ((c) & 0xff)

#define C_DEFAULT       ((long)T_DEFAULT <<24) /* 0 */
#define C_ANSI(c)       (((long)T_ANSI <<24) | (c))
#define C_256(c)        (((long)T_256 <<24) | (c))
#define C_RGB(r, g, b)  (((long)T_RGB  <<24) | ((r) <<16) | ((g) <<8) | (b))


/* attributes we care about in some way */
#define A_BOLD        (1u<<0)
#define A_ITALIC      (1u<<1)
#define A_UNDERLINE   (1u<<2)
#define A_BLINK       (1u<<3)
#define A_INVERSE     (1u<<4)
#define A_CONCEAL     (1u<<5)


/* attr/fg/bg/all 0 is the default attr/fg/bg/all, respectively */
typedef struct sgr_state {
	unsigned attr;
	long fg;
	long bg;
} sgr_state;


#define CASE_RANGE_8(base) \
	case base+0: case base+1: case base+2: case base+3: \
	case base+4: case base+5: case base+6: case base+7 /* : */

static void update_sgr(sgr_state *sgr, const unsigned char* vals, int len)
{
	for (; len; ++vals, --len) {
		unsigned v = vals[0];
		switch (v) {
		case  0: *sgr = (sgr_state){0}; break;

		case  1: sgr->attr |=  A_BOLD; break;
		case 22: sgr->attr &= ~A_BOLD; break;

		case  3: sgr->attr |=  A_ITALIC; break;
		case 23: sgr->attr &= ~A_ITALIC; break;

		case  4: sgr->attr |=  A_UNDERLINE; break;
		case 24: sgr->attr &= ~A_UNDERLINE; break;

		case  5: /* fallthrough */
		case  6: sgr->attr |=  A_BLINK; break;
		case 25: sgr->attr &= ~A_BLINK; break;

		case  7: sgr->attr |=  A_INVERSE; break;
		case 27: sgr->attr &= ~A_INVERSE; break;

		case  8: sgr->attr |=  A_CONCEAL; break;
		case 28: sgr->attr &= ~A_CONCEAL; break;

		case 39: sgr->fg = C_DEFAULT; break;
		case 49: sgr->bg = C_DEFAULT; break;

		CASE_RANGE_8(30): sgr->fg = C_ANSI(v - 30); break;
		CASE_RANGE_8(40): sgr->bg = C_ANSI(v - 40); break;

		CASE_RANGE_8(90): sgr->fg = C_256(8 + v - 90); break;
		CASE_RANGE_8(100):sgr->bg = C_256(8 + v - 100); break;

		case 38: case 48: case 58: {  /* expecting: 5;n or 2;r;g;b */
			long color, advance;

			if (len >= 3 && vals[1] == 5)
				color = C_256(vals[2]);
			else if (len >= 5 && vals[1] == 2)
				color = C_RGB(vals[2], vals[3], vals[4]);
			else  /* invalid and we've lost sync, abort */
				return;

			if (v == 38)
				sgr->fg = color;
			else if (v == 48)
				sgr->bg = color;
			/* else ignore underline color */

			advance = vals[1] == 5 ? 2 : 4;
			vals += advance;
			len -= advance;
			break;
		}  /* 38 48 58 */
		}
	}
}

typedef unsigned char u8;

// static long long xdist_sq(u8 r,u8 g,u8 b, u8 R,u8 G,u8 B) {
// 	int rx = (r+R+1) / 2;
// 	return (long long)(512 + rx) * (r-R) * (r-R) / 256
// 	     + (long long)4 * (g-G) * (g-G)
// 	     + (long long)(512 + 255 - rx) * (b-B) * (b-B) / 256;
// }

#define dist_sq(r,g,b, R,G,B) ((r-R)*(r-R) + (g-G)*(g-G) + (b-B)*(b-B))

static int rgb2wincolor(u8 r, u8 g, u8 b)
{
	static const int c1 = 128, c2 = 255, w = 192, bb = 128;
	// static const int c1 = 64, c2 = 255, w = 0, bb = 0;
	// static const int c1 = 144, c2 = 255, w = 0, bb = 0;
	// static const int c1 = 72, c2 = 144, w = 128, bb = 96;
	// static const int c1 = 72, c2 = 220, w = 172, bb = 128;
	static const struct {u8 r, g, b;} xp[16] = {
		// black, red, green, yellow, blue, magenta, cyan, white
		// {0,0,0}, {128,0,0}, {0,128,0}, {128,128,0},
		// {0,0,128}, {128,0,128}, {0,128,128}, {192,192,192},
		// {128,128,128}, {255,0,0}, {0,255,0}, {255,255,0},
		// {0,0,255}, {255,0,255}, {0,255,255}, {255,255,255},

		{0,0,0}, {c1,0,0}, {0,c1,0}, {c1,c1,0},
		{0,0,c1}, {c1,0,c1}, {0,c1,c1}, {w,w,w},
		{bb,bb,bb}, {c2,0,0}, {0,c2,0}, {c2,c2,0},
		{0,0,c2}, {c2,0,c2}, {0,c2,c2}, {c2,c2,c2},
	};
	long long d_min = LONG_LONG_MAX;  // bigger than max possible distance
	int i_min, i;
	// #define dist_sq(r,g,b, R,G,B) (2*(r-R)*(r-R) + 4*(g-G)*(g-G) + 2*(b-B)*(b-B))
	for (i = 0; i < 16; ++i) {
		int d = dist_sq(r,g,b, xp[i].r, xp[i].g, xp[i].b);
		if (i==0 || i == 7 || i == 8 || i == 15)
			continue;//d *= 8;
		if (d < d_min) {
			d_min = d;
			i_min = i;
		}
	}
	return i_min;
}

int static get_win_color(long color, int def)
{
#if MSDOS_COMPILER==WIN32C
	/* Screen colors used by 3x and 4x SGR commands. */
	static unsigned char screen_color[] = {
		0, /* BLACK */
		FOREGROUND_RED,
		FOREGROUND_GREEN,
		FOREGROUND_RED|FOREGROUND_GREEN,
		FOREGROUND_BLUE,
		FOREGROUND_BLUE|FOREGROUND_RED,
		FOREGROUND_BLUE|FOREGROUND_GREEN,
		FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED
	};
#else
	static enum COLORS screen_color[] = {
		BLACK, RED, GREEN, BROWN,
		BLUE, MAGENTA, CYAN, LIGHTGRAY
	};
#endif

	static const int x256[6] = {0, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
	int r, g, b, tmp, any, high;

	switch (CGET_TYPE(color)) {
	case T_DEFAULT:
		return def;
	case T_ANSI:
		return screen_color[CGET_ANSI(color)];
	case T_RGB:
		r = CGET_R(color), g = CGET_G(color), b = CGET_B(color);
		break;
	default:  /* T_256 */
		tmp = CGET_256(color);
		if (tmp < 16)  /* 8 ANSI + 8 bright */
			return screen_color[tmp & 7] | (tmp & 8);

		tmp -= 16;
		if (tmp >= 216)  /* linear grayscale */
			r = g = b = 8 + 10 * (tmp - 216);
		else  /* non linear, indexed 6x6x6 color cube */
			r = x256[tmp / 36], g = x256[tmp / 6 % 6], b = x256[tmp % 6];
	}
	// {
	// 	int c = rgb2wincolor(r, g, b);
	// 	return screen_color[c & 7] | (c & 8);
	// }
#if 1
	/*
	 * convert RGB to 3 bit color + 1 bit brightness.
	 * each channel is considered in one of 3 states: off/low/high.
	 * for white/black do 2 bit grayscale, alse if there are highs then
	 * ignore the lows and do bright, else normal color without bright.
	 * the thresholds 112/196 are arbitrary-ish to work well in practice.
	 */
	any = (r > 112) + (g > 112) + (b > 112);
	high = (r > 196) + (g > 196) + (b > 196);
	tmp = (r + g + b) / 3;

	// int xs = r*r + b*b + g*g;
	// long gray_dist = ((long)3*tmp*tmp - (r*r + b*b + g*g));
	long gray_dist = dist_sq(tmp,tmp,tmp, r,g,b);

	/* bright threshold is 60 (not 64) to balance the 256 color grayscale */
	// if (any == 0 || (any == 3 && (high == 0 || high == 3)))  /* do grayscale */
	// if (any == 0 || (any == 3 && high != 2))  /* do grayscale */
	// if (gray_dist < 20*20)
	#define sq(x) ((x)*(x))
	if (gray_dist < sq(40))
	// if (dist_sq(r, g, b, tmp,tmp,tmp) < 1000)
		// return 2;
		// return 0;
		return screen_color[tmp & 0x80 ? 7 : 0]
			| ((tmp & ~0x80) >= 60 ? 8 : 0);
	// return 0;
	{
		int c = rgb2wincolor(r, g, b);
		return screen_color[c & 7] | (c & 8);
	}

	return high ? screen_color[(r > 196) | 2 * (g > 196) | 4 * (b > 196)] | 8
	            : screen_color[(r > 112) | 2 * (g > 112) | 4 * (b > 112)];

	int c = rgb2wincolor(r, g, b);
	return screen_color[c & 7] | (c & 8);
#endif
}

static void set_win_colors(sgr_state *sgr)
{
	int fg, bg, tmp;

	/* !sgr_mode -> exactly one attribute with default fg+bg maps to -Dx config */
	if (!sgr_mode && sgr->fg == C_DEFAULT && sgr->bg == C_DEFAULT) {
		switch (sgr->attr) {
		case A_BOLD:      WIN32setcolors(bo_fg_color, bo_bg_color); return;
		case A_UNDERLINE: WIN32setcolors(ul_fg_color, ul_bg_color); return;
		case A_BLINK:     WIN32setcolors(bl_fg_color, bl_bg_color); return;
		case A_INVERSE:   WIN32setcolors(so_fg_color, so_bg_color); return;

		/* there's no -Di so italic should not be here, but for backward
		 * compatibility: -Ds applies to standout (inverse), but because
		 * italic is applied as inverse, -Ds was applied to italic too
		 * TODO: support -Di... ? */
		case A_ITALIC:    WIN32setcolors(so_fg_color, so_bg_color); return;
		}
	}

	/* resolve the color, then apply attributes as additional mods */
	fg = get_win_color(sgr->fg, nm_fg_color);
	bg = get_win_color(sgr->bg, nm_bg_color);

	if (sgr->attr & A_BOLD)
		fg |= 8;

	if (sgr->attr & (A_BLINK | A_UNDERLINE))
		bg |= 8;

	if (sgr->attr & (A_INVERSE | A_ITALIC)) {
		tmp = fg;
		fg = bg;
		bg = tmp;
	}

	if (sgr->attr & A_CONCEAL)
		fg = bg ^ 8;

	WIN32setcolors(fg, bg);
}

enum PARSE_STATE {
	PS_NORMAL = 0,
	PS_ESC,        // right after ESC
	PS_CSI,        // after ESC[ and before the end of sequence
	PS_OSC,        // after ESC] and before ST or BEL
	PS_OSC_ESC,    // right after ESC in PS_OSC
};

/* we maintain a parsing state, and the sgr state of our virtual terminal.
 * specifically, we don't maintain a state of the current windows colors.
 * once an SGR sequence completes, we apply the sgr state as windows olors.
 * if the buffer ends inside a sequence, we'll continue from the exact place */
static void win_flush(void)
{
	static sgr_state sgr;
	static enum PARSE_STATE parse_state;
	static unsigned v;  /* currently parsed sgr value */
	static unsigned char sgr_vals[64];
	static int sgr_len;

#if MSDOS_COMPILER != WIN32C
	static const int vt_enabled = 0;  /* global only with WIN32C */
#endif

	char *p = obuf, *oend = ob;
	ob = obuf;  /* we always consume the whole buffer */

	if (ctldisp != OPT_ONPLUS || (vt_enabled && sgr_mode)) {
		WIN32textout(obuf, oend - obuf);
		return;
	}

	for (; p != oend; ++p) {
		switch (parse_state) {
		case PS_NORMAL: {
			char *pesc = memchr(p, ESC, oend - p);
			if (!pesc) {
				WIN32textout(p, oend - p);
				return;
			}
			if (pesc > p)
				WIN32textout(p, pesc - p);

			p = pesc;
			parse_state = PS_ESC;
			continue;
		}

		case PS_ESC:
			parse_state = *p == '[' ? PS_CSI
			            : *p == ']' ? PS_OSC
			            : PS_NORMAL; /* assume ESC-<one-char-thing> */

			/* OSC is passthrough on VT terminal, else eaten */
			if (*p == ']' && vt_enabled)
				WIN32textout("\033]", 2);
			continue;

		case PS_CSI:
			/* we parse the CSI as if it was an SGR sequence, eventhough
			 * we don't know that yet. we'll discard if it ends up not SGR. */
			if (*p >= '0' && *p <= '9') {
				v = v * 10 + *p - '0';
				continue;
			}
			/* separator or end of CSI */
			if (sgr_len < sizeof(sgr_vals))  /* ignore overflow vals */
				sgr_vals[sgr_len++] = v;
			v = 0;

			if ((unsigned char)*p < 0x40)
				continue;  /* still in CSI */

			/* end of CSI */
			if (is_ansi_end(*p)) {  /* which was SGR */
				/* len >= 1 always, with value 0 if got nothing */
				update_sgr(&sgr, sgr_vals, sgr_len);
				set_win_colors(&sgr);
			}
			sgr_len = 0;
			parse_state = PS_NORMAL;
			continue;

		case PS_OSC_ESC:
			if (*p == '\\') {
				if (vt_enabled)
					WIN32textout(p, 1);
				parse_state = PS_NORMAL;
				continue;
			}

			parse_state = PS_OSC;
			/* fallthrough: not ST, re-process *p as PS_OSC */

		case PS_OSC: {
			/* ends at ST ("\e\\") or BEL ('\a') */
			/* try to write in one go - avoid breaking UTF8 */
			char *p0 = p;
			while (p != oend && *p != '\a' && *p != ESC)
				++p;
			if (vt_enabled)  /* write also the '\a' or ESC */
				WIN32textout(p0, p - p0 + (p != oend));

			if (p == oend)
				return;
			parse_state = *p == '\a' ? PS_NORMAL : PS_OSC_ESC;
			continue;
		}

		}  /* switch */
	}
}

#endif  /* avih colors parser */

#endif

/*
 * Flush buffered output.
 *
 * If we haven't displayed any file data yet,
 * output messages on error output (file descriptor 2),
 * otherwise output on standard output (file descriptor 1).
 *
 * This has the desirable effect of producing all
 * error messages on error output if standard output
 * is directed to a file.  It also does the same if
 * we never produce any real output; for example, if
 * the input file(s) cannot be opened.  If we do
 * eventually produce output, code in edit() makes
 * sure these messages can be seen before they are
 * overwritten or scrolled away.
 */
public void flush(void)
{
	size_t n;

	n = ptr_diff(ob, obuf);
	if (n == 0)
		return;
	ob = obuf;

#if MSDOS_COMPILER==MSOFTC
	if (interactive())
	{
		obuf[n] = '\0';
		_outtext(obuf);
		return;
	}
#else
#if MSDOS_COMPILER==WIN32C || MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	if (interactive())
	{
		ob = obuf + n;
		*ob = '\0';
		win_flush();
		return;
	}
#endif
#endif

	if (write(outfd, obuf, n) != n)
		screen_trashed();
}

/*
 * Set the output file descriptor (1=stdout or 2=stderr).
 */
public void set_output(int fd)
{
	flush();
	outfd = fd;
}

/*
 * Output a character.
 * ch is int for compatibility with tputs.
 */
public int putchr(int ch)
{
	char c = (char) ch;
#if 0 /* fake UTF-8 output for testing */
	extern int utf_mode;
	if (utf_mode)
	{
		static char ubuf[MAX_UTF_CHAR_LEN];
		static int ubuf_len = 0;
		static int ubuf_index = 0;
		if (ubuf_len == 0)
		{
			ubuf_len = utf_len(c);
			ubuf_index = 0;
		}
		ubuf[ubuf_index++] = c;
		if (ubuf_index < ubuf_len)
			return c;
		c = get_wchar(ubuf) & 0xFF;
		ubuf_len = 0;
	}
#endif
	clear_bot_if_needed();
#if MSDOS_COMPILER
	if (c == '\n' && is_tty)
	{
		/* remove_top(1); */
		putchr('\r');
	}
#else
#ifdef _OSK
	if (c == '\n' && is_tty)  /* In OS-9, '\n' == 0x0D */
		putchr(0x0A);
#endif
#endif
	/*
	 * Some versions of flush() write to *ob, so we must flush
	 * when we are still one char from the end of obuf.
	 */
	if (ob >= &obuf[sizeof(obuf)-1])
		flush();
	*ob++ = c;
	at_prompt = 0;
	return (c);
}

public void clear_bot_if_needed(void)
{
	if (!need_clr)
		return;
	need_clr = 0;
	clear_bot();
}

/*
 * Output a string.
 */
public void putstr(constant char *s)
{
	while (*s != '\0')
		putchr(*s++);
}


/*
 * Convert an integral type to a string.
 */
#define TYPE_TO_A_FUNC(funcname, type) \
void funcname(type num, char *buf, int radix) \
{ \
	int neg = (num < 0); \
	char tbuf[INT_STRLEN_BOUND(num)+2]; \
	char *s = tbuf + sizeof(tbuf); \
	if (neg) num = -num; \
	*--s = '\0'; \
	do { \
		*--s = "0123456789ABCDEF"[num % radix]; \
	} while ((num /= radix) != 0); \
	if (neg) *--s = '-'; \
	strcpy(buf, s); \
}

TYPE_TO_A_FUNC(postoa, POSITION)
TYPE_TO_A_FUNC(linenumtoa, LINENUM)
TYPE_TO_A_FUNC(inttoa, int)

/*
 * Convert a string to an integral type.  Return ((type) -1) on overflow.
 */
#define STR_TO_TYPE_FUNC(funcname, cfuncname, type) \
type cfuncname(constant char *buf, constant char **ebuf, int radix) \
{ \
	type val = 0; \
	lbool v = 0; \
	for (;; buf++) { \
		char c = *buf; \
		int digit = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1; \
		if (digit < 0 || digit >= radix) break; \
		v = v || ckd_mul(&val, val, radix); \
		v = v || ckd_add(&val, val, digit); \
	} \
	if (ebuf != NULL) *ebuf = buf; \
	return v ? (type)(-1) : val; \
} \
type funcname(char *buf, char **ebuf, int radix) \
{ \
	constant char *cbuf = buf; \
	type r = cfuncname(cbuf, &cbuf, radix); \
	if (ebuf != NULL) *ebuf = (char *) cbuf; /*{{const-issue}}*/ \
	return r; \
}

STR_TO_TYPE_FUNC(lstrtopos, lstrtoposc, POSITION)
STR_TO_TYPE_FUNC(lstrtoi, lstrtoic, int)
STR_TO_TYPE_FUNC(lstrtoul, lstrtoulc, unsigned long)

/*
 * Print an integral type.
 */
#define IPRINT_FUNC(funcname, type, typetoa) \
static int funcname(type num, int radix) \
{ \
	char buf[INT_STRLEN_BOUND(num)]; \
	typetoa(num, buf, radix); \
	putstr(buf); \
	return (int) strlen(buf); \
}

IPRINT_FUNC(iprint_int, int, inttoa)
IPRINT_FUNC(iprint_linenum, LINENUM, linenumtoa)

/*
 * This function implements printf-like functionality
 * using a more portable argument list mechanism than printf's.
 *
 * {{ This paranoia about the portability of printf dates from experiences
 *    with systems in the 1980s and is of course no longer necessary. }}
 */
public int less_printf(constant char *fmt, PARG *parg)
{
	constant char *s;
	constant char *es;
	int col;

	col = 0;
	while (*fmt != '\0')
	{
		if (*fmt != '%')
		{
			putchr(*fmt++);
			col++;
		} else
		{
			++fmt;
			switch (*fmt++)
			{
			case 's':
				s = parg->p_string;
				es = s + strlen(s);
				parg++;
				while (*s != '\0')
				{
					LWCHAR ch = step_charc(&s, +1, es);
					constant char *ps = utf_mode ? prutfchar(ch) : prchar(ch);
					while (*ps != '\0')
					{
						putchr(*ps++);
						col++;
					}
				}
				break;
			case 'd':
				col += iprint_int(parg->p_int, 10);
				parg++;
				break;
			case 'x':
				col += iprint_int(parg->p_int, 16);
				parg++;
				break;
			case 'n':
				col += iprint_linenum(parg->p_linenum, 10);
				parg++;
				break;
			case 'c':
				s = prchar((LWCHAR) parg->p_char);
				parg++;
				while (*s != '\0')
				{
					putchr(*s++);
					col++;
				}
				break;
			case '%':
				putchr('%');
				break;
			}
		}
	}
	return (col);
}

/*
 * Get a RETURN.
 * If some other non-trivial char is pressed, unget it, so it will
 * become the next command.
 */
public void get_return(void)
{
	int c;

#if ONLY_RETURN
	while ((c = getchr()) != '\n' && c != '\r')
		bell();
#else
	c = getchr();
	if (c != '\n' && c != '\r' && c != ' ' && c != READ_INTR)
		ungetcc((char) c);
#endif
}

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
public void error(constant char *fmt, PARG *parg)
{
	int col = 0;
	static char return_to_continue[] = "  (press RETURN)";

	errmsgs++;

	if (!interactive())
	{
		less_printf(fmt, parg);
		putchr('\n');
		return;
	}

	if (!oldbot)
		squish_check();
	at_exit();
	clear_bot();
	at_enter(AT_STANDOUT|AT_COLOR_ERROR);
	col += so_s_width;
	col += less_printf(fmt, parg);
	putstr(return_to_continue);
	at_exit();
	col += (int) sizeof(return_to_continue) + so_e_width;

	get_return();
	lower_left();
	clear_eol();

	if (col >= sc_width)
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		screen_trashed();

	flush();
}

/*
 * Output a message in the lower left corner of the screen
 * and don't wait for carriage return.
 * Usually used to warn that we are beginning a potentially
 * time-consuming operation.
 */
static void ierror_suffix(constant char *fmt, PARG *parg, constant char *suffix1, constant char *suffix2, constant char *suffix3)
{
	at_exit();
	clear_bot();
	at_enter(AT_STANDOUT|AT_COLOR_ERROR);
	(void) less_printf(fmt, parg);
	putstr(suffix1);
	putstr(suffix2);
	putstr(suffix3);
	at_exit();
	flush();
	need_clr = 1;
}

public void ierror(constant char *fmt, PARG *parg)
{
	ierror_suffix(fmt, parg, "... (interrupt to abort)", "", "");
}

public void ixerror(constant char *fmt, PARG *parg)
{
	if (!supports_ctrl_x())
		ierror(fmt, parg);
	else
	{
		char ichar[MAX_PRCHAR_LEN+1];
		strcpy(ichar, prchar((LWCHAR) intr_char));
		ierror_suffix(fmt, parg, "... (", ichar, " or interrupt to abort)");
	}
}

/*
 * Output a message in the lower left corner of the screen
 * and return a single-character response.
 */
public int query(constant char *fmt, PARG *parg)
{
	int c;
	int col = 0;

	if (interactive())
		clear_bot();

	(void) less_printf(fmt, parg);
	c = getchr();

	if (interactive())
	{
		lower_left();
		if (col >= sc_width)
			screen_trashed();
		flush();
	} else
	{
		putchr('\n');
	}

	if (c == 'Q')
		quit(QUIT_OK);
	return (c);
}
