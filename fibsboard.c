/*
 * fibsboard.c
 * Copyright (c)1993 by Andreas Schneider. All rights reserved.
 *
 * This routine may be used in clients for my FIBS backgammon server
 * as long as the source of such a program is available free of charge.
 * NO WARRANTY:
 * Those routines are not supposed to do anything useful!
 *
 * History:
 *
 * V 1.000 - May 24, 1993
 * First release. Quick hack based on the routines of the server.
 *
 * V 1.001 - May 24, 1993
 * Added boardstyle 4
 *
 * V 1.002 - May 24, 1993
 * All lines have fixed length now.
 *
 * V 1.003 - May 24, 1993
 * Added rotine board_diff
 * 
 * V 1.004 - May 25, 1993
 * Fixed a couple of bugs
 *
 */

/* #defining DEBUG produces lots of output */
/*#define DEBUG*/

/*
 * if STAND_ALONE is #defined this file compiles into a filter that
 * converts rawboard lines into ASCII boards and leaves other lines
 * unchanged (might be handy for use with pipes)
 */

/* #defining BLANK_LINE prints an extra blank line below the board */
/*#define BLANK_LINE*/

/*#define STAND_ALONE*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>              /* could be omitted if DEBUG and STAND_ALONE are not #defined */

#ifndef Export
#define Export
#endif

#ifndef Prototype
#define Prototype extern
#endif

#ifndef Local
#define Local static
#endif

#ifndef min
#define min(A,B)  ((A) < (B)? (A) : (B))
#endif

#ifndef YES
#define YES 1
#endif

#ifndef NO
#define NO 0
#endif

/* For use with telnet SIMPLE_NEW_LINES should not be #defined.
 * The telnet protocol needs both \r and \n to be sent.
 */

#define SIMPLE_NEW_LINES

#ifdef SIMPLE_NEW_LINES
#define NEW_LINE "\n"
#else
#define NEW_LINE "\r\n"
#endif

#ifndef LINE_LIMIT
#define LINE_LIMIT 79
#endif

#define UPPER_HALF 1
#define LOWER_HALF 0
#define COLOR_O 1
#define COLOR_X -1
#define UNLIMITED 9999

/* nifty #define's for easy debugging */

#ifdef DEBUG
#ifdef __STDC__
#define get_int(var) \
  do { \
     if (!get_number(& var )) \
     { \
       fputs("Shriek! " #var "not read\n",stderr); \
       return NO; \
     } \
     else \
       fprintf(stderr,#var " set to %i\n", var ); \
  } while (0)
#endif /* __STDC__ */
#endif

#ifndef get_int
#define get_int(var) \
  do { \
    if (!get_number(& var )) \
      return NO; \
  } while (0)
#endif

/* Prototypes: */

#ifdef __STDC__

Prototype int do_board (char *);
Prototype char *board_diff (int *, int *);
Local void fill_line (char *, int);
Local void top_lines (void);
Local void bottom_lines (void);
Local void middle_line (void);
Local void line_5_pos (int);
Local void line_1to4_pos (int, int);
Local void six_pos_up (int, int);
Local void six_pos_down (int, int);
Local void draw_bar (int, int);
Local void one_line (int, int, int, int);
Local int decode_board (void);
Local int get_number (int *);
Local void init_board (void);

#endif

/* Variables that can be read from extern routines: */
/* The style variable can be set to 1 or 2 by extern routines for different style */
Export int style = 4;

/* the board_buffer contains the board in ASCII representation */
Export char board_buffer[2048];

/* Local variables set by scanning rawboard output */
Local char player_name[128];
Local char opponent_name[128];
Local int match_length;
Local int player_got;
Local int opponent_got;
Local int board[26];
Local int turn;
Local int die1;
Local int die2;
Local int opponent_die1;
Local int opponent_die2;
Local int cube;
Local int player_may_double;
Local int opponent_may_double;
Local int was_doubled;
Local int color;
Local int direction;
Local int home;
Local int bar;
Local int player_on_home;
Local int opponent_on_home;
Local int player_on_bar;
Local int opponent_on_bar;
Local int can_move;
Local int forced_move;
Local int did_crawford;
Local int max_redoubles;

/* Other variables used by routines in this file */
Local char buf[128];
Local char cur_line[256];
Local char spaces[128];
Local char old_board_buffer[2048];
Local char *old_board_ptr;
Local int old_board_line;
Local int old_board_col;
Local char difference[2048];

/* appends lots of spaces to string and limits to LINE_LIMIT chars.
 * if newline == YES then NEW_LINE is added. After filling the line
 * it is appended to board_buffer.
 */
void
fill_line (string, newline)
     char *string;
     int newline;
{
  strcat (string, spaces);
  string[LINE_LIMIT - 1] = 0;
  if (newline)
    strcat (string, NEW_LINE);
  strcat (board_buffer, string);
}

void
top_lines ()
{
  if (color == COLOR_O)
  {
    switch (style)
    {
      default:                 /* style 4 is default */
      case 1:
      case 4:
        strcpy (cur_line, "   +13-14-15-16-17-18-------19-20-21-22-23-24-+ X: ");
        break;
      case 2:
        strcpy (cur_line, "    13 14 15 16 17 18       19 20 21 22 23 24");
        fill_line (cur_line, YES);
        strcpy (cur_line, "   +------------------------------------------+ X: ");
        break;
    }
  }
  else
  {
    switch (style)
    {
      default:
      case 1:
      case 4:
        strcpy (cur_line, "   +-1--2--3--4--5--6--------7--8--9-10-11-12-+ O: ");
        break;
      case 2:
        strcpy (cur_line, "     1  2  3  4  5  6        7  8  9 10 11 12");
        fill_line (cur_line, YES);
        strcpy (cur_line, "   +------------------------------------------+ O: ");
        break;
    }
  }
  strcpy (buf, opponent_name);
  /* limit name to 15 chars */
  buf[15] = 0;
  strcat (cur_line, buf);
  sprintf (buf, " - score: %i", opponent_got);
  strcat (cur_line, buf);
  fill_line (cur_line, YES);
}

void
bottom_lines ()
{
  /* draw bottom of the board */
  switch (style)
  {
    default:
    case 1:
    case 4:
      if (color == COLOR_O)
        strcpy (cur_line, "   +12-11-10--9--8--7--------6--5--4--3--2--1-+ O: ");
      else
        strcpy (cur_line, "   +24-23-22-21-20-19-------18-17-16-15-14-13-+ X: ");
      break;
    case 2:
      strcpy (cur_line, "   +------------------------------------------+ ");
      strcat (cur_line, (color == COLOR_O) ? "O: " : "X: ");
      break;
  }
  /* now inform about score */
  strcpy (buf, player_name);
  /* limit name to 15 chars */
  buf[15] = 0;
  strcat (cur_line, buf);
  sprintf (buf, " - score: %i", player_got);
  strcat (cur_line, buf);
  /* for style 2 we also need the numbers below the board */
  if (style == 2)
  {
    fill_line (cur_line, YES);
    if (color == COLOR_O)
      strcpy (cur_line, "    12 11 10  9  8  7        6  5  4  3  2  1");
    else
      strcpy (cur_line, "    24 23 22 21 20 19       18 17 16 15 14 13");
  }
  /* this routine does not append a NEW_LINE */
  fill_line (cur_line, NO);
}

void
middle_line ()
{
  /* that's easy :-) */
  if (color == COLOR_O)
  {
    strcpy (cur_line, "  v|                  |BAR|                   |");
  }
  else
  {
    strcpy (cur_line, "   |                  |BAR|                   |v");
  }
  if (match_length == UNLIMITED)
    sprintf (buf, "    unlimited match");
  else
    sprintf (buf, "    %i-point match", match_length);
  strcat (cur_line, buf);
  if (player_may_double && opponent_may_double)
  {
    sprintf (buf, " - Cube: %i", cube);
    strcat (cur_line, buf);
  }
  fill_line (cur_line, YES);
}

/* If there are more than 5 pieces on a position
 * this routine will write the number of pieces on
 * this position instead of X or O
 */
void
line_5_pos (pos)
     int pos;
{
  if (board[pos] == 5)
  {
    strcat (cur_line, " O ");
  }
  else if (board[pos] == -5)
  {
    strcat (cur_line, " X ");
  }
  else if (abs (board[pos]) > 5)
  {
    strcpy (buf, "");
    sprintf (buf, "%2i ", abs (board[pos]));
    strcat (cur_line, buf);
  }
  else
    /* less than 5 of any kind */
  {
    strcat (cur_line, "   ");
  }
}

/* This routine knows how to write 'X' or 'O' or ' ' */

void
line_1to4_pos (line, pos)
     int line;
     int pos;
{
  if (board[pos] >= line)
  {
    strcat (cur_line, " O ");
  }
  else if (board[pos] <= -line)
  {
    strcat (cur_line, " X ");
  }
  else
  {
    strcat (cur_line, "   ");
  }
}

/* This routine writes half a line of the board with increasing numbers */

void
six_pos_up (line, from_pos)
     int line;
     int from_pos;
{
  int pos;

  for (pos = from_pos; pos < from_pos + 6; pos++)
  {
    if (line == 5)
    {
      line_5_pos (pos);
    }
    else
    {
      line_1to4_pos (line, pos);
    }
  }
}

/* similar routine with decreasing numbers */

void
six_pos_down (line, from_pos)
     int line;
     int from_pos;
{
  int pos;

  for (pos = from_pos; pos > from_pos - 6; pos--)
  {
    if (line == 5)
    {
      line_5_pos (pos);
    }
    else
    {
      line_1to4_pos (line, pos);
    }
  }
}

/* write the center of the board */

void
draw_bar (line, half)
     int line;
     int half;
{
  if (line <= 4 && half == UPPER_HALF)
  {
    /* draw opponents pieces on bar */
    if (opponent_on_bar < line)
    {
      /* no need to show a piece on that line */
      strcat (cur_line, "|   | ");
    }
    else
    {
      /* there are enough pieces on the bar */
      if (color == COLOR_O)
        /* opponent has X */
        strcat (cur_line, "| X | ");
      else
        /* opponent has O */
        strcat (cur_line, "| O | ");
    }
  }
  else if (line == 5 && half == UPPER_HALF)
  {
    if (opponent_on_bar <= 4)
      strcat (cur_line, "|   | ");
    else if (opponent_on_bar == 5)
    {
      if (color == COLOR_O)
        /* opponent has X */
        strcat (cur_line, "| X | ");
      else
        /* opponent has O */
        strcat (cur_line, "| O | ");
    }
    else
    {
      /* opponent has more than 5 pieces on bar - write the number */
      strcpy (buf, "");
      sprintf (buf, "|%2i | ", abs (opponent_on_bar));
      strcat (cur_line, buf);
    }
  }
  else if (line == 5 && half == LOWER_HALF)
  {
    if (player_on_bar <= 4)
      strcat (cur_line, "|   | ");
    else if (player_on_bar == 5)
    {
      if (color == COLOR_O)
        strcat (cur_line, "| O | ");
      else
        strcat (cur_line, "| X | ");
    }
    else
    {
      /* user has more than 5 pieces on bar */
      strcpy (buf, "");
      sprintf (buf, "|%2i | ", abs (player_on_bar));
      strcat (cur_line, buf);
    }
  }
  else
  {
    /* line<4 && half==LOWER_HALF: draw own pieces on bar */
    if (player_on_bar < line)
    {
      /* not enough pieces on bar */
      strcat (cur_line, "|   | ");
    }
    else
    {
      /* enough pieces - draw one */
      if (color == COLOR_X)
        strcat (cur_line, "| X | ");
      else
        strcat (cur_line, "| O | ");
    }
  }
}

/* write one line of the board */

void
one_line (line, from_pos, to_pos, half)
     int line;
     int from_pos;
     int to_pos;
     int half;
{
  strcpy (cur_line, "   |");
  if (from_pos < to_pos)
  {
    /* count upwards - first six first */
    six_pos_up (line, from_pos);
    /* now do the bar */
    draw_bar (line, half);
    /* now the next six */
    six_pos_up (line, from_pos + 6);
  }
  else
  {
    /* count downwards  */
    six_pos_down (line, from_pos);
    /* the bar: */
    draw_bar (line, half);
    /* the rest */
    six_pos_down (line, from_pos - 6);
  }
  /* line must end. That's how: */
  if (line == 2 && style == 4)
  {
    sprintf (buf, "|    BAR: %i  OFF: %i",
             (half == UPPER_HALF) ? opponent_on_bar : player_on_bar,
             (half == UPPER_HALF) ? opponent_on_home : player_on_home);
    strcat (cur_line, buf);
  }
  else if (line == 3 && style == 4)
  {
    if (half == UPPER_HALF && opponent_may_double && !player_may_double)
    {
      sprintf (buf, "|    Cube: %i", cube);
      strcat (cur_line, buf);
    }
    else if (half == LOWER_HALF && player_may_double && !opponent_may_double)
    {
      sprintf (buf, "|    Cube: %i", cube);
      strcat (cur_line, buf);
    }
    else
      strcat (cur_line, "|");
  }
  else if (line == 4 && style == 4)
  {
    if (half == UPPER_HALF && die1 == 0 && opponent_die1 != 0)
    {
      sprintf (buf, "|    rolled %i %i", opponent_die1, opponent_die2);
      strcat (cur_line, buf);
    }
    else if (half == LOWER_HALF && die1 != 0)
    {
      sprintf (buf, "|    rolled %i %i", die1, die2);
      strcat (cur_line, buf);
    }
    else
      strcat (cur_line, "|");
  }
  else if (line == 5 && half == LOWER_HALF && match_length == UNLIMITED)
  {
    /* here goes information about possible redoubles */
    strcat (cur_line, "|    ");
    switch (max_redoubles)
    {
      case 0:
        strcpy (buf, "No redoubles");
        break;
      case 1:
        strcpy (buf, "1 redouble");
        break;
      case UNLIMITED:
        strcpy (buf, "Unlimited redoubles");
        break;
      default:
        sprintf (buf, "%i redoubles", max_redoubles);
        break;
    }
    strcat (cur_line, buf);
  }
  else
    strcat (cur_line, "|");
  fill_line (cur_line, YES);
}

/* get one more integer number from rawboard string
 * The routine returns NO (0) if no more numbers available
 */

int
get_number (int_var)
     int *int_var;
{
  char *token;

  token = strtok (NULL, ":");
  if (!token)
    return NO;
  *int_var = atoi (token);
  return YES;
}

void
init_board ()
{
  int pos;

  strcpy (player_name, "noboby");
  strcpy (opponent_name, "nobody");
  match_length = 0;
  player_got = 0;
  opponent_got = 0;
  for (pos = 0; pos < 26; pos++)
    board[pos] = 0;
  turn = 0;
  die1 = 0;
  die2 = 0;
  opponent_die1 = 0;
  opponent_die2 = 0;
  cube = 1;
  player_may_double = YES;
  opponent_may_double = YES;
  was_doubled = NO;
  color = COLOR_O;
  direction = 1;
  home = 25;
  bar = 0;
  player_on_home = 0;
  opponent_on_home = 0;
  can_move = 0;
  forced_move = 0;
  did_crawford = NO;
  max_redoubles = NO;
}

/* get all the integer numbers from the rawboard string.
 * The routine returns NO if an error occurs.
 * Note that get_int is a macro that does 'return NO' if
 * get_number fails!
 */

int
decode_board ()
{
  int pos;

  get_int (match_length);
  get_int (player_got);
  get_int (opponent_got);
  for (pos = 0; pos < 26; pos++)
    get_int (board[pos]);
  get_int (turn);
  get_int (die1);
  get_int (die2);
  get_int (opponent_die1);
  get_int (opponent_die2);
  get_int (cube);
  get_int (player_may_double);
  get_int (opponent_may_double);
  get_int (was_doubled);
  get_int (color);
  get_int (direction);
  get_int (home);
  get_int (bar);
  get_int (player_on_home);
  get_int (opponent_on_home);
  get_int (player_on_bar);
  get_int (opponent_on_bar);
  get_int (can_move);
  get_int (forced_move);
  get_int (did_crawford);
  get_int (max_redoubles);
  return YES;
}

/*
 * do_board is the main routine that can be called by extern routines.
 * It's argument is the rawboard line received from the backgammon
 * server. If something goes wrong do_board returns NO (0), otherwise
 * it returns YES (1). The board is stored in board_buffer. This
 * variable contains all the lines of the board separated by the
 * NEW_LINE string ("\r\n" or "\n"). The last line is not terminated
 * by NEW_LINE. board_buffer is one string terminated by \0.
 * If input==NULL the routine is used to initialize variables. The board
 * that is produced in this case should be written by the client if the
 * routine board_diff is used to update the board.
 */

int
do_board (input)
     char *input;
{
  int line;
  int count;
  char copy_of_input[255];
  char *token;

  for (count = 0; count < 127; count++)
    spaces[count] = ' ';
  spaces[count] = 0;
  if (!input)
  {
    /* build an empty board */
    init_board ();
    strcpy (old_board_buffer, "");
    old_board_ptr = old_board_buffer;
    old_board_col = old_board_line = 1;
  }
  else
  {
    if (strstr (input, "board:") != input)
      /* Oops! This is not a rawbord line! */
      return NO;
    /* extract board information from input */
    strcpy (copy_of_input, input);
    /* strtok will work on that copy (strtok destroys the string it scans) */
    /* skip the 'board:' at the beginning of the line: */
    token = strtok (copy_of_input, ":");
    /* next line is player's name.. */
    token = strtok (NULL, ":");
    if (!token)
      return NO;
    else
      strcpy (player_name, token);
    /* .. followed by opponent's name */
    token = strtok (NULL, ":");
    if (!token)
      return NO;
    else
      strcpy (opponent_name, token);
    /* The following arguments are integers and scanned in decode_board */
    if (!decode_board ())
      return NO;
    /* make a copy of the previous board */
    strcpy (old_board_buffer, board_buffer);
    old_board_ptr = old_board_buffer;
    old_board_col = old_board_line = 1;
  }
  /* Fun starts here: */
  /* clean board_buffer */
  strcpy (board_buffer, "");
  if (color == COLOR_O)
  {
    /* build board for O */
    top_lines ();
    for (line = 1; line <= 5; line++)
      one_line (line, 13, 24, UPPER_HALF);
    middle_line ();
    for (line = 5; line >= 1; line--)
      one_line (line, 12, 1, LOWER_HALF);
    bottom_lines ();
  }
  else
  {
    /* build board for X */
    top_lines ();
    for (line = 1; line <= 5; line++)
      one_line (line, 1, 12, UPPER_HALF);
    middle_line ();
    for (line = 5; line >= 1; line--)
      one_line (line, 24, 13, LOWER_HALF);
    bottom_lines ();
  }
  if (style != 4)
  {
    /* append the NEW_LINE that bottom_lines left out */
    strcat (board_buffer, NEW_LINE);
#ifdef BLANK_LINE
    strcat (board_buffer, NEW_LINE);
#endif
    /* Show some more information below the board: */
    sprintf
      (buf, "   BAR: O-%i X-%i   OFF: O-%i X-%i   Cube: %i",
       (color == COLOR_O) ? player_on_bar : opponent_on_bar,
       (color == COLOR_X) ? player_on_bar : opponent_on_bar,
       (color == COLOR_O) ? player_on_home : opponent_on_home,
       (color == COLOR_X) ? player_on_home : opponent_on_home,
       cube
      );
    strcpy (cur_line, buf);
    /* show who owns the cube */
    if (player_may_double == NO && opponent_may_double == YES)
    {
      /* owned by opponent */
      sprintf (buf, " (owned by %s)", opponent_name);
      strcat (cur_line, buf);
    }
    else if (player_may_double == YES && opponent_may_double == NO)
    {
      /* owned by player */
      sprintf (buf, " (owned by %s)", player_name);
      strcat (cur_line, buf);
    }
    if (die1 != 0)
    {
      sprintf (buf, "  %s rolled %i %i.", player_name, die1, die2);
      strcat (cur_line, buf);
    }
    else if (opponent_die1 != 0)
    {
      sprintf (buf, "  %s rolled %i %i.",
               opponent_name,
               opponent_die1,
               opponent_die2);
      strcat (cur_line, buf);
    }
    fill_line(cur_line,NO);
  }
  if (!input)
  {
     strcpy(old_board_buffer,board_buffer);
     old_board_ptr = old_board_buffer;
     old_board_col = old_board_line = 1;
  }
  if (strlen(old_board_buffer)!=strlen(board_buffer))
    fprintf(stderr,"!=");
  return YES;
}

/*
 * This routine can be called after do_board. It compares the current
 * board with the previous one. It sets x and y coordinates to the
 * position where a change occured and return a pointer to a nul-terminated
 * string representing the new text or NULL if no more changes can be found.
 */
char *
board_diff (x, y)
     int *x;
     int *y;
{
  char *difference_ptr = difference;

  /* search for next difference */
  /* first skip everything that's equal */
  while (*old_board_ptr && *old_board_ptr == board_buffer[old_board_ptr - old_board_buffer])
  {
    if (*old_board_ptr == '\n')
    {
      old_board_line++;
      old_board_col = 1;
    }
    else if (*old_board_ptr != '\r')
      old_board_col++;
    old_board_ptr++;
  }
  if (*old_board_ptr)
  {
    /* not 0 - we've found a difference */
    /* set returned screen position */
    *x = old_board_col;
    *y = old_board_line;
    /* now copy the difference to the string 'difference' */
    *difference_ptr = 0;
    while (*old_board_ptr && *old_board_ptr != board_buffer[old_board_ptr - old_board_buffer])
    {
      /* while the 2 boards differ */
      *(difference_ptr++) = board_buffer[old_board_ptr - old_board_buffer];
      if (*old_board_ptr == '\n')
      {
        old_board_line++;
        old_board_col = 1;
      }
      else if (*old_board_ptr != '\r')
        old_board_col++;
      old_board_ptr++;
    }
    /* end of difference */
    *difference_ptr = 0;
    return difference;
  }
  /* no difference found - first while got to end of board_buffer */
  return NULL;
}

#ifdef STAND_ALONE

void
usage (name)
     char *name;
{
  fprintf (stderr, "usage: %s [optional_filename]\n", name);
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  FILE *file;
  char line[2048];
  int x,
    y;
  char *diff;

  switch (argc)
  {
    case 1:
      /* no argument */
      file = stdin;
      break;
    case 2:
      if ((file = fopen (argv[1], "r")) == NULL)
      {
        fprintf (stderr, "can't open file '%s'\n", argv[1]);
        usage (argv[0]);
        return 1;
      }
      break;
    default:
      usage (argv[0]);
      return 1;
      break;
  }
  while (fgets (line, sizeof line, file) == line)
  {
    if (do_board (line))
      puts (board_buffer);
    else
      fputs (line, stdout);
  }
  return 0;
}
#endif
