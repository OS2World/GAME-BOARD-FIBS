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

#define WORLD_TEMP     001
#define WORLD_CONN     002
#define WORLD_ACTIVE   004

typedef struct World {         /* World structure */
    int flags;
    struct World *next;
    char *name, *character, *pass, *address, *port, *mfile;
    struct Sock *socket;       /* open socket, if any */
    struct History history[1]; /* history and logging info */
    struct Queue queue[1];     /* buffer for unprocessed lines */
} World;


extern void   FDECL(addworld,(char *args));
extern World *FDECL(new_world,(char *name, char *character, char *pass,
                    char *address, char *port, char *mfile));
extern void   FDECL(remove_world,(char *args));
extern void   FDECL(purge_world,(char *args));
extern void   FDECL(write_worlds,(char *args));
extern void   FDECL(list_worlds,(int full, char *pattern, TFILE *fp));
extern void   FDECL(free_world,(World *w));
extern void   FDECL(nuke_world,(World *w));
extern World *NDECL(get_default_world);
extern World *NDECL(get_world_header);
extern World *FDECL(find_world,(char *name));
extern void   FDECL(flush_world,(World *w));
