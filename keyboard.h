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

extern NFunc        *FDECL(find_efunc,(char *name));
extern void          NDECL(init_keyboard);
extern void          NDECL(use_stty);
extern void          NDECL(get_window_size);
extern struct Macro *FDECL(bind_key,(struct Macro *macro));
extern void          FDECL(unbind_key,(struct Macro *macro));
extern struct Macro *FDECL(find_key,(char *key));
extern char         *FDECL(translate_keystring,(char *src));
extern char         *FDECL(keyname,(char *key));
extern void          NDECL(handle_keyboard_input);
extern void          FDECL(handle_input_string,(char *input, int len));
extern void          FDECL(process_buffer,(Stringp inbuf));
extern void          NDECL(cbreak_noecho_mode);
extern void          NDECL(cooked_echo_mode);
