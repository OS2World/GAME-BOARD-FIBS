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

/*********************************************************
 * Fugue command handlers, part 2                        *
 *                                                       *
 * Contents:                                             *
 * 1. Flags and numerical runtime registers              *
 * 2. Macro subsystem, including gags, triggers, hilites *
 *********************************************************/

#include <stdio.h>
#include <ctype.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "macro.h"
#include "keyboard.h"
#include "output.h"
#include "command1.h"
#include "history.h"
#include "world.h"
#include "socket.h"

#define ON (!cstrcmp(args, "on"))
#define OFF (!cstrcmp(args, "off"))

static void FDECL(do_flag,(char *args, int *flag, char *prep));
static void FDECL(do_prefix,(char *args, int *flag,
                             Stringp prefix, char *prep));

void NDECL(init_prefixes);
int  FDECL(do_file_load,(char *args));
void FDECL(read_file_commands,(TFILE *file, int oldformat));

extern char *do_board();

HANDLER (handle_background_command);
HANDLER (handle_bamf_command);
HANDLER (handle_board_commnd);
HANDLER (handle_beep_command);
HANDLER (handle_bind_command);
HANDLER (handle_borg_command);
HANDLER (handle_cat_command);
HANDLER (handle_cleardone_command);
HANDLER (handle_clearfull_command);
HANDLER (handle_def_command);
HANDLER (handle_dokey_command);
HANDLER (handle_edit_command);
HANDLER (handle_gag_command);
HANDLER (handle_gpri_command);
HANDLER (handle_hilite_command);
HANDLER (handle_hook_command);
HANDLER (handle_hpri_command);
HANDLER (handle_isize_command);
HANDLER (handle_kecho_command);
HANDLER (handle_list_command);
HANDLER (handle_load_command);
HANDLER (handle_log_command);
HANDLER (handle_login_command);
HANDLER (handle_lp_command);
HANDLER (handle_lpquote_command);
HANDLER (handle_mecho_command);
HANDLER (handle_more_command);
HANDLER (handle_nogag_command);
HANDLER (handle_nohilite_command);
HANDLER (handle_ptime_command);
HANDLER (handle_purge_command);
HANDLER (handle_qecho_command);
HANDLER (handle_quiet_command);
HANDLER (handle_quitdone_command);
HANDLER (handle_redef_command);
HANDLER (handle_save_command);
HANDLER (handle_shpause_command);
HANDLER (handle_sockmload_command);
HANDLER (handle_sub_command);
HANDLER (handle_trig_command);
HANDLER (handle_trigc_command);
HANDLER (handle_trigp_command);
HANDLER (handle_trigpc_command);
HANDLER (handle_unbind_command);
HANDLER (handle_undef_command);
HANDLER (handle_undefn_command);
HANDLER (handle_undeft_command);
HANDLER (handle_unhook_command);
HANDLER (handle_untrig_command);
HANDLER (handle_visual_command);
HANDLER (handle_watchdog_command);
HANDLER (handle_watchname_command);
HANDLER (handle_wrap_command);
HANDLER (handle_wrapspace_command);

int background = 1     ,     kecho      = FALSE ,     shpause    = FALSE ,
    bamf       = FALSE ,     log_on     = 0     ,     sockmload  = FALSE ,
    beeping    = TRUE  ,     lpflag     = FALSE ,     sub        = FALSE ,
    borg       = TRUE  ,     lpquote    = FALSE ,     login      = TRUE  ,
    clear      = FALSE ,     mecho      = FALSE ,     visual     = FALSE ,
    cleardone  = FALSE ,     more       = 0     ,     wd_enabled = FALSE ,
    concat     = FALSE ,     qecho      = FALSE ,     wn_enabled = FALSE ,
    gag        = TRUE  ,     quitdone   = FALSE ,     
    hilite     = TRUE  ,     quiet      = FALSE ,                
    hookflag   = TRUE  ,     redef      = FALSE ;
    board      = FALSE;

Stringp mprefix, kprefix, qprefix, pattern, body;
int wrapspace = 0, hpri = 0, gpri = 0;
int wnmatch = 4, wnlines = 5, wdmatch = 2, wdlines = 5;

extern int style;

void init_prefixes()
{
    Stringinit(kprefix);
    Stringinit(mprefix);
    Stringinit(qprefix);
    Stringinit(pattern);
    Stringinit(body);
}

#ifdef DMALLOC
void free_prefixes()
{
    Stringfree(kprefix);
    Stringfree(mprefix);
    Stringfree(qprefix);
    Stringfree(pattern);
    Stringfree(body);
}
#endif

/********************
 * Generic handlers *
 ********************/   

static void do_flag(args, flag, prep)
    char *args;
    int *flag;
    char *prep;
{
    if (!*args) oprintf("%% %s %sabled", prep, (*flag) ? "en" : "dis");
    else if (OFF) *flag = FALSE;
    else if (ON) *flag = TRUE;
    else oputs("% Unknown option; valid options are OFF and ON");
}

static void do_prefix(args, flag, prefix, prep)
    Stringp prefix;
    int *flag;
    char *args, *prep;
{
    if (!*args) oprintf("%% %s %s", prep, *flag ? "on" : "off");
    else if (OFF) {
        *flag = FALSE;
        Stringterm(prefix, 0);
    } else if (ON) *flag = TRUE;
    else {
        Sprintf(prefix, "%s ", args);
        *flag = TRUE;
    }
}

void read_file_commands(file, oldformat)
    TFILE *file;
    int oldformat;    /* Accept tinytalk-style world defs at top of file? */
{
    Stringp line, cmd;
    char *p;

    Stringinit(line);
    Stringinit(cmd);
    while (tfgetS(line, file) != NULL) {
        if (!line->len || line->s[0] == ';') continue;
        if (line->s[0] == '/') oldformat = FALSE;
        newline_package(line, 0);
        for (p = line->s; isspace(*p); p++);
        Stringcat(cmd, p);
        if (line->len && line->s[line->len - 1] == '\\')
            if (line->len < 2 || line->s[line->len - 2] != '%') {
                Stringterm(cmd, cmd->len - 1);
                continue;
            }
        if (!cmd->len) continue;
        if (oldformat) addworld(cmd->s); else handle_command(cmd->s, NULL);
        Stringterm(cmd, 0);
    }
    if (cmd->len)
        if (oldformat) addworld(cmd->s); else handle_command(cmd->s, NULL);
    Stringfree(line);
    Stringfree(cmd);
}

int do_file_load(args)
    char *args;
{
    register TFILE *cmdfile;

    if ((cmdfile = tfopen(args, NULL, "r")) == NULL) return FALSE;
    do_hook(H_LOAD, "%% Loading commands from %s.", "%s", cmdfile->name);
    read_file_commands(cmdfile, FALSE);
    tfclose(cmdfile);
    return TRUE;
}

/*********************************************************************
 *                           F L A G S                               *
 *********************************************************************/

/****************
 * Pure toggles *
 ****************/

void handle_quiet_command(args)      /* Turn on portal suppression. */
    char *args;
{
    do_flag(args, &quiet, "Suppression of login messages");
}

void handle_login_command(args)      /* Turn on automatic login. */
    char *args;
{
    do_flag(args, &login, "Autologin is");
}

void handle_background_command(args)
    char *args;
{
    int old = background;

    do_flag(args, &background, "Background triggers");
    if (background && !old) background_on();
}

void handle_bamf_command(args)
    char *args;
{
    if (!*args) {
        if (!bamf) oputs("% Bamfing is disabled.");
        else if (bamf == 1) oputs("% Unter bamfing is enabled.");
        else if (bamf == 2) oputs("% Old-style bamfing is enabled.");
    } else if (OFF) bamf = 0;
    else if ((ON) || !cstrncmp(args, "unter", 5)) bamf = 1;
    else if (!cstrncmp(args, "old", 3)) bamf = 2;
    else oputs("% Unknown option; valid options are OFF, ON and OLD.");
}

void handle_borg_command(args)
    char *args;
{
    do_flag(args, &borg, "Cyborg triggers");
}

void handle_redef_command(args)
    char *args;
{
    do_flag(args, &redef, "Redefinition");
}

void handle_visual_command(args)
    char *args;
{
    int old_state = visual;
    extern int can_have_visual;

    if (can_have_visual) {
        do_flag(args, &visual, "Visual mode");
        if (old_state != visual) {
            if (!visual) fix_screen();
            setup_screen();
        }
    } else {
        oputs("% Visual mode not supported.");
    }
}

void handle_clearfull_command(args)
    char *args;
{
    do_flag(args, &clear, "Clearing of input window upon filling");
}

void handle_sub_command(args)
    char *args;
{
    if (!*args) {
        if (!sub) oputs("% No substitution on normal input.");
        else if (sub == 1) oputs("% Newline substitution on.");
        else if (sub == 2)
            oputs("% Full reentrance substitution on normal input.");
    } else if (OFF) sub = 0;
    else if (ON) sub = 1;
    else if (!cstrncmp(args, "full", 4)) sub = 2;
    else oputs("% Unknown option; valid options are OFF, ON and FULL.");
}

void handle_shpause_command(args)
    char *args;
{
    do_flag(args, &shpause, "Pause before returning from /sh command");
}

void handle_cleardone_command(args)
    char *args;
{
    do_flag(args, &cleardone, "Clearing on return");
}

void handle_sockmload_command(args)
    char *args;
{
    do_flag(args, &sockmload, "Load macro file on socket moves");
}

void handle_lp_command(args)
    char *args;
{
    do_flag(args, &lpflag, "Displaying of partial lines");
}

void handle_lpquote_command(args)
    char *args;
{
    do_flag(args, &lpquote, "Waiting for *^H for quoting");
}

void handle_more_command(args)
    char *args;
{
    if (!*args) oprintf("%% Paging %sabled", more ? "en" : "dis");
    else if (OFF) { clear_more(0); more = 0; }
    else if (ON) { more = 1; reset_outcount(); }
    else oputs("% Unknown option; valid options are OFF and ON");
}

void handle_quitdone_command(args)
    char *args;
{
    do_flag(args, &quitdone, "Quitting upon disconnection from last socket");
}

/**************************
 * Toggles with arguments *
 **************************/

void handle_wrap_command(args)
    char *args;
{
    static int wordwrap = TRUE;
    int width;

    if (*args) {
        if (OFF) wordwrap = 0;
        else if (ON) wordwrap = 1;
        else {
            width = numarg(&args);
            if (args && !*args) enable_wrap(width);
            else oputs("% Invalid argument.");
        }
    }
    oputs(wordwrap ? "% Wordwrap is enabled." : "% Wordwrap is disabled.");
    if (wordwrap) enable_wrap(0);
    else disable_wrap();
}

void handle_board_command(args)      /* use additional window */
    char *args;
{
    int oldboard=board;
    int oldstyle=style;
    int newstyle=0;

    if (*args) {
      if (OFF) board=0;
      else if (ON) board=1;
      else {
          newstyle = numarg(&args);
          if (args && !*args && (newstyle==1 ||  newstyle==2 || newstyle==4)) style=newstyle;
          else oputs("% Invalid argument.");
      }
    }
    else 
      oputs(board ? "% Backgammon window used." : "% Backgammon window not used.");
    if (style != oldstyle) {
      do_board(NULL);
      setup_screen();
    }
    else if (board!=oldboard)
      setup_screen();
}

/* Actually a miscellaneous routine, but it looks a lot like a flag */

void handle_beep_command(args)
    char *args;
{
    int beep = 3;

    if (ON) beeping = TRUE;
    else if (OFF) beeping = FALSE;
    else if (isdigit(*args)) beep = atoi(args);

    if (!beeping) return;
    while (beep--) putch('\007');
}

void handle_log_command(args)    /* Enable logging. */
    char *args;
{
    do_log(args);
}

void handle_kecho_command(args)
    char *args;
{
    do_prefix(args, &kecho, kprefix, "Echoing of input is");
}

void handle_mecho_command(args)
    char *args;
{
    do_prefix(args, &mecho, mprefix, "Echoing of macros is");
}

void handle_qecho_command(args)
    char *args;
{
    do_prefix(args, &qecho, qprefix, "Echoing of quote text is");
}

void handle_cat_command(args)
    char *args;
{
    concat = (*args == '%') ? 2 : 1;
}

void do_watch(args, name, wlines, wmatch, flag)
    char *args, *name;
    int *wlines, *wmatch, *flag;
{
    int lines = 0, match = 0;
    char *ptr;

    if (OFF) {
        *flag = FALSE;
        oprintf("%% %s disabled.", name);
        return;
    }
    ptr = args;
    if (ON) for (ptr += 2; isspace(*ptr); ptr++);
    if (match = numarg(&ptr)) *wmatch = match;
    if (ptr && (lines = numarg(&ptr))) *wlines = lines;
    *flag = TRUE;
    oprintf("%% %s enabled, searching for %d out of %d lines",
      name, *wmatch, *wlines);
}

void handle_watchdog_command(args)
    char *args;
{
    do_watch(args, "Watchdog", &wdlines, &wdmatch, &wd_enabled);
}

void handle_watchname_command(args)
    char *args;
{
    do_watch(args, "Namewatch", &wnlines, &wnmatch, &wn_enabled);
}

/******************************
 * Runtime user-set variables *
 ******************************/

void handle_isize_command(args)
    char *args;
{
    extern int ilines;
    int size;

    if (*args && ((size = atoi(args)) > 0)) {
        ilines = size;
        if (visual) {
            fix_screen();
            setup_screen();
        }
    } else oprintf("%% Input window is %d lines.", ilines);
}

void handle_wrapspace_command(args)
    char *args;
{
    if (*args) {
        if (isdigit(*args)) wrapspace = atoi(args);
        else if (OFF) wrapspace = 0;
        else oputs("% Wrapspace must be followed by a number or OFF");
    }
    else oprintf("%% Wrap indents %d columns", wrapspace);
}

void handle_ptime_command(args)
    char *args;
{
    extern TIME_T process_time;

    if (*args) {
        if (isdigit(*args)) process_time = atoi(args);
        else oputs("% /ptime must be followed by a number");
    } else oprintf("%% %d seconds between quote lines", process_time);
}

void handle_hpri_command(args)
    char *args;
{
    if (!*args) oprintf("%% Hilite priority is %d", hpri);
    else hpri = atoi(args);
}

void handle_gpri_command(args)
    char *args;
{
    if (!*args) oprintf("%% Gag priority is %d", gpri);
    else gpri = atoi(args);
}


/*************************************************************************
 *                     M A C R O   S U B S Y S T E M                     *
 *************************************************************************/


/**********
 * Macros *
 **********/

void handle_def_command(args)                /* Define a macro. */
    char *args;
{
    Macro *spec;
    if (spec = macro_spec(args)) do_add(spec);
}

void handle_edit_command(args)
    char *args;
{
    Macro *spec;
    if (spec = macro_spec(args)) do_edit(spec);
}

void handle_undef_command(args)              /* Undefine a macro. */
    char *args;
{
    remove_macro(args, 0, 0);
}

void handle_undeft_command(args)
    char *args;
{
    remove_macro(args, F_ALL, 0);
}

void handle_undefn_command(args)
    char *args;
{
    remove_by_number(args);
}

void handle_purge_command(args)
    char *args;
{
    purge_macro(macro_spec(args));
}

void handle_save_command(args)
    char *args;
{
    save_macros(args);
}

void handle_list_command(args)
    char *args;
{
    list_macros(args);
}

void handle_load_command(args)
    char *args;
{                   
    do_file_load(args);
}

/*
 * Generic utility to split arguments into pattern and body.
 * Note: I can get away with this only because none of the functions
 * that use it are reentrant.  Be careful.
 */

static void split_args(args)
    char *args;
{
    extern Stringp pattern, body;
    char *place;

    place = strchr(args, '=');
    if (place == NULL) {
        Stringcpy(pattern, args);
        Stringterm(body, 0);
    } else {
        Stringncpy(pattern, args, place - args);
        Stringcpy(body, place + 1);
    }
    stripString(pattern);
    stripString(body);
}

/***********
 * Hilites *
 ***********/

void handle_hilite_command(args)
    char *args;
{
    if (!*args) {
        hilite = TRUE;
        oputs("Hilites are enabled.");
    } else {
        split_args(args);
        add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL,
            hpri, 100, F_HILITE, 0, 0));
    }
}

void handle_nohilite_command(args)
    char *args;
{
    if (!*args) {
        hilite = FALSE;
        oputs("Hilites are disabled.");
    } else {
        remove_macro(args, F_HILITE, 0);
    }
}


/********
 * Gags *
 ********/

void handle_gag_command(args)
    char *args;
{
    if (!*args) {
        gag = TRUE;
        oputs("Gags are enabled.");
    } else {
        split_args(args);
        add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL,
            gpri, 100, F_GAG, 0, 0));
    }
}

void handle_nogag_command(args)
    char *args;
{
    if (!*args) {
        gag = FALSE;
        oputs("Gags are disabled.");
    } else {
        remove_macro(args, F_GAG, 0);
    }
}


/************
 * Triggers *
 ************/

void handle_trig_command(args)
    char *args;
{
    split_args(args);
    add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL, 0, 100,
        F_NORM, 0, 0));
}

void handle_trigp_command(args)
    char *args;
{
    int pri;

    pri = numarg(&args);
    if (!args || !*args) {
        oputs("% Bad /trigp format");
        return;
    }
    split_args(args);
    add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL, pri,
        100, F_NORM, 0, 0));
}

void handle_trigc_command(args)
    char *args;
{
    int prob;

    prob = numarg(&args);
    if (!args || !*args) {
        oputs("% Bad /trigc format");
        return;
    }
    split_args(args);
    add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL, 0, prob,
        F_NORM, 0, 0));
}

void handle_trigpc_command(args)
    char *args;
{
    int pri, prob;

    pri = numarg(&args);
    if (!args || !*args) {
        oputs("% Bad /trigpc format");
        return;
    }
    prob = numarg(&args);
    if (!args || !*args) {
        oputs("% Bad /trigpc format");
        return;
    }
    split_args(args);
    add_macro(new_macro("", pattern->s, "", 0, "", body->s, NULL, pri,
        prob, F_NORM, 0, 0));
}

void handle_untrig_command(args)
    char *args;
{
    remove_macro(args, F_NORM, 0);
}


/*********
 * Hooks *
 *********/

void handle_hook_command(args)
    char *args;
{
    if (!*args) oprintf("%% Hooks %sabled", hookflag ? "en" : "dis");
    else if (OFF) hookflag = FALSE;
    else if (ON) hookflag = TRUE;
    else {
        split_args(args);
        add_hook(pattern->s, body->s);
    }
}


void handle_unhook_command(args)
    char *args;
{
    remove_macro(args, 0, 1);
}


/********
 * Keys *
 ********/

void handle_unbind_command(args)
    char *args;
{
    Macro *macro;

    if (!*args) return;
    if (macro = find_key(translate_keystring(args))) nuke_macro(macro);
    else oprintf("%% No binding for %s", args);
}

void handle_bind_command(args)
    char *args;
{
    Macro *spec;

    if (!*args) return;
    split_args(args);
    spec = new_macro("", "", translate_keystring(pattern->s), 0, "", body->s,
        NULL, 0, 0, 0, 0, 0);
    if (install_bind(spec)) add_macro(spec);
}

void handle_dokey_command(args)
    char *args;
{
    NFunc *func;

    if (func = find_efunc(args)) (*func)();
    else oprintf("%% No editing function %s", args); 
}
