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

typedef struct History {       /* circular list of Alines, and logfile */
    Aline **alines;
    int size, maxsize, pos, index, num;
    TFILE *logfile;
} History;

typedef struct Textnode {      /* node in Queue */
    Aline *aline;
    struct Textnode *next;
} Textnode;

typedef struct Queue {         /* FIFO for buffering lines */
    Textnode *head, *tail;
} Queue;


extern void   NDECL(init_histories);
extern void   FDECL(free_history,(History *q));
extern int    FDECL(history_full,(History *q));
extern void   FDECL(record_world,(History *q, Aline *aline));
extern void   FDECL(record_local,(Aline *aline));
extern void   FDECL(record_input,(char *line));
extern int    FDECL(recall_history,(char *args, Queue *buf));
extern void   FDECL(recall_input,(Stringp str, int dir));
extern int    FDECL(is_suppressed,(History *q, char *line));
extern void   FDECL(history_sub,(char *pattern));
extern void   FDECL(init_queue,(Queue *q));
extern void   FDECL(enqueue,(Queue *q, Aline *aline));
extern Aline *FDECL(dequeue,(Queue *q));
extern void   FDECL(free_queue,(Queue *q));
extern void   FDECL(do_log,(char *args));
