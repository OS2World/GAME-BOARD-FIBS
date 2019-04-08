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

/**************************************************
 * Fugue keyboard handling.                       *
 * Handles all keyboard input and keybindings.    *
 * Also deals with tty driver.                    *
 **************************************************/

#ifdef hpux
# include <sys/ioctl.h>
# include <termio.h>
# include <bsdtty.h>
#else
# ifdef SYSVTTY
#  ifdef WINS
#   include <sys/types.h> 
#   include <sys/inet.h>
#   include <sys/stream.h>
#   include <sys/ptem.h>
#  endif
#  include <termio.h>
static int first_time = 1;           /* first time in cbreak/noecho mode? */
static struct termio old_tty_state;  /* save termio state for return to shell */
# else
#  include <sys/ioctl.h>
#  include <sgtty.h>
# endif
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "macro.h"
#include "output.h"
#include "history.h"
#include "socket.h"
#include "expand.h"

#undef CTRL
#define CTRL(x) ctrl_values[x]
#define DEFAULT_COLUMNS 80

typedef struct KeyNode {
    int children;
    union {
        struct KeyNode **child;
        Macro *macro;
    } u;
} KeyNode;

static void     NDECL(init_ctrl_table);
static Macro   *FDECL(trie_insert,(KeyNode **root, Macro *macro, char *key));
static KeyNode *FDECL(untrie_key,(KeyNode **root, char *s));

void NDECL(do_newline);
void NDECL(do_recallb);
void NDECL(do_recallf);
void NDECL(do_socketb);
void NDECL(do_socketf);
void NDECL(do_bspc);
void NDECL(do_bword);
void NDECL(do_dline);
void NDECL(do_dch);
void NDECL(do_redraw);
void NDECL(do_up);
void NDECL(do_down);
void NDECL(do_left);
void NDECL(do_right);
void NDECL(do_home);
void NDECL(do_end);
void NDECL(do_wleft);
void NDECL(do_wright);
void NDECL(do_deol);
void NDECL(do_insert);

void   NDECL(init_keyboard);
void   NDECL(use_stty);
void   NDECL(get_window_size);
Macro *FDECL(bind_key,(Macro *macro));
void   FDECL(unbind_key,(Macro *macro));
Macro *FDECL(find_key,(char *key));
char  *FDECL(translate_keystring,(char *src));
char  *FDECL(keyname,(char *key));
void   NDECL(handle_keyboard_input);
void   FDECL(handle_input_string,(char *input, int len));
void   FDECL(process_buffer,(Stringp inbuf));
void   NDECL(cbreak_noecho_mode);
void   NDECL(cooked_echo_mode);
void   NDECL((*FDECL(find_efunc,(char *name))));

extern int visual, redef;

static Stringp scratch;                 /* buffer for manipulating text */
static char ctrl_values[128];           /* map of '^X' form to ascii */
static Stringp cat_keybuf;              /* Total buffer for /cat */
static Stringp current_input;           /* unprocessed keystrokes */
static KeyNode *keytrie = NULL;         /* root of keybinding trie */

Stringp keybuf;                         /* input buffer */
int keyboard_pos;                       /* current position in buffer */

typedef struct EditFunc {
    char *name;
    NFunc *func;
} EditFunc;

static EditFunc efunc[] = {
    { "RECALLB",  do_recallb  },
    { "RECALLF",  do_recallf  },
    { "SOCKETB",  do_socketb  },
    { "SOCKETF",  do_socketf  },
    { "BSPC"   ,  do_bspc     },
    { "BWORD"  ,  do_bword    },
    { "DLINE"  ,  do_dline    },
    { "DCH"    ,  do_dch      },
    { "REFRESH",  do_refresh  },
    { "REDRAW" ,  do_redraw   },
    { "UP"     ,  do_up       },
    { "DOWN"   ,  do_down     },
    { "RIGHT"  ,  do_right    },
    { "LEFT"   ,  do_left     },
    { "HOME"   ,  do_home     },
    { "END"    ,  do_end      },
    { "WLEFT"  ,  do_wleft    },
    { "WRIGHT" ,  do_wright   },
    { "DEOL"   ,  do_deol     },
    { "INSERT" ,  do_insert   },
    { "PAGE"   ,  do_page     },
    { "HPAGE"  ,  do_hpage    },
    { "LINE"   ,  do_line     },
    { "FLUSH"  ,  do_flush    },
    { "NEWLINE",  do_newline  }
};

static int nfuncs = sizeof(efunc) / sizeof(struct EditFunc);

NFunc *find_efunc(name)
    char *name;
{
    int i;

    for (i = 0; i < nfuncs; i++)
        if (cstrcmp(name, efunc[i].name) == 0) break;
    return (i < nfuncs) ? efunc[i].func : NULL;
}

void init_keyboard()
{
    init_ctrl_table();
    keytrie = NULL;
    use_stty();
    get_window_size();
    Stringinit(scratch);
    Stringinit(keybuf);
    Stringinit(cat_keybuf);
    Stringinit(current_input);
    cbreak_noecho_mode();
    keyboard_pos = 0;
}

#ifdef DMALLOC
void free_keyboard()
{
    Stringfree(scratch);
    Stringfree(keybuf);
    Stringfree(cat_keybuf);
    Stringfree(current_input);
}
#endif

static void init_ctrl_table()
{
    int i, j;

    for (i = 0; i < 128; i++) {
        j = ucase(i) - 'A' + 1;
        ctrl_values[i] = (j < 0) ? (j + 128) : j;
    }
}

static void set_ekey(key, cmd)
    char *key, *cmd;
{
    Macro *macro;

    if (macro = find_key(key)) {
        if (redef) {
            do_hook(H_REDEF, "%% Redefined %s %s", "%s %s", "binding", key);
            nuke_macro(macro);
        } else return;
    }
    macro = new_macro("", "", key, 0, "", cmd, NULL, 0, 0, 0, 0, TRUE);
    if (bind_key(macro)) add_macro(macro);
    else nuke_macro(macro);
}

char *keyname(key)
    char *key;
{
    STATIC_BUFFER(buffer)

    Stringterm(buffer, 0);
    for ( ; *key; key++) {
        if (*key == '^' || *key == '\\') {
            Sprintf(buffer, "\200\\%c", *key);
        } else if (!isprint(*key) || iscntrl(*key)) {
            Sprintf(buffer, "\200^%c", (*key + '@') % 128);
        } else Stringadd(buffer, *key);
    }
    return buffer->s;
}

void use_stty()
{
    char bs[2], dline[2], bword[2], refresh[2];

#ifdef hpux
    struct termio te_blob;
    struct ltchars lt_blob;

    if (ioctl(0, TCGETA, &te_blob) == -1) perror("TCGETA ioctl");
    if (ioctl(0, TIOCGLTC, &lt_blob) == -1) perror("TIOCGLTC ioctl");
    *bs = te_blob.c_cc[VERASE];
    *dline = te_blob.c_cc[VKILL];
    *bword = lt_blob.t_werasc;
    *refresh = lt_blob.t_rprntc;
#else
# ifdef SYSVTTY
    struct termio tcblob;

    if (ioctl(0, TCGETA, &tcblob) == -1) perror("TCGETA ioctl");
    *bs = tcblob.c_cc[VERASE];
    *dline = tcblob.c_cc[VKILL];
#  ifdef sgi
    *bword = tcblob.c_cc[VWERASE];
    *refresh = tcblob.c_cc[VRPRNT];
#  else
    *bword = CTRL('W');
    *refresh = CTRL('R');
#  endif
# else
    struct sgttyb sg_blob;
    struct ltchars lt_blob;

    if (ioctl(0, TIOCGETP, &sg_blob) == -1) perror("TIOCGETP ioctl");
    if (ioctl(0, TIOCGLTC, &lt_blob) == -1) perror("TIOCGLTC ioctl");
    *bs = sg_blob.sg_erase;
    *dline = sg_blob.sg_kill;
    *bword = lt_blob.t_werasc;
    *refresh = lt_blob.t_rprntc;
# endif /* SYSVTTY */
#endif /* hpux */
    bs[1] = dline[1] = bword[1] = refresh[1] = '\0';
    set_ekey(isascii(*bs) && *bs ? bs : "^H", "/DOKEY BSPC");
    set_ekey(isascii(*bword) && *bword ? bword : "^W", "/DOKEY BWORD");
    set_ekey(isascii(*dline) && *dline ? dline : "^X", "/DOKEY DLINE");
    set_ekey(isascii(*refresh) && *refresh ? refresh : "^R", "/DOKEY REFRESH");
}

void get_window_size()
{
#ifndef WINS
# ifdef TIOCGWINSZ
    extern int columns, lines, current_wrap_column;
    struct winsize size;
    int ocol, oline;

    ocol = columns;
    oline = lines;
    if (ioctl(0, TIOCGWINSZ, &size) == -1) {
        perror("TIOCGWINSZ ioctl");
        return;
    }
    if (!size.ws_col || !size.ws_row) return;
    columns = size.ws_col;
    lines = size.ws_row;
    if (columns <= 20) columns = DEFAULT_COLUMNS;
    if (columns != ocol || lines != oline) do_redraw();
    if (current_wrap_column) current_wrap_column = columns - 1;
    do_hook(H_RESIZE, NULL, "%d %d", columns, lines);
# endif
#endif
}

Macro *find_key(key)
    char *key;
{
    KeyNode *n;

    for (n = keytrie; n && n->children && *key; n = n->u.child[*key++]);
    return (n && !n->children && !*key) ? n->u.macro : NULL;
}

static Macro *trie_insert(root, macro, key)
    KeyNode **root;
    Macro *macro;
    char *key;
{
    int i;

    if (!*root) {
        *root = (KeyNode *) MALLOC(sizeof(KeyNode));
        if (*key) {
            (*root)->children = 1;
            (*root)->u.child = (KeyNode **) MALLOC(128 * sizeof(KeyNode *));
            for (i = 0; i < 128; i++) (*root)->u.child[i] = NULL;
            return trie_insert(&(*root)->u.child[*key], macro, key + 1);
        } else {
            (*root)->children = 0;
            return (*root)->u.macro = macro;
        }
    } else {
        if (*key) {
            if ((*root)->children) {
                if (!(*root)->u.child[*key]) (*root)->children++;
                return trie_insert(&(*root)->u.child[*key], macro, key + 1);
            } else {
                oprintf("%% %s is prefixed by an existing sequence.",
                    keyname(macro->bind));
                return NULL;
            }
        } else {
            if ((*root)->children) {
                oprintf("%% %s is prefix of an existing sequence.",
                  keyname(macro->bind));
                return NULL;
            } else if (redef) {
                return (*root)->u.macro = macro;
            } else {
                oprintf("%% Binding %s already exists.", keyname(macro->bind));
                return NULL;
            }
        }
    }
}

Macro *bind_key(macro)
    Macro *macro;
{
    return trie_insert(&keytrie, macro, macro->bind);
}

static KeyNode *untrie_key(root, s)
    KeyNode **root;
    char *s;
{
    if (!*s) {
        FREE(*root);
        return *root = NULL;
    }
    if (untrie_key(&((*root)->u.child[*s]), s + 1)) return *root;
    if (--(*root)->children) return *root;
    FREE((*root)->u.child);
    FREE(*root);
    return *root = NULL;
}

void unbind_key(macro)
    Macro *macro;
{
    untrie_key(&keytrie, macro->bind);
}

/* translate_keystring returns ptr to static area.  Copy if needed. */
char *translate_keystring(src)
    char *src;
{
    STATIC_BUFFER(dest);

    Stringterm(dest, 0);
    while (*src) {
        if (*src == '^') {
            src++;
            if (*src) {
                Stringadd(dest, CTRL(*src));
                src++;
            } else Stringadd(dest, '^');
        } else if (*src == '\\' && isdigit(*++src)) {
            Stringadd(dest, (char)atoi(src) % 128);
            while (isdigit(*++src));
        } else Stringadd(dest, *src++);
    }
    return dest->s;
}

void handle_keyboard_input()
{
    char *s, buf[64];
    int i, count;
    Macro *macro;
    static KeyNode *n;
    static int key_start = 0, input_start = 0, place = 0;
    STATIC_BUFFER(dest)

    if ((count = read(0, buf, 64)) == -1) {
        if (errno == EINTR) return;
        else die("%% Couldn't read keyboard.\n");
    }
    if (!count) return;
    for (i = 0; i < count; i++) {
        buf[i] &= 0x7f;
#ifdef SYSVTTY
        if (buf[i] == '\r') Stringadd(current_input, '\n');
        else
#endif
        if (buf[i] != '\0') Stringadd(current_input, buf[i]);
    }

    s = current_input->s;
    if (!n) n = keytrie;
    while (s[place]) {
        while (s[place] && n && n->children) n = n->u.child[s[place++]];
        if (!n || !keytrie->children) {                   /* No match      */
            place = ++key_start;                             /* try suffix */
            n = keytrie;
        } else if (n->children) {                         /* Partial match */
            /* do nothing */
        } else {                                          /* Total match   */
            macro = n->u.macro;
            handle_input_string(s + input_start, key_start - input_start);
            key_start = input_start = place;
            reset_outcount();
            if (macro->func) (*macro->func)();
            else {
                Stringinit(dest);
                process_macro(macro->body, "", dest, TRUE);
                Stringfree(dest);
            }
            n = keytrie;
        }
    }

    handle_input_string(s + input_start, key_start - input_start);
    if (!s[key_start]) {
        Stringterm(current_input, 0);
        place = key_start = 0;
    }
    input_start = key_start;
}

void handle_input_string(input, len)
    char *input;
    int len;
{
    extern int input_cursor, insert;
    int i, j;
    char save;

    if (len == 0) return;
    if (!input_cursor) ipos();
    for (i = j = 0; i < len; i++) {
        if (isprint(input[i])) input[j++] = input[i];
    }
    save = input[len = j];
    input[len] = '\0';
    iputs(input);
    input[len] = save;

    if (keyboard_pos == keybuf->len) {                    /* add to end */
        Stringncat(keybuf, input, len);
    } else if (insert) {                                  /* insert in middle */
        Stringcpy(scratch, keybuf->s + keyboard_pos);
        Stringterm(keybuf, keyboard_pos);
        Stringncat(keybuf, input, len);
        SStringcat(keybuf, scratch);
    } else if (keyboard_pos + len < keybuf->len) {        /* overwrite */
        for (i = 0, j = keyboard_pos; i < len; keybuf->s[j++] = input[i++]);
    } else {                                              /* write past end */
        Stringterm(keybuf, keyboard_pos);
        Stringncat(keybuf, input, len);
    }                      
    keyboard_pos += len;
}

void do_newline()
{
    clear_refresh_pending();
    reset_outcount();
    inewline();
    SStringcpy(scratch, keybuf);
    Stringterm(keybuf, keyboard_pos = 0);
    process_buffer(scratch);
    if (visual) ipos();
}

void do_recallb()
{
    recall_input(keybuf, -1);
    keyboard_pos = keybuf->len;
    do_replace();
}

void do_recallf()
{
    recall_input(keybuf, 1);
    keyboard_pos = keybuf->len;
    do_replace();
}

void do_socketb()
{
    movesock(-1);
}

void do_socketf()
{
    movesock(1);
}

void do_bspc()
{
    if (!keyboard_pos) return;
    Stringcpy(scratch, keybuf->s + keyboard_pos);
    Stringterm(keybuf, --keyboard_pos);
    SStringcat(keybuf, scratch);
    ibs();
}

void do_bword()
{
    int place;

    if (!keyboard_pos) return;
    place = keyboard_pos - 1;
    while(isspace(keybuf->s[place])) place--;
    while(!isspace(keybuf->s[place])) place--;
    place++;
    if (place < 0) place = 0;
    Stringcpy(scratch, keybuf->s + keyboard_pos);
    Stringterm(keybuf, place);
    SStringcat(keybuf, scratch);
    ibackword(place);
    keyboard_pos = place;
}

void do_dline()
{
    Stringterm(keybuf, keyboard_pos = 0);
    do_replace();
}

void do_dch()
{
    if (keyboard_pos == keybuf->len) return;
    newpos(keyboard_pos + 1);
    do_bspc();
}

void do_redraw()
{
    extern int screen_setup;

    if (!screen_setup || !visual) return;
    clr();
    setup_screen();
    do_replace();
}

void do_up()
{
    newpos(keyboard_pos - getwrap());
}

void do_down()
{
    newpos(keyboard_pos + getwrap());
}

void do_left()
{
    newpos(keyboard_pos - 1);
}

void do_right()
{
    newpos(keyboard_pos + 1);
}

void do_home()
{
    newpos(0);
}

void do_end()
{
    newpos(keybuf->len);
}

void do_wleft()
{
    int place;

    place = keyboard_pos - 1;
    while(isspace(keybuf->s[place])) place--;
    while(!isspace(keybuf->s[place])) if (--place < 0) break;
    place++;
    newpos(place);
}

void do_wright()
{
    int place;

    place = keyboard_pos;
    while (!isspace(keybuf->s[place]))
        if (++place > keybuf->len) break;
    while (isspace(keybuf->s[place])) place++;
    newpos(place);
}

void do_deol()
{
    dEOL();
    Stringterm(keybuf, keyboard_pos);
}

void do_insert()
{
    toggle_insert();
}

void process_buffer(inbuf)
    Stringp inbuf;
{
    extern Stringp kprefix;
    extern int kecho, concat, sub;
    char *temp, *ptr, code;
    STATIC_BUFFER(buffer)

    Stringterm(buffer, 0);
    if (concat) {
        if (inbuf->s[0] == '.' && !inbuf->s[1]) {
            SStringcpy(inbuf, cat_keybuf);
            Stringterm(cat_keybuf, 0);
            concat = 0;
        } else {
            SStringcat(cat_keybuf, inbuf);
            if (concat == 2) Stringcat(cat_keybuf, "%\\");
            return;
        }
    }

    if (kecho) oprintf("%S%S", kprefix, inbuf);
    else if (!inbuf->len && visual) oputs("");

#ifdef SNARF_BLANKS
    if (!inbuf->len) return;
#endif

    temp = inbuf->s;
    if (!sub || *temp == '^') SStringcpy(buffer, inbuf);
    else if (sub == 2) {
        process_macro(inbuf->s, "", buffer, 1);
        if (inbuf->len) record_input(inbuf->s);
        return;
    } else if (*temp == '/') SStringcpy(buffer, inbuf);
    else while (*temp) {
        if (*temp == '%' && sub) {
            if (*++temp == '%')
                while (*temp == '%') Stringadd(buffer, *temp++);
            else if (*temp == '\\' || *temp == ';') {
                Stringadd(buffer, '\n');
                temp++;
            } else if (*temp == 'e' || *temp == 'E') {
                Stringadd(buffer, '\033');
                temp++;
            } else Stringadd(buffer, '%');
        } else if (*temp == '\\') {
            temp++;
            if (isdigit(*temp)) {
                code = (char)atoi(temp);
                Stringadd(buffer, code);
                while (isdigit(*++temp));
            } else Stringadd(buffer, '\\');
        } else {
            for (ptr = temp++; *temp && *temp != '%' && *temp != '\\'; temp++);
            Stringncat(buffer, ptr, temp - ptr);
        }
    }

    if (buffer->s[0] == '^') {
        newline_package(buffer, 0);
        history_sub(buffer->s + 1);
    } else {
        check_command(TRUE, buffer);
        if (inbuf->len) record_input(inbuf->s);
    }
}

void cbreak_noecho_mode()
{
#ifdef hpux
    struct termio blob;

    if (ioctl(0, TCGETA, &blob) == -1) perror("TCGETA ioctl");
    blob.c_lflag &= ~(ECHO | ECHOE | ICANON);
    blob.c_cc[VMIN] = 0;
    blob.c_cc[VTIME] = 0;
    if (ioctl(0, TCSETAF, &blob) == -1) perror("TCSETAF ioctl");
#else
# ifdef SYSVTTY
    struct termio blob;

    first_time = FALSE;
    if (ioctl(0, TCGETA, &blob) == -1) perror("TCGETA ioctl");
    bcopy(&blob, &old_tty_state, sizeof(struct termio));
    blob.c_iflag = IGNBRK | IGNPAR | ICRNL;
    blob.c_oflag = OPOST | ONLCR;
    blob.c_lflag = ISIG;
    blob.c_cc[VMIN] = 0;
    blob.c_cc[VTIME] = 0;
    if (ioctl(0, TCSETAF, &blob) == -1) perror("TCSETAF ioctl");
# else
    struct sgttyb blob;

    if (ioctl(0, TIOCGETP, &blob) == -1) perror("TIOCGETP ioctl");
    blob.sg_flags |= CBREAK;
    blob.sg_flags &= ~ECHO;
    if (ioctl(0, TIOCSETP, &blob) == -1) perror("TIOCSETP ioctl");
# endif
#endif
}

void cooked_echo_mode()
{
#ifdef hpux
    struct termio blob;

    if (ioctl(0, TCGETA, &blob) == -1) perror("TCGETA ioctl");
    blob.c_lflag |= ECHO | ECHOE | ICANON;
    if (ioctl(0, TCSETAF, &blob) == -1) perror("TCSETAF ioctl");
#else
# ifdef SYSVTTY
    if (!first_time)
        if (ioctl(0, TCSETA, &old_tty_state) == -1) perror("TCSETA ioctl");
# else
    struct sgttyb blob;

    if (ioctl(0, TIOCGETP, &blob) == -1) perror("TIOCGETP ioctl");
    blob.sg_flags &= ~CBREAK;
    blob.sg_flags |= ECHO;
    if (ioctl(0, TIOCSETP, &blob) == -1) perror("TIOCSETP ioctl");
# endif
#endif
}
