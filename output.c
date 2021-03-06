/*************************************************************
 *    ______                                                 *
 *   /     /\  TinyFugue was derived from a client initially *
 *  /   __/  \ written by Anton Rang (Tarrant) and later     *
 *  |  / /\  | modified by Leo Plotkin (Grod).  The early    *
 *  |  |/    | versions of TinyFugue written by Greg Hudson  *
 *  |  X__/  | (Explorer_Bob).  The current version is       *
 *  \ /      / written and maintained by Ken Keys (Hawkeye), *
 *   \______/  who can be reached at kkeys@ucsd.edu.         *
 *                                                           *
 *             No copyright 1992, no rights reserved.        *
 *             Fugue is in the public domain.                *
 *************************************************************/

/*****************************************************************
 * Fugue output handling                                         *
 *                                                               *
 * Screen handling written by Greg Hudson (Explorer_Bob) and     *
 * Ken Keys (Hawkeye).  Output queueing written by Ken Keys.     *
 * Handles all screen-related phenomena.                         *
 *****************************************************************/

#include <ctype.h>
#include <stdio.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "macro.h"
#include "output.h"

#ifdef HARDCODE
   smallstr BUF;
#  define clear_line()     ewrite("\r\033[K")
#  define clear_to_end()   ewrite("\033[K")
#  define attributes_off() ewrite("\033[m")
#  define enable_cm()      /* not used by vt100 */
#  define disable_cm()     /* not used by vt100 */
#  define xy(x,y)     do { \
                          sprintf(BUF,"\033[%d;%dH",(y),(x)); ewrite(BUF); \
                      } while (0)
#  define scroll(Y,y) 

#else
#  ifdef TERMCAP
     static int   FDECL(getcap,(char *cap, Stringp str));
     static void  FDECL(tp,(char *s));
#    define clear_to_end()   tp(clear_to_eol->s)
#    define attributes_off() tp(attr_off->s)
#    define enable_cm()      tp(ti->s)
#    define disable_cm()     tp(te->s)
#    define xy(x,y)          tp(tgoto(cm->s, (x) - 1, (y) - 1));
#    define scroll(Y,y)      tp(tgoto(cs->s, (y) - 1, (Y) - 1));

#  else
#    define clear_to_end()   /* not supported w/o TERMCAP or HARDCODE */
#    define attributes_off() /* not supported w/o TERMCAP or HARDCODE */
#    define xy(x,y)          /* not supported w/o TERMCAP or HARDCODE */
#    define scroll(Y,y)      /* not supported w/o TERMCAP or HARDCODE */
#  endif
#endif

#ifndef HARDCODE
static void  NDECL(clear_line);
#endif
static void  NDECL(empty_input_window);
static void  NDECL(scroll_input);
static void  FDECL(ioutput,(int c));
static void  FDECL(lwrite,(char *str, int len));
static void  FDECL(hwrite,(char *s, int len, int attrs));
static void  NDECL(discard_screen_queue);
static int   NDECL(check_more);
static char *FDECL(wrapline,(int *lenp, short *attrp));
static void  NDECL(output_novisual);
#ifdef VISUAL
static void  NDECL(output_noscroll);
static void  NDECL(output_scroll);
#else
# define output_noscroll()      /* not supported without TERMCAP or HARDCODE */
# define output_scroll()        /* not supported without TERMCAP or HARDCODE */
#endif
#define ewrite(str) lwrite(str, strlen(str))
#define tputch(c) (putch(((c) == '\t') ? '@' : (c)))

void NDECL(ipos);
void NDECL(clr);
void FDECL(putch,(int c));
void NDECL(init_term);
void NDECL(setup_screen);
void NDECL(oflush);
void FDECL(clear_more,(int new));
#ifdef VISUAL
void FDECL(put_world,(char *name));
void FDECL(put_mail,(int flag));
void FDECL(put_logging,(int flag));
void FDECL(put_active,(int count));
#endif
void NDECL(toggle_insert);
void NDECL(fix_screen);
void NDECL(clear_input_window);
void FDECL(iputs,(char *s));
void NDECL(inewline);
void NDECL(ibs);
void FDECL(ibackword,(int place));
void FDECL(newpos,(int place));
void NDECL(dEOL);
void NDECL(do_line_refresh);
void NDECL(do_replace);
void NDECL(reset_outcount);
void FDECL(localoutput,(Aline *aline));
void FDECL(aoutput,(Aline *aline));
void FDECL(enable_wrap,(int column));
void NDECL(disable_wrap);
int  NDECL(getwrap);
void FDECL(setprompt,(char *s));

void NDECL(do_refresh);
void NDECL(do_page);
void NDECL(do_hpage);
void NDECL(do_line);
void NDECL(do_flush);

#define Wrap (current_wrap_column ? current_wrap_column : columns)
#define keyboard_end (keybuf->len)

#define DEFAULT_LINES 25
#define DEFAULT_COLUMNS 80
#define DEFAULT_BOARD_SIZE 13

extern char board_buffer[2048];
extern int do_board();
extern char *board_diff();

/* Flags */

extern int beeping, hilite, gag;
extern int visual;              /* Visual mode? */
extern int clear;               /* Clear rather than scroll input? */
extern int cleardone;           /* Clear input window for each line? */
extern int wrapspace;           /* Indent wrapped lines */
extern int more;                /* paging: 0 == off, 1 == on, 2 == paused */
extern int log_on;
extern int board;
extern int style;

/* Termcap strings and flags */

#ifdef TERMCAP
static Stringp write_buffer;
static Stringp ti;             /* Cursor motion mode */
static Stringp te;             /* Cursor motion mode */
static Stringp cl;             /* Clear screen       */
static Stringp cm;             /* Move cursor        */
static Stringp cs;             /* Set scroll area    */
static Stringp underline;      /* termcap us         */
static Stringp reverse;        /* termcap mr         */
static Stringp blink;          /* termcap mb         */
static Stringp dim;            /* termcap mh         */
static Stringp bold;           /* termcap md         */
static Stringp attr_off;       /* termcap me         */
static Stringp start_of_line;  /* termcap cr or '\r' */
static Stringp clear_to_eol;   /* termcap ce         */
#endif
static short have_attr;
static int have_clear, have_scroll;

int input_cursor = TRUE;            /* is cursor in input window position? */
int can_have_visual = FALSE;        /* cm, cl and ce caps necessary */

/* Others */

extern int keyboard_pos;            /* position of logical cursor in keybuf */
extern int mail_size;               /* size of mail file */
extern int active_count;            /* number of active sockets */
extern Stringp keybuf;              /* input buffer */

static int ox, oy, oy1;             /* Current position in output window */
static int ix, iy, iy1;             /* Current position in input window */
static int bx,by,bendy;             /* Current position of bg board */
static int linestart, iendx, iendy;
static Stringp lpprompt;            /* LP prompt */
static Stringp moreprompt;          /* pager prompt */
static String *prompt;              /* current prompt */
static int default_wrap_column;
static int outcount;                /* lines remaining until more prompt */
static short more_attrs;            /* attributes for more prompt */
static Aline *currentline = NULL;   /* current logical line for printing */
static char *physline = NULL;       /* start of next physical line */

int ilines = 0;                     /* input window size */
int insert = TRUE;                  /* keyboard insert mode */
int current_wrap_column;
int lines, columns;                 /* Take a wild guess */
int screen_setup = FALSE;           /* is *screen* in visual mode? */
Queue screen_queue[1];              /* text waiting to be displayed */
Queue *output_dest = screen_queue;  /* where to queue output */

#ifdef TERMCAP
extern char *FDECL(tgoto,(char *code, int destcol, int destline));
extern char *FDECL(tgetstr,(char *id, char **area));
#endif

/********************************
 * INTERMEDIARY OUTPUT ROUTINES *
 ********************************/

void putch(c)
    char c;
{
    while (write(1, &c, 1) != 1);
}

static void lwrite(str, len)
    char *str;
    int len;
{
    int numwritten;

    while (len > 0) {
        if ((numwritten = write(1, str, len)) == -1) numwritten = 0;
        len -= numwritten;
        str += numwritten;
        if (len > 0) sleep(1);
    }
}

/********************************************************
 *                                                      *
 *                   TERMCAP ROUTINES                   *
 *                                                      *
 ********************************************************/

#ifdef TERMCAP
static int getcap(cap, str)
    char *cap;
    Stringp str;
{
    char tempstr[1024];    /* max length of termcap string is 1024 */
    char *temp;

    temp = tempstr;
    if (tgetstr(cap, &temp) == NULL) return 0;
    else Stringcpy(str, tempstr);
    return 1;
}
#endif

void init_term()
{
    char blob[1024];
    char *termtype;

    do_board(NULL);
    ix = 1;
    init_queue(screen_queue);
    Stringinit(lpprompt);
    Stringinit(moreprompt);
    Stringcpy(moreprompt, "--More--");
    prompt = lpprompt;
    lines = DEFAULT_LINES;
    columns = DEFAULT_COLUMNS;
#ifdef HARDCODE
    have_attr = F_UNDERLINE | F_REVERSE | F_FLASH | F_HILITE | F_BELL;
    have_scroll = FALSE;
    can_have_visual = TRUE;
    have_clear = TRUE;
#else
# ifdef TERMCAP
    Stringinit(write_buffer);
    Stringinit(start_of_line);
    Stringinit(clear_to_eol);
    Stringinit(ti);
    Stringinit(te);
    Stringinit(cl);
    Stringinit(cm);
    Stringinit(cs);
    Stringinit(underline);
    Stringinit(reverse);
    Stringinit(blink);
    Stringinit(dim);
    Stringinit(bold);
    Stringinit(attr_off);
    outcount = lines;
    current_wrap_column = default_wrap_column = columns - 1;
    if ((termtype = getenv("TERM")) == NULL) {
        oputs("% Warning: null terminal type");
        return;
    }
    if (tgetent(blob, termtype) != 1) {
        oprintf("% Warning: terminal type \"%s\" unsupported.", termtype);
        return;
    }
    have_attr = 0;
    have_clear = have_scroll = can_have_visual = TRUE;

    if ((columns = tgetnum("co")) == -1) columns = DEFAULT_COLUMNS;
    current_wrap_column = default_wrap_column = columns - 1;
    if ((lines = tgetnum("li")) == -1) lines = DEFAULT_LINES;
    if (!getcap("cr", start_of_line)) Stringcpy(start_of_line, "\r");
    if (!getcap("ce", clear_to_eol)) can_have_visual = have_clear = FALSE;
    if (!getcap("ti", ti)) Stringcpy(ti, "");
    if (!getcap("te", ti)) Stringcpy(te, "");
    if (!getcap("cl", cl)) can_have_visual = FALSE;
    if (!getcap("cm", cm)) can_have_visual = FALSE;
    if (!getcap("cs", cs)) have_scroll = FALSE;
    if (getcap("us", underline))  have_attr |= F_UNDERLINE;
    if (getcap("mr", reverse))    have_attr |= F_REVERSE;
    if (getcap("mb", blink))      have_attr |= F_FLASH;
    if (getcap("mh", dim))        have_attr |= F_DIM;
    if (getcap("md", bold))       have_attr |= F_HILITE;
    else if (getcap("so", bold))  have_attr |= F_HILITE;
    if (!getcap("me", attr_off)) have_attr = 0;
    have_attr |= F_BELL;
# else
    have_attr = F_BELL;
    have_clear = have_scroll = can_have_visual = FALSE;
# endif
#endif
    outcount = lines;
}

void ipos()
{
    input_cursor = TRUE;
    xy(ix, iy);
}

void clr()
{
#ifdef HARDCODE
    ewrite("\033[2J\033[H");
#else
# ifdef TERMCAP
    tp(cl->s);
# endif
#endif
}

#ifndef HARDCODE
static void clear_line()
{
    STATIC_BUFFER(buffer)

#ifdef TERMCAP
    if (have_clear) {
        tp(start_of_line->s);
        tp(clear_to_eol->s);
        return;
    }
#endif
    Stringterm(buffer, 0);
    Stringadd(buffer, '\r');
    Stringnadd(buffer, ' ', ix - 1);
    Stringadd(buffer, '\r');
    lwrite(buffer->s, buffer->len);
}
#endif

static void attributes_on(attr)
    int attr;
{
    if ((attr & F_BELL) && beeping) putch('\007');
#ifdef HARDCODE
    if (!hilite) return;
    if (attr & F_UNDERLINE) ewrite("\033[4m");
    if (attr & F_REVERSE)   ewrite("\033[7m");
    if (attr & F_FLASH)     ewrite("\033[5m");
    if (attr & F_HILITE)    ewrite("\033[1m");
#else
# ifdef TERMCAP
    if (!hilite) return;
    if (have_attr & attr & F_UNDERLINE) tp(underline->s);
    if (have_attr & attr & F_REVERSE)   tp(reverse->s);
    if (have_attr & attr & F_FLASH)     tp(blink->s);
    if (have_attr & attr & F_DIM)       tp(dim->s);
    if (have_attr & attr & F_HILITE)    tp(bold->s);
# endif
#endif
}

#ifdef TERMCAP
static void tp(s)
    char *s;
{
    tputs(s, 1, putch);
    fflush(stdout);
}
#endif

/*****************************************************
 *                                                   *
 *                  WINDOW HANDLING                  *
 *                                                   *
 *****************************************************/

void setup_screen()
{
    int i;
    char *line;
    Stringp scr;
    World *world;

    attributes_off();
    if (!visual) {
        if (more == 2) prompt = moreprompt;
#if 0
        do_replace();
#endif
        return;
    }
#ifdef VISUAL
    clr();
    prompt = lpprompt;
    if (board)
    {
      switch (style)
      {
        case 1: 
          bendy=14+1;
          break;
        case 2:
          bendy=16+1;
          break;
        case 4:
          bendy=13+1;
          break;
        default:
          bendy = DEFAULT_BOARD_SIZE + 1;
          break;
      }
    }
    else
      bendy = 0;
    if (!ilines) ilines = 3;
    if (ilines > lines - 3 - bendy)
      ilines = lines - 3 - bendy;
    iy1 = lines - ilines + 1;
    oy1 = iy1 - 2;
    outcount = oy1 - bendy;
    enable_cm();

    if (have_scroll) {
        Stringinit(scr);
        Stringnadd(scr, '\n', ilines);
        scroll(1, lines);
        xy(1, lines);
        lwrite(scr->s, scr->len);
        Stringfree(scr);
    } else clr();

    line = MALLOC(columns + 1);
    strcpy(line, "_________");
    if (world = fworld()) strncat(line, world->name, columns - 40 - 10 - 1);
    for (i = strlen(line); i < columns - 40 - 1; i++) line[i] = '_';
    if (active_count) sprintf(line + i, "(Active:%2d)_", active_count);
    else strcpy(line + i, "____________");
    strcat(line, log_on ? "(Logging)_" : "__________");
    strcat(line, mail_size ? "(Mail)__Insert: " : "________Insert: ");
    strcat(line, (insert) ? "On_" : "Off");
    xy(1, iy1 - 1);
    ewrite(line);
    if (board)
    {
       for (i = 0; i < columns; i++) line[i] = '_';
       xy(1,bendy);
       ewrite(line);
       xy(1,1);
       ewrite(board_buffer);
    }
    FREE(line);
    if (more == 2) {
        xy(1, oy1 + 1);
        hwrite(moreprompt->s, moreprompt->len, more_attrs);
    }

    if (have_scroll) scroll(bendy+1, oy1);
    ix = iendx = ox = 1;
    oy = bendy + 1;
    by = bx = 1;
    iy = iendy = linestart = iy1;
    ipos();
    screen_setup = TRUE;
#endif
}

#ifdef VISUAL
void put_world(name)
    char *name;
{
    char *line;
    int i, len;

    if (!visual) return;
    len = columns - 40 - 10 - 1;
    line = MALLOC(len);
    strcpy(line, "");
    if (name) strncat(line, name, len);
    for (i = strlen(line); i < len; i++) line[i] = '_';
    line[i] = '\0';
    xy(10, iy1-1);
    ewrite(line);
    FREE(line);
    ipos();
}

void put_mail(flag)
    int flag;
{
    if (screen_setup) {
        xy(columns - 18, iy1 - 1);
        ewrite(flag ? "(Mail)" : "______");
        ipos();
    }
}

void put_logging(flag)
    int flag;
{
    if (screen_setup) {
        xy(columns - 28, iy1 - 1);
        ewrite(flag ? "(Logging)" : "_________");
        ipos();
    }
}

void put_active(count)
    int count;
{
    smallstr buf;
    if (screen_setup) {
        xy(columns - 40, iy1 - 1);
        if (count) {
            sprintf(buf, "(Active:%2d)", count);
            ewrite(buf);
        } else ewrite("___________");
        ipos();
    }
}
#endif

void toggle_insert()
{
    insert = !insert;
    if (screen_setup) {
        xy(columns - 2, iy1 - 1);
        ewrite(insert ? "On_" : "Off");
        ipos();
    }
}

void fix_screen()
{
    empty_input_window();
#ifdef VISUAL
    if (have_scroll) scroll(1, lines);
    xy(1, iy1 - 1);
    input_cursor = TRUE;
    clear_line();
    disable_cm();
    screen_setup = 0;
    outcount = lines - 1;
#endif
}

static void empty_input_window()
{
    int i;

    if (!visual) clear_input_window();
    for (i = iy1; i <= lines; i++) {
        xy(1, i);
        clear_line();
    }
    ix = iendx = 1;
    iy = iendy = linestart = iy1;
    ipos();
}

void clear_input_window()
{
    int i;

    if (!visual) {
        clear_line();
        iendx = ix = 1;
        return;
    }
    for (i = linestart; i <= iendy; i++) {
        xy(1, i);
        clear_to_end();
    }
    ix = iendx = 1;
    iy = iendy = linestart;
    ipos();
}

static void scroll_input()
{
    scroll(iy1, lines);
    xy(1, lines);
    putch('\n');
    scroll(bendy+1, oy1);
    xy(iendx = 1, iy = lines);
}

/***********************************************************************
 *                                                                     *
 *                        INPUT WINDOW HANDLING                        *
 *                                                                     *
 ***********************************************************************/

static void ioutput(c)
    char c;
{
    if (visual) {
        if (iendy == lines && iendx > Wrap) return;
        tputch(c);
        if (++iendx > Wrap && iendy != lines) xy(iendx = 1, ++iendy);
    } else {
        if (iendx > Wrap) return;
        iendx++;
        tputch(c);
    }
}

void iputs(s)
    char *s;
{
    int i, j, oiex, oiey;

    if (!s[0]) return;
    if (!visual) {
        for (j = 0; s[j]; j++) {
            putch(s[j]);
            if (++iendx > Wrap) iendx = Wrap;
            if (++ix > Wrap) {
                putch('\n');
                iendx = ix = 1;
            }
        }
        if (insert || ix == 1) {
            iendx = ix;
            for (i = keyboard_pos; i < keyboard_end; i++)
                ioutput(keybuf->s[i]);
            for (i = ix; i < iendx; i++) putch('\010');
        }
        return;
    }

    if (!insert) {
        oiex = iendx;
        oiey = iendy;
    }
    for (j = 0; s[j]; j++) {
        iendx = ix;
        iendy = iy;
        if (ix == Wrap && iy == lines) {
            if (have_scroll && ilines > 1 && !clear) {
                tputch(s[j]);
                scroll_input();
                ix = 1;
                ipos();
                if (--linestart < iy1) linestart = iy1;
            } else {
                tputch(s[j]);
                empty_input_window();
            }
        } else ioutput(s[j]);
        ix = iendx;
        iy = iendy;
    }
    if (insert) {
        for (i = keyboard_pos; i < keyboard_end; i++) ioutput(keybuf->s[i]);
        if (keyboard_pos != keyboard_end) ipos();
    } else {
        iendx = oiex;
        iendy = oiey;
    }
}

void inewline()
{
    Stringterm(lpprompt, 0);
    if (!visual) {
        putch('\n');
        ix = iendx = 1;
        if (prompt->len > 0) set_refresh_pending();
        return;
    }

    if (!input_cursor) ipos();
    if (cleardone) {
        linestart = iy1;
        clear_input_window();
        iy = iy1;
        ix = iendx = 1;
        return;
    }
    iy = iendy;
    if (++iy > lines) {
        if (have_scroll && ilines > 1 && !clear) {
            scroll_input();
            linestart = lines;
        } else {
            empty_input_window();
            iy = iy1;
        }
    } else linestart = iy;
    iendy = iy;
    ix = iendx = 1;
}

void ibs()
{
    int kstart, pstart, i;

    if (!visual) {
        ix--;
        if (ix) {
            putch('\010');
            iendx = ix;
            for (i = keyboard_pos; i < keyboard_end; i++)
                ioutput(keybuf->s[i]);
            ioutput(' ');
            for (i = keyboard_pos; i < keyboard_end; i++) putch('\010');
            putch('\010');
        } else {
            ix = Wrap;
            do_line_refresh();
        }
      return;
    }

    if (!input_cursor) ipos();
    if (ix == 1 && iy == iy1) {              /* Move back a screen. */
        empty_input_window();
        kstart = keyboard_pos - (Wrap * ilines) + 1;
        if (kstart < 0) {
            pstart = prompt->len + kstart;
            if (pstart < 0) pstart = 0;
            kstart = 0;
        } else pstart = prompt->len;
        while (prompt->s[pstart]) ioutput(prompt->s[pstart++]);
        for (i = kstart; i < keyboard_pos; i++) ioutput(keybuf->s[i]);
        ix = iendx;
        iy = iendy;
        for (i = keyboard_pos; i < keyboard_end; i++)
            ioutput(keybuf->s[i]);
        ipos();
        return;
    }
    if (ix == 1) {
        ix = Wrap;
        iy--;
        ipos();
    } else {
        ix--;
        putch('\010');
    }
    iendx = ix;
    iendy = iy;
    for (i = keyboard_pos; i < keyboard_end; i++)
        ioutput(keybuf->s[i]);
    putch(' ');
  
    if ((keyboard_pos == keyboard_end) && (ix != Wrap)) putch('\010');
    else ipos();
}

void ibackword(place)
    int place;
{
    int kstart, pstart, i, oiex, oiey;

    if (!visual) {
        if (place == keyboard_end)
            for (i = place; i < keyboard_pos; i++) ibs();
        else {
            keyboard_pos = place;
            do_line_refresh();
        }
        return;
    }

    if (!input_cursor) ipos();
    ix -= (keyboard_pos - place);
    if (ix < 1) {
        ix += Wrap;
        iy--;
        if (iy < iy1) {
            empty_input_window();
            kstart = place - (Wrap * ilines) + (Wrap - ix) + 1;
            if (kstart < 0) {
                pstart = prompt->len + kstart;
                if (pstart < 0) pstart = 0;
                kstart = 0;
            } else pstart = prompt->len;
            while(prompt->s[pstart]) ioutput(prompt->s[pstart++]);
            for (i = kstart; i < place; i++) ioutput(keybuf->s[i]);
            ix = iendx;
            iy = iendy;
            for (i = place; i < keyboard_end; i++)
                ioutput(keybuf->s[i]);
            if (keyboard_pos != keyboard_end) ipos();
            return;
        }
    }
    ipos();
    iendx = ix;
    iendy = iy;
    for (i = place; i < keyboard_end; i++)
        ioutput(keybuf->s[i]);

    oiex = iendx;
    oiey = iendy;
    for (i = place; i < keyboard_pos; i++) ioutput(' ');
    iendx = oiex;
    iendy = oiey;
    ipos();
}

void newpos(place)
    int place;
{
    int diff, nix, niy, i, kstart, pstart;

    if (place < 0) place = 0;
    if (place > keyboard_end) place = keyboard_end;
    if (place == keyboard_pos) return;
    if (!visual) {
        nix = ix + place - keyboard_pos;
        if (nix == ix) return;
        if (nix < 1 || nix > Wrap) {
            keyboard_pos = place;
            do_line_refresh();
        } else {
            if (nix < ix) 
                for (i = keyboard_pos; i > place; i--) putch('\010');
            else for (i = keyboard_pos; i < place; i++) putch(keybuf->s[i]);
            ix = nix;
        }
        keyboard_pos = place;
        return;
    }

    diff = place - keyboard_pos;
    if (diff == 0) return;
    niy = iy + diff / Wrap;
    nix = ix + diff % Wrap;

    if (nix > Wrap) {
        niy++;
        nix -= Wrap;
    }
    while (niy > lines) {
        kstart = keyboard_pos + (lines - iy + 1) * Wrap - ix + 1;
        scroll_input();
        for (i = kstart; i < keyboard_end; i++) ioutput(keybuf->s[i]);
        keyboard_pos += (lines - iy1 + 1) * Wrap;
        niy--;
    }

    if (nix < 1) {
        niy--;
        nix += Wrap;
    }
    if (niy < iy1) {
        kstart = keyboard_pos - (iy - niy) * Wrap - ix + 1;
        if (kstart < 0) {
            pstart = prompt->len + kstart;
            if (pstart < 0) pstart = 0;
            kstart = 0;
        } else pstart = prompt->len;
        empty_input_window();
        while (prompt->s[pstart]) ioutput(prompt->s[pstart++]);
        while (keybuf->s[kstart]) ioutput(keybuf->s[kstart++]);
        niy = iy1;
    }

    ix = nix;
    iy = niy;

    ipos();
    keyboard_pos = place;
}

void dEOL()
{
    int i;

    if (!visual) {
        for (i = ix; i <= iendx; i++) putch(' ');
        for (i = ix; i <= iendx; i++) putch('\010');
        return;
    }
    clear_to_end();
    for (i = iy + 1; i <= iendy; i++) {
        xy(1, i);
        clear_to_end();
    }
    iendx = ix;
    iendy = iy;
    ipos();
}

void do_refresh()
{
    int oix, oiy, kpos, ppos;

    if (!visual) {
        do_replace();
        return;
    }

    kpos = keyboard_pos - (iy - linestart) * Wrap - ix + 1;
    if (kpos < 0) {
        ppos = prompt->len + kpos;
        kpos = 0;
    } else ppos = prompt->len;

    oix = ix;
    oiy = iy;
    clear_input_window();
    while (prompt->s[ppos]) ioutput(prompt->s[ppos++]);
    while (keybuf->s[kpos]) ioutput(keybuf->s[kpos++]);
    ix = oix;
    iy = oiy;
    if (keyboard_pos != keyboard_end) ipos();
}

void do_line_refresh()
{
    int curcol, start, pstart, i;

    if (!visual && more == 2) return;
    clear_refresh_pending();
    if (visual) {
        ipos();
        return;
    }

    curcol = (prompt->len + keyboard_pos) % Wrap;
    start = keyboard_pos - curcol;
    if (start < 0) {
        pstart = prompt->len + start;
        start = 0;
    } else pstart = prompt->len;

    clear_input_window();
    while (prompt->s[pstart]) ioutput(prompt->s[pstart++]);
    while (keybuf->s[start]) ioutput(keybuf->s[start++]);
    ix = curcol + 1;
    for (i = iendx; i > ix; i--) putch('\010');
}

void do_replace()
{
    int i, okpos;

    clear_input_window();
    okpos = keyboard_pos;
    keyboard_pos = keyboard_end;
    iputs(prompt->s);
    iputs(keybuf->s);
    if (visual) newpos(okpos);
    keyboard_pos = okpos;
    if (!visual) {
        if (ix - keyboard_end + keyboard_pos >= 1)
            for (i = keyboard_pos; i < keyboard_end; i++) putch('\010');
        else {
            putch('\n');
            do_line_refresh();
        }
    }
    clear_refresh_pending();
}


/*****************************************************
 *                                                   *
 *                  OUTPUT HANDLING                  *
 *                                                   *
 *****************************************************/

/*************
 * Utilities *
 *************/

static void hwrite(s, len, attrs)
    char *s;
    int len, attrs;
{
    if (attrs) attributes_on(attrs);
    lwrite(s, len);
    if (attrs & have_attr) attributes_off();
}

void reset_outcount()
{
    outcount = visual ? oy1 - bendy : lines - 1;
}

/* return TRUE if okay to print */
static int check_more()
{
    if (more == 2) return FALSE;
    if (!more || ox != 1) return TRUE;
    if (outcount-- > 0) return TRUE;

    more = 2;                                   /* output is paused */
    if ((more_attrs = do_hook(H_MORE, NULL, "")) == 0)
        more_attrs = F_HILITE | F_REVERSE;
    if (visual) {
        xy(1, oy1 + 1);
        hwrite(moreprompt->s, moreprompt->len, more_attrs);
        ipos();
    } else {
        prompt = moreprompt;
        do_replace();
    }
    return FALSE;
}

void clear_more(new)
    int new;
{
    if (more != 2) return;
    more = new;
    if (visual) {
        xy(1, oy1 + 1);
        ewrite("________");
        ipos();
    } else {
        prompt = lpprompt;
        clear_input_window();
        set_refresh_pending();
    }
    oflush();
}

void do_page()
{
    if (more != 2) return;
    outcount = visual ? oy1 - bendy : lines - 1;
    clear_more(1);
}

void do_hpage() 
{
    if (more != 2) return;
    outcount = (visual ? oy1 - bendy : lines - 1) / 2;
    clear_more(1);
}

void do_line()
{
    if (more != 2) return;
    outcount = 1;
    clear_more(1);
}

void do_flush()
{
    if (more != 2) return;
    discard_screen_queue();
}

static void discard_screen_queue()
{
    outcount = visual ? oy1 - bendy : lines - 1;
    free_queue(screen_queue);
    if (currentline) {
        free_aline(currentline);
        currentline = NULL;
        physline = NULL;
    }
    clear_more(1);
    localoutput(new_aline("--- Output discarded ---", F_NEWLINE));
}

/* return the next physical line to be printed */
static char *wrapline(lenp, attrp)
    int *lenp;
    short *attrp;
{
    static short firstline = TRUE;
    char *place, *max;
    STATIC_BUFFER(dest)

    while (!currentline || (currentline->attrs & F_GAG) && gag) {
        if (currentline) free_aline(currentline);
        if (!(currentline = dequeue(screen_queue))) return NULL;
    }

    if (!check_more()) return NULL;
    Stringterm(dest, 0);
    if (!physline) {
        physline = currentline->str;
        firstline = TRUE;
    }
    max = physline + Wrap - ox + 1;
    *attrp = currentline->attrs;
    if (!firstline) {
        *attrp &= ~F_BELL;
        if (current_wrap_column) {
            Stringnadd(dest, ' ', wrapspace);
            max -= wrapspace;
        }
    }

    for (place = physline; *place && place < max && *place != '\n'; place++);
    if (!*place) {
        Stringcat(dest, physline);
        *lenp = dest->len;
        firstline = currentline->attrs & F_NEWLINE;
    } else if (*place == '\n') {
        Stringncat(dest, physline, place - physline);
        *lenp = dest->len;
        firstline = TRUE;
        *attrp |= F_NEWLINE;
        physline = ++place;
    } else {
        if (current_wrap_column)
            while (place != physline && !isspace(*place)) place--;
        if (place == physline) {
            Stringncat(dest, physline, max - physline);
            physline = max;
        } else {
            Stringncat(dest, physline, place - physline);
            if (current_wrap_column) for (++place; isspace(*place); ++place);
            physline = place;
        }
        *attrp |= F_NEWLINE;
        *lenp = dest->len;
        firstline = FALSE;
    }
    if (!*place) {
        physline = NULL;
        free_aline(currentline);
        currentline = NULL;
    }
    return(dest->s);
}


/****************
 * Main drivers *
 ****************/

void aoutput(aline)
    Aline *aline;
{
    record_local(aline);
    localoutput(aline);
}

void localoutput(aline)
    Aline *aline;
{
    if (output_dest == screen_queue && !currentline && !output_dest->head)
        /* shortcut if screen queue is empty */
        (currentline = aline)->links++;
    else
        enqueue(output_dest, aline);
    if (output_dest == screen_queue && more != 2) oflush();
}

void oflush()
{
    if (!visual || !screen_setup) output_novisual();
    else if (!have_scroll) output_noscroll();
    else output_scroll();
}

static void output_novisual()
{
    char *line;
    int len;
    short attrs;

    if (ix != 1) {
        clear_input_window();
        set_refresh_pending();
    }
    while ((line = wrapline(&len, &attrs)) != NULL) {
        hwrite(line, len, attrs);
        if (attrs & F_NEWLINE) {
            putch('\n');
            ox = 1;
        } else ox = len + 1;
    }
}

#ifdef VISUAL
static void output_noscroll()
{
    char *line;
    static char board_line[1024];
    int i;
    int len;
    short attrs;
    static short newline_held = TRUE;
    char *diff;
    int x;
    int y;

    while ((line = wrapline(&len, &attrs)) != NULL) {
        if (ox == 1) {
		if(oy == (oy1))
		{
			xy(1,bendy+2);

		}
                else if(oy == (oy1-1))
                {
                        xy(1,bendy+1);
                }
		else
            		xy(1, (oy + 2));
            clear_to_end();
        }


        xy(ox, oy);
        input_cursor = FALSE;

        if (board && strncmp(line,"board:",6) == 0) {
            strcpy(board_line,line);
            strcat(board_line,wrapline(&len,&attrs));
            do_board(board_line);
            while ((diff=board_diff(&x,&y)))
            {
              xy(x,y);
              hwrite(diff,strlen(diff),attrs);
            }
            xy(ox,oy1);
        }
        else
	{
            hwrite(line, len, attrs);
	    if (attrs & F_NEWLINE) {
            	ox = 1;
            	if ((oy % oy1 + 1) == 1)
		{
			oy = bendy+1;
		 	xy(ox, oy);
		}
		else
		{
			oy++;
			xy(ox,oy);
		}
            } 
	    else 
		{
			ox = len + 1;
		}
        }
     }
}

static void output_scroll()
{
    char *line;
    static char board_line[1024];
    int i;
    int len;
    short attrs;
    static short newline_held = TRUE;
    char *diff;
    int x;
    int y;

    while ((line = wrapline(&len, &attrs)) != NULL) {
        if (input_cursor) {
            xy(ox, oy1);
            input_cursor = FALSE;
        }
        if (newline_held) putch('\n');
        if (newline_held = attrs & F_NEWLINE) ox = 1;
        else ox = len + 1;
        if (board && strncmp(line,"board:",6) == 0) {
            strcpy(board_line,line);
            strcat(board_line,wrapline(&len,&attrs));
            do_board(board_line);
            while ((diff=board_diff(&x,&y)))
            {
              xy(x,y);
              hwrite(diff,strlen(diff),attrs);
            }
            xy(ox,oy1);
        }
        else {
            hwrite(line, len, attrs);
            xy(ox, oy1);
        }
    }
}
#endif

/***********************************
 * Interfaces with rest of program *
 ***********************************/

void enable_wrap(column)
    int column;
{
    if (column) default_wrap_column = current_wrap_column = column;
    else current_wrap_column = default_wrap_column;
}

void disable_wrap()
{
    current_wrap_column = 0;
}

int getwrap()
{
    return (Wrap);
}

void setprompt(s)
    char *s;
{
    if (strcmp(lpprompt->s, s)) {
        Stringcpy(lpprompt, s);
        do_replace();
    }
}

#ifdef DMALLOC
void free_term()
{
#ifdef TERMCAP
    Stringfree(write_buffer);
    Stringfree(start_of_line);
    Stringfree(clear_to_eol);
    Stringfree(ti);
    Stringfree(te);
    Stringfree(cl);
    Stringfree(cm);
    Stringfree(cs);
    Stringfree(underline);
    Stringfree(reverse);
    Stringfree(blink);
    Stringfree(dim);
    Stringfree(bold);
    Stringfree(attr_off);
#endif
    Stringfree(lpprompt);
    Stringfree(moreprompt);
}
#endif
