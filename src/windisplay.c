/*
 *  A Z-Machine
 *  Copyright (C) 2000 Andrew Hunter
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Display for Win32
 */

#include "../config.h"

#if WINDOW_SYSTEM == 2

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <windows.h>

#include "zmachine.h"
#include "display.h"
#include "zoomres.h"
#include "rc.h"

#include "windisplay.h"
#include "xfont.h"

/***                           ----// 888 \\----                           ***/

static int process_events(long int, int*, int);

static char zoomClass[] = "ZoomWindowClass";
static HINSTANCE inst = NULL;

COLORREF wincolour[] =
{
  RGB(0xdd, 0xdd, 0xdd),
  RGB(0xaa, 0xaa, 0xaa),
  RGB(0xff, 0xff, 0xff),
  
  /* ZMachine colours start here */
  RGB(0x00, 0x00, 0x00),
  RGB(0xff, 0x00, 0x00),
  RGB(0x00, 0xff, 0x00),
  RGB(0xff, 0xff, 0x00),
  RGB(0x00, 0x00, 0xff),
  RGB(0xff, 0x00, 0xff),
  RGB(0x00, 0xff, 0xff),
  RGB(0xff, 0xff, 0xcc),

  RGB(0xbb, 0xbb, 0xbb),
  RGB(0x88, 0x88, 0x88),
  RGB(0x44, 0x44, 0x44)
};

#define DEFAULT_FORE 0
#define DEFAULT_BACK 7
#define FIRST_ZCOLOUR 3
#define FLASH_TIME 400

HBRUSH winbrush[14];
HPEN   winpen  [14];
HWND   mainwin;
HDC    mainwindc;

static xfont** font = NULL;

static char*   fontlist[] =
{
  "'Arial' 10",
  "'Arial' 10 b",
  "'Arial' 10 i",
  "'Courier New' 10 f",
  "font3",
  "'Arial' 10 ib",
  "'Courier New' 10 fb",
  "'Courier New' 10 fi",
  "'Courier New' 10 fib"
};

static int style_font[16] = { 0, 1, 2, 5, 3, 6, 7, 8,
			      4, 4, 4, 4, 4, 4, 4, 4 };

struct text
{
  int fg, bg;
  int font;

  int len;
  int* text;
  
  struct text* next;
};

struct line
{
  struct text* start;
  int          n_chars;
  int          baseline;
  int          ascent;
  int          descent;
  int          height;

  struct line* next;
};

struct cellline
{
  int*   cell;
  int*   fg;
  int*   bg;
  int*   font;
};

struct window
{
  int xpos, ypos;

  int winsx, winsy;
  int winlx, winly;

  int overlay;
  int force_fixed;
  int no_more;
  int no_scroll;

  int fore, back;
  int style;

  int winback;

  struct text* text;
  struct text* lasttext;
  
  struct line* line;
  struct line* lastline;

  struct cellline* cline;
};

int cur_win;
struct window text_win[3];
static int    nShow;

#define CURWIN text_win[cur_win]
#define CURSTYLE (text_win[cur_win].style|(text_win[cur_win].force_fixed<<8))

#define DEFAULTX 80
#define DEFAULTY 30
static int size_x, size_y;
static int max_x,  max_y;

int xfont_x = 0;
int xfont_y = 0;
static int win_x, win_y;
static int total_x, total_y;

static int caret_x, caret_y, caret_height;
static int caret_on = 0;
static int caret_shown = 0;
static int caret_flashing = 0;
static HPEN caret_pen;

/***                           ----// 888 \\----                           ***/

static inline int istrlen(const int* string)
{
  int x = 0;

  while (string[x] != 0) x++;
  return x;
}

/***                           ----// 888 \\----                           ***/

static void size_window(void)
{
  RECT rct, clrct;
  
  xfont_x = xfont_get_width(font[3]);
  xfont_y = xfont_get_height(font[3]);

  win_x = xfont_x*size_x;
  win_y = xfont_y*size_y;

  GetWindowRect(mainwin, &rct);
  GetClientRect(mainwin, &clrct);
  
  SetWindowPos(mainwin, HWND_TOP,
	       rct.top, rct.left,
	       win_x+8 + ((rct.right-rct.left)-clrct.right),
	       win_y+8 + ((rct.bottom-rct.top)-clrct.bottom),
	       0);

  total_x = win_x + 8;
  total_y = win_y + 8;
}

void display_initialise(void)
{
  int x;

  text_win[0].text        = NULL;
  text_win[0].lasttext    = NULL;
  text_win[0].line        = NULL;
  text_win[0].lastline    = NULL;
  text_win[0].cline       = NULL;

  /* Allocate colours */
  for (x=3; x<14; x++)
    {
      winbrush[x] = CreateSolidBrush(wincolour[x]);
      winpen[x]   = CreatePen(PS_SOLID, 1, wincolour[x]);
    }

  font = realloc(font, sizeof(xfont*)*9);
  /* Allocate fonts */
  for (x=0; x<9; x++)
    {
      font[x] = xfont_load_font(fontlist[x]);
    }

  max_x = size_x = DEFAULTX;
  max_y = size_y = DEFAULTY;
  
  size_window();
  
  ShowWindow(mainwin, nShow);
  UpdateWindow(mainwin);

  display_clear();
}

void display_reinitialise(void)
{
  int x;
  
  /* Deallocate colours */
  for (x=3; x<14; x++)
    {
      DeleteObject(winbrush[x]);
      DeleteObject(winpen[x]);
    }

  /* Deallocate fonts */
  for (x=0; x<5; x++)
    xfont_release_font(font[x]);

  /* Allocate colours */
  for (x=3; x<14; x++)
    {
      winbrush[x] = CreateSolidBrush(wincolour[x]);
      winpen[x]   = CreatePen(PS_SOLID, 1, wincolour[x]);
    }

  /* Allocate fonts */
  for (x=0; x<9; x++)
    {
      font[x] = xfont_load_font(fontlist[x]);
    }
}

void display_finalise(void)
{
  int x;
  
  /* Deallocate colours */
  for (x=3; x<14; x++)
    {
      DeleteObject(winbrush[x]);
      DeleteObject(winpen[x]);
    }

  /* Deallocate fonts */
  for (x=0; x<9; x++)
    xfont_release_font(font[x]);
}

/***                           ----// 888 \\----                           ***/

void display_clear(void)
{
  int x, y, z;
  
  /* Clear the main text window */
  text_win[0].force_fixed = 0;
  text_win[0].overlay     = 0;
  text_win[0].no_more     = 0;
  text_win[0].no_scroll   = 0;
  text_win[0].fore        = DEFAULT_FORE+FIRST_ZCOLOUR;
  text_win[0].back        = DEFAULT_BACK+FIRST_ZCOLOUR;
  text_win[0].style       = 0;
  text_win[0].xpos        = 0;
  text_win[0].ypos        = 0;
  text_win[0].winsx       = 0;
  text_win[0].winsy       = 0;
  text_win[0].winlx       = win_x;
  text_win[0].winly       = win_y;
  text_win[0].winback     = DEFAULT_BACK+FIRST_ZCOLOUR;

  /* Clear the overlay windows */
  for (x=1; x<3; x++)
    {
      text_win[x].force_fixed = 1;
      text_win[x].overlay     = 1;
      text_win[x].no_more     = 1;
      text_win[x].no_scroll   = 1;
      text_win[x].xpos        = 0;
      text_win[x].ypos        = 0;
      text_win[x].winsx       = 0;
      text_win[x].winsy       = 0;
      text_win[x].winlx       = win_x;
      text_win[x].winly       = 0;

      text_win[0].winback     = DEFAULT_BACK+FIRST_ZCOLOUR;
      text_win[x].fore        = DEFAULT_FORE+FIRST_ZCOLOUR;
      text_win[x].back        = DEFAULT_BACK+FIRST_ZCOLOUR;
      text_win[x].style       = 4;

      text_win[x].text        = NULL;
      text_win[x].line        = NULL;
      text_win[x].cline       = malloc(sizeof(struct cellline)*size_y);
      
      for (y=0; y<size_y; y++)
	{
	  text_win[x].cline[y].cell = malloc(sizeof(int)*size_x);
	  text_win[x].cline[y].fg   = malloc(sizeof(int)*size_x);
	  text_win[x].cline[y].bg   = malloc(sizeof(int)*size_x);
	  text_win[x].cline[y].font = malloc(sizeof(int)*size_x);

	  for (z=0; z<size_x; z++)
	    {
	      text_win[x].cline[y].cell[z] = ' ';
	      text_win[x].cline[y].fg[z]   = DEFAULT_FORE+FIRST_ZCOLOUR;
	      text_win[x].cline[y].bg[z]   = DEFAULT_BACK+FIRST_ZCOLOUR;
	      text_win[x].cline[y].font[z] = style_font[4];
	    }
	}
    }

  cur_win = 0;
  display_erase_window();
}

static void new_line(int more)
{
  struct line* line;
  RECT rct;

  if (CURWIN.lastline == NULL)
    {
      CURWIN.lastline = CURWIN.line = malloc(sizeof(struct line));

      CURWIN.line->start    = NULL;
      CURWIN.line->n_chars  = 0;
      CURWIN.line->baseline =
	CURWIN.ypos + xfont_get_ascent(font[style_font[(CURSTYLE>>1)&15]]);
      CURWIN.line->ascent   = xfont_get_ascent(font[style_font[(CURSTYLE>>1)&15]]);
      CURWIN.line->descent  = xfont_get_descent(font[style_font[(CURSTYLE>>1)&15]]);
      CURWIN.line->height   = xfont_get_height(font[style_font[(CURSTYLE>>1)&15]]);
      CURWIN.line->next     = NULL;

      return;
    }

  rct.top    = CURWIN.lastline->baseline - CURWIN.lastline->ascent+4;
  rct.bottom = CURWIN.lastline->baseline + CURWIN.lastline->descent+4;
  rct.left   = 4;
  rct.right  = win_x+4;
  InvalidateRect(mainwin, &rct, 0);
  
  line = malloc(sizeof(struct line));

  line->start     = NULL;
  line->n_chars   = 0;
  line->baseline  = CURWIN.lastline->baseline+CURWIN.lastline->descent;
  line->baseline += xfont_get_ascent(font[style_font[(CURSTYLE>>1)&15]]);
  line->ascent    = xfont_get_ascent(font[style_font[(CURSTYLE>>1)&15]]);
  line->descent   = xfont_get_descent(font[style_font[(CURSTYLE>>1)&15]]);
  line->height    = xfont_get_height(font[style_font[(CURSTYLE>>1)&15]]);
  line->next      = NULL;

  CURWIN.lastline->next = line;
  CURWIN.lastline = line;

  CURWIN.xpos = 0;
  CURWIN.ypos = line->baseline - line->ascent;

  if (line->baseline+line->descent > CURWIN.winly)
    {
      int toscroll;
      struct line* l;

      toscroll = (line->baseline+line->descent)-CURWIN.winly;
      l = CURWIN.line;

      while (l != NULL)
	{
	  l->baseline -= toscroll;
	  l = l->next;
	}

      display_update();
    }
}

static void format_last_text(int more)
{
  int x;
  struct text* text;
  int word_start, word_len, total_len, xpos;
  xfont* fn;
  struct line* line;
  RECT rct;
  
  text = CURWIN.lasttext;

  fn = font[text->font];

  if (CURWIN.lastline == NULL)
    {
      new_line(more);
    }

  word_start = 0;
  word_len   = 0;
  total_len  = 0;
  xpos       = CURWIN.xpos;
  line       = CURWIN.lastline;
  
  for (x=0; x<text->len; x++)
    {
      if (text->text[x] == ' '  ||
	  text->text[x] == '\n' ||
	  x == (text->len-1))
	{
	  int w;
	  
	  word_len++;

	  w = xfont_get_text_width(fn,
				   text->text + word_start,
				   word_len);
	  
	  /* We've got a word */
	  xpos += w;

	  if (xpos > CURWIN.winlx)
	    {
	      /* Put this word on the next line */
	      new_line(more);

	      xpos = CURWIN.xpos + w;
	      line = CURWIN.lastline;
	    }

	  if (line->start == NULL)
	    line->start = text;
	  line->n_chars += word_len;

	  word_start += word_len;
	  total_len  += word_len;
	  word_len    = 0;

	  if (text->text[x] == '\n')
	    {
	      new_line(more);

	      xpos = CURWIN.xpos;
	      line = CURWIN.lastline;
	    }
	}
      else
	{
	  word_len++;
	}
    }

  CURWIN.xpos = xpos;

  rct.top    = CURWIN.lastline->baseline - CURWIN.lastline->ascent+4;
  rct.bottom = CURWIN.lastline->baseline + CURWIN.lastline->descent+4;
  rct.left   = 4;
  rct.right  = win_x+4;
  InvalidateRect(mainwin, &rct, 0);
}

void display_prints(const int* str)
{
  if (CURWIN.overlay)
    {
      int x;
      RECT rct;
      int sx;

      CURWIN.style |= 8;
      sx = CURWIN.xpos;
      
      /* Is an overlay window */
      for (x=0; str[x] != 0; x++)
	{
	  if (str[x] > 31)
	    {
	      if (CURWIN.xpos >= size_x)
		{
		  rct.top = CURWIN.ypos*xfont_y+4;
		  rct.bottom = CURWIN.ypos*xfont_y+4+xfont_y;
		  rct.left   = sx*xfont_x+4;
		  rct.right  = win_x+4;
		  InvalidateRect(mainwin, &rct, 0);
		  sx = 0;
		  
		  CURWIN.xpos = 0;
		  CURWIN.ypos++;
		}
	      if (CURWIN.ypos >= size_y)
		{
		  CURWIN.ypos = size_y-1;
		}

	      CURWIN.cline[CURWIN.ypos].cell[CURWIN.xpos] = str[x];
	      if (CURWIN.style&1)
		{
		  CURWIN.cline[CURWIN.ypos].fg[CURWIN.xpos]   = CURWIN.back;
		  CURWIN.cline[CURWIN.ypos].bg[CURWIN.xpos]   = CURWIN.fore;
		}
	      else
		{
		  CURWIN.cline[CURWIN.ypos].fg[CURWIN.xpos]   = CURWIN.fore;
		  CURWIN.cline[CURWIN.ypos].bg[CURWIN.xpos]   = CURWIN.back;
		}
	      CURWIN.cline[CURWIN.ypos].font[CURWIN.xpos] = style_font[(CURSTYLE>>1)&15];
	      
	      CURWIN.xpos++;
	    }
	  else
	    {
	      switch (str[x])
		{
		case 10:
		case 13:
		  rct.top = CURWIN.ypos*xfont_y+4;
		  rct.bottom = CURWIN.ypos*xfont_y+4+xfont_y;
		  rct.left   = sx*xfont_x+4;
		  rct.right  = CURWIN.xpos*xfont_x+4;
		  InvalidateRect(mainwin, &rct, 0);

		  sx = 0;
		  CURWIN.xpos = 0;
		  CURWIN.ypos++;
		  
		  if (CURWIN.ypos >= size_y)
		    {
		      CURWIN.ypos = size_y-1;
		    }
		  break;
		}
	    }
	}

      rct.top = CURWIN.ypos*xfont_y+4;
      rct.bottom = CURWIN.ypos*xfont_y+4+xfont_y;
      rct.left   = sx*xfont_x+4;
      rct.right  = CURWIN.xpos*xfont_x+4;
      InvalidateRect(mainwin, &rct, 0);
    }
  else
    {
      struct text* text;

      text = malloc(sizeof(struct text));

      if (CURWIN.style&1)
	{
	  text->fg   = CURWIN.back;
	  text->bg   = CURWIN.fore;
	}
      else
	{
	  text->fg   = CURWIN.fore;
	  text->bg   = CURWIN.back;
	}
      text->font = style_font[(CURSTYLE>>1)&15];
      text->len  = istrlen(str);
      text->text = malloc(sizeof(int)*text->len);
      text->next = NULL;
      memcpy(text->text, str, sizeof(int)*text->len);

      if (CURWIN.lasttext == NULL)
	{
	  CURWIN.text = text;
	  CURWIN.lasttext = text;
	}
      else
	{
	  CURWIN.lasttext->next = text;
	  CURWIN.lasttext = text;
	}

      format_last_text(-1);
    }
}

void display_prints_c(const char* str)
{
  int* txt;
  int x, len;

  txt = malloc((len=strlen(str))*sizeof(int)+sizeof(int));
  for (x=0; x<=len; x++)
    {
      txt[x] = str[x];
    }
  display_prints(txt);
  free(txt);
}

void display_printf(const char* format, ...)
{
  va_list* ap;
  char     string[512];
  int x,len;
  int      istr[512];

  va_start(ap, format);
  vsprintf(string, format, ap);
  va_end(ap);

  len = strlen(string);
  
  for (x=0; x<=len; x++)
    {
      istr[x] = string[x];
    }
  display_prints(istr);
}

/***                           ----// 888 \\----                           ***/

static int debug_console = 0;
static HANDLE console_buffer = INVALID_HANDLE_VALUE;
static int console_exit = 0;

static BOOL ctlHandler(DWORD ct)
{
  switch (ct)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
      return TRUE;

    default:
      return FALSE;
    }
}

static BOOL ctlExitHandler(DWORD ct)
{
  switch (ct)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
      console_exit = 1;
      ExitProcess(0);
      return TRUE;

    default:
      return FALSE;
    }
}

void printf_debug(char* format, ...)
{
  va_list* ap;
  char     string[512];

  va_start(ap, format);
  vsprintf(string, format, ap);
  va_end(ap);

  if (!debug_console)
    {
      if (!AllocConsole())
	{
	  FreeConsole();
	  AllocConsole();
	}

      SetConsoleTitle("Zoom debug information");
      debug_console = 1;
      console_buffer =
	CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,
				  FILE_SHARE_READ|FILE_SHARE_WRITE,
				  NULL,
				  CONSOLE_TEXTMODE_BUFFER,
				  NULL);
      SetConsoleCtrlHandler(ctlHandler, TRUE);
      SetConsoleActiveScreenBuffer(console_buffer);
    }

  if (console_buffer != INVALID_HANDLE_VALUE)
    {
      DWORD written;
      
      WriteConsole(console_buffer, string, strlen(string), &written, NULL);
    }
}

void printf_info (char* format, ...)
{
  va_list* ap;
  char     string[512];

  va_start(ap, format);
  vsprintf(string, format, ap);
  va_end(ap);

  if (!debug_console)
    {
      if (!AllocConsole())
	{
	  FreeConsole();
	  AllocConsole();
	}
      
      SetConsoleTitle("Zoom debug information");
      debug_console = 1;
      console_buffer =
	CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,
				  FILE_SHARE_READ|FILE_SHARE_WRITE,
				  NULL,
				  CONSOLE_TEXTMODE_BUFFER,
				  NULL);
      SetConsoleCtrlHandler(ctlHandler, TRUE);
      SetConsoleActiveScreenBuffer(console_buffer);
    }

  if (console_buffer != INVALID_HANDLE_VALUE)
    {
      DWORD written;
      
      WriteConsole(console_buffer, string, strlen(string), &written, NULL);
    }
}

void printf_info_done(void)
{
}

void display_exit(int code)
{
  CloseWindow(mainwin);

  if (debug_console)
    {
      printf_debug("\n\n(Close this window to exit)\n");
      SetConsoleCtrlHandler(ctlHandler, FALSE);
      if (SetConsoleCtrlHandler(ctlExitHandler, TRUE))
	{
	  MSG msg;
	  
	  while (!console_exit && GetMessage(&msg, NULL, 0, 0))
	    {
	      TranslateMessage(&msg);
	      DispatchMessage(&msg);
	    }
	}
    }
  
  exit(code);
}

static char* error = NULL;
void printf_error(char* format, ...)
{
  va_list* ap;
  char     string[512];

  va_start(ap, format);
  vsprintf(string, format, ap);
  va_end(ap);

  if (error == NULL)
    {
      error = malloc(strlen(string)+1);
      strcpy(error, string);
    }
  else
    {
      error = realloc(error, strlen(error)+strlen(string)+1);
      strcat(error, string);
    }
}

void printf_error_done(void)
{
  MessageBox(0, error, "Zoom - error", MB_ICONEXCLAMATION|MB_TASKMODAL);
  free(error);
  error = NULL;
}

/***                           ----// 888 \\----                           ***/

int display_readline(int* buf, int buflen, long int timeout)
{
  int result;
  
  result = process_events(timeout, buf, buflen);

  if (result)
    new_line(0);

  return result;
}

int display_readchar(long int timeout)
{
  return process_events(timeout, NULL, 0);
}

/***                           ----// 888 \\----                           ***/

ZDisplay* display_get_info(void)
{
  static ZDisplay dis;

  dis.status_line   = 1;
  dis.can_split     = 1;
  dis.variable_font = 1;
  dis.colours       = 1;
  dis.boldface      = 1;
  dis.italic        = 1;
  dis.fixed_space   = 1;
  dis.sound_effects = 0;
  dis.timed_input   = 1;
  dis.mouse         = 0;

  dis.lines   = size_y;
  dis.columns = size_x;
  dis.width   = size_x;
  dis.height  = size_y;
  dis.font_width  = 1;
  dis.font_height = 1;
  dis.pictures    = 0;
  dis.fore        = DEFAULT_FORE;
  dis.back        = DEFAULT_BACK;

  return &dis;
}

void display_set_title(const char* title)
{
  char* str;

  str = malloc(strlen(title)+strlen("Zoom " VERSION " - ")+1);
  strcpy(str, "Zoom " VERSION " - ");
  strcat(str, title);
  SetWindowText(mainwin, str);
}

void display_update(void)
{
  RECT rct;

  rct.top = rct.left = 0;
  rct.right  = total_x;
  rct.bottom = total_y;
  InvalidateRect(mainwin, &rct, 0);
}

/***                           ----// 888 \\----                           ***/

void display_set_colour(int fore, int back)
{
  if (fore == -1)
    fore = DEFAULT_FORE;
  if (back == -1)
    back = DEFAULT_BACK;
  if (fore == -2)
    fore = CURWIN.fore;
  if (back == -2)
    back = CURWIN.back;

  CURWIN.fore = fore + FIRST_ZCOLOUR;
  CURWIN.back = back + FIRST_ZCOLOUR;
}

void display_split(int lines, int window)
{
  text_win[window].winsx = CURWIN.winsx;
  text_win[window].winlx = CURWIN.winsx;
  text_win[window].winsy = CURWIN.winsy;
  text_win[window].winly = CURWIN.winsy + xfont_y*lines;
  text_win[window].xpos  = 0;
  text_win[window].ypos  = 0;

  CURWIN.winsy += xfont_y*lines;
  if (CURWIN.ypos < CURWIN.winsy)
    CURWIN.ypos = CURWIN.winsy;
}

void display_join(int window1, int window2)
{
  if (text_win[window1].winsy != text_win[window2].winly)
    return; /* Windows can't be joined */
  text_win[window1].winsy = text_win[window2].winsy;
  text_win[window2].winly = text_win[window2].winsy;  
}

void display_set_cursor(int x, int y)
{
  if (CURWIN.overlay)
    {
      CURWIN.xpos = x;
      CURWIN.ypos = y;
    }
  else
    {
      if (CURWIN.line != NULL)
	zmachine_fatal("Can't move the cursor in a non-overlay window when text has been printed");

      CURWIN.xpos = x*xfont_x;
      CURWIN.ypos = y*xfont_y;
    }
}

void display_set_gcursor(int x, int y)
{
  display_set_cursor(x,y);
}

void display_set_scroll(int scroll)
{
}

int display_get_gcur_x(void)
{
  return CURWIN.xpos;
}

int display_get_gcur_y(void)
{
  return CURWIN.ypos;
}

int display_get_cur_x(void)
{
  return CURWIN.xpos;
}

int display_get_cur_y(void)
{
  return CURWIN.ypos;
}

int display_set_font(int font)
{
  switch (font)
    {
    case -1:
      display_set_style(-16);
      break;

    default:
      break;
    }

  return 0;
}

int display_set_style(int style)
{
  int old_style;

  old_style = CURWIN.style;
  
  if (style == 0)
    CURWIN.style = 0;
  else
    {
      if (style > 0)
	CURWIN.style |= style;
      else
	CURWIN.style &= ~(-style);
    }

  return old_style;
}

void display_set_window(int window)
{
  text_win[window].fore = CURWIN.fore;
  text_win[window].back = CURWIN.back;
  text_win[window].style = CURWIN.style;
  cur_win = window;
}

int display_get_window(void)
{
  return cur_win;
}

void display_set_more(int window,
		      int more)
{
}

void display_erase_window(void)
{
  RECT rct;
  
  if (CURWIN.overlay)
    {
      int x,y;
      
      for (y=0; y<(CURWIN.winly/xfont_y); y++)
	{
	  for (x=0; x<size_x; x++)
	    {
	      CURWIN.cline[y].cell[x] = ' ';
	      CURWIN.cline[y].fg[x]   = CURWIN.fore;
	      CURWIN.cline[y].bg[x]   = CURWIN.back;
	      CURWIN.cline[y].font[x] = style_font[4];
	    }
	}
    }
  else
    {
      struct text* text;
      struct text* nexttext;
      struct line* line;
      struct line* nextline;
      int x, y, z;

      text = CURWIN.text;
      while (text != NULL)
	{
	  nexttext = text->next;
	  free(text->text);
	  free(text);
	  text = nexttext;
	}
      CURWIN.text = CURWIN.lasttext = NULL;
      CURWIN.winback = CURWIN.back;

      line = CURWIN.line;
      while (line != NULL)
	{
	  nextline = line->next;
	  free(line);
	  line = nextline;
	}
      CURWIN.line = CURWIN.lastline = NULL;
      
      for (y=(CURWIN.winsy/xfont_y); y<size_y; y++)
	{
	  for (x=0; x<size_x; x++)
	    {
	      for (z=1; z<=2; z++)
		{
		  text_win[z].cline[y].cell[x] = ' ';
		  text_win[z].cline[y].fg[x]   = FIRST_ZCOLOUR+DEFAULT_FORE;
		  text_win[z].cline[y].bg[x]   = CURWIN.winback;
		  text_win[z].cline[y].font[x] = style_font[4];
		}
	    }
	}
    }

  rct.top = 0;
  rct.left = 0;
  rct.right = total_x;
  rct.bottom = total_y;
  InvalidateRect(mainwin, &rct, 0);
}

void display_erase_line(int val)
{
  if (CURWIN.overlay)
    {
      int x;
      RECT rct;
      
      if (val == 1)
	val = size_x;
      else
	val += CURWIN.xpos;

      for (x=CURWIN.xpos; x<val; x++)
	{
	  CURWIN.cline[CURWIN.ypos].cell[x] = ' ';
	  CURWIN.cline[CURWIN.ypos].fg[x]   = CURWIN.fore;
	  CURWIN.cline[CURWIN.ypos].bg[x]   = CURWIN.back;
	  CURWIN.cline[CURWIN.ypos].font[x] = style_font[4];
	}

      rct.top = CURWIN.ypos*xfont_y;
      rct.left = 0;
      rct.right = total_x;
      rct.bottom = CURWIN.ypos*xfont_y+xfont_y;
      InvalidateRect(mainwin, &rct, 0);
    }
}

void display_force_fixed(int window,
			 int val)
{
  CURWIN.force_fixed = val;
}


/***                           ----// 888 \\----                           ***/

void display_beep(void)
{
}

/***                           ----// 888 \\----                           ***/

static void draw_window(int win,
			HDC dc)
{
  if (text_win[win].overlay)
    {
      int x, y;

      x = 0; y = 0;

      for (y=0; y<size_y; y++)
	{
	  for (x=0; x<size_x; x++)
	    {
	      if (text_win[win].cline[y].cell[x] != ' ' ||
		  text_win[win].cline[y].bg[x]   != text_win[0].winback)
		{
		  int len;
		  int fn, fg, bg;

		  len = 1;
		  fg = text_win[win].cline[y].fg[x];
		  bg = text_win[win].cline[y].bg[x];
		  fn = text_win[win].cline[y].font[x];
		  
		  while (text_win[win].cline[y].font[x+len] == fn &&
			 text_win[win].cline[y].fg[x+len]   == fg &&
			 text_win[win].cline[y].bg[x+len]   == bg &&
			 (bg != DEFAULT_BACK+FIRST_ZCOLOUR ||
			  text_win[win].cline[y].cell[x+len] != ' '))
		    len++;

		  xfont_set_colours(text_win[win].cline[y].fg[x],
				    text_win[win].cline[y].bg[x]);
		  xfont_plot_string(font[text_win[win].cline[y].font[x]],
				    dc,
				    x*xfont_x+4,
				    y*xfont_y+4,
				    &text_win[win].cline[y].cell[x],
				    len);

		  x+=len-1;
		}
	    }
	}
    }
  else
    {
      RECT rct;
      
      struct line* line;
      struct text* text;

      struct text* lasttext;
      int lastchars, nchars, x, width, lasty;
      int startchar;

      line = text_win[win].line;
      lastchars = 0;
      lasttext = NULL;

      if (line != NULL)
	{
	  rct.top    = 4;
	  rct.bottom = line->baseline - line->ascent+4;
	  rct.left   = 4;
	  rct.right  = 4+win_x;
	  if (rct.top < rct.bottom)
	    FillRect(dc, &rct, winbrush[text_win[win].winback]);

	  lasty = rct.bottom;
	}
      else
	lasty = 4;
      
      while (line != NULL)
	{
	  width     = 0;
	  nchars    = 0;
	  startchar = 0;
	  text      = line->start;

	  if (text == NULL && line->n_chars > 0)
	    zmachine_fatal("Programmer is a spoon");

	  if (text == lasttext)
	    {
	      startchar = lastchars;
	      nchars = lastchars;
	    }

	  for (x=0; x<line->n_chars; x++)
	    {
	      nchars++;
	      if (nchars == text->len || x == line->n_chars-1)
		{
		  int w;
		  int sub = 0;
		  
		  if (text->text[nchars-1] == '\n')
		    sub = 1;
		  
		  w = xfont_get_text_width(font[text->font],
					   text->text + startchar,
					   nchars - startchar - sub);
		  
		  rct.top    = line->baseline - line->ascent+4;
		  rct.bottom = line->baseline + line->descent+4;
		  rct.left   = width+4;
		  rct.right  = width+w+4;
		  if (rct.top < rct.bottom)
		    FillRect(dc, &rct, winbrush[text->bg]);
		  
		  xfont_set_colours(text->fg,
				    text->bg);
		  xfont_plot_string(font[text->font],
				    dc,
				    width+4,
				    line->baseline-
				    xfont_get_ascent(font[text->font])+4,
				    text->text + startchar,
				    nchars - startchar - sub);
		  
		  lastchars = nchars;
		  lasttext = text;
		  
		  nchars = 0;
		  text = text->next;
		  startchar = 0;
		  
		  width += w;
		}
	    }
	  
	  rct.top    = line->baseline-line->ascent+4;
	  rct.bottom = line->baseline+line->descent+4;
	  rct.left   = width+4;
	  rct.right  = win_x+4;
	  FillRect(dc, &rct, winbrush[text_win[win].winback]);
	  
	  lasty = rct.bottom;
	      
	  line = line->next;
	}

      rct.top    = lasty;
      rct.bottom = win_y+4;
      rct.left   = 4;
      rct.right  = 4+win_x;
      if (rct.top < rct.bottom)
	FillRect(dc, &rct, winbrush[text_win[win].winback]); 
    }
}

static void draw_caret(HDC dc)
{
  if ((caret_on^caret_shown))
    {
      HPEN tpen;
      
      SetROP2(dc, R2_XORPEN);

      SelectObject(dc, caret_pen);
      MoveToEx(dc, caret_x+1, caret_y, NULL);
      LineTo(dc, caret_x+1, caret_y+caret_height);

      tpen = CreatePen(PS_SOLID, 2, wincolour[CURWIN.back]);
      SelectObject(dc, tpen);
      MoveToEx(dc, caret_x+1, caret_y, NULL);
      LineTo(dc, caret_x+1, caret_y+caret_height);
      DeleteObject(tpen);
      
      SetROP2(dc, R2_COPYPEN);
      caret_shown = !caret_shown;
    }
}

static void redraw_caret(void)
{
  draw_caret(mainwindc);
}

static void show_caret(void)
{
  caret_on = 1;
  redraw_caret();
}

static void hide_caret(void)
{
  caret_on = 0;
  redraw_caret();
}

static void flash_caret(void)
{
  caret_on = !caret_on;
  redraw_caret();
}

static void resize_window()
{
  RECT rct;
  int owin;
  int x,y,z;

  if (xfont_x == 0 || xfont_y == 0)
    return;
  
  owin = cur_win;
  cur_win = 0;

  GetClientRect(mainwin, &rct);
  
  if (rct.bottom <= CURWIN.winsy)
    rct.bottom = CURWIN.winsy + xfont_y;
  
  size_x = (rct.right-8)/xfont_x;
  size_y = (rct.bottom-8)/xfont_y;

  win_x = xfont_x * size_x;
  win_y = xfont_y * size_y;

  total_x = rct.right;
  total_y = rct.bottom;

  CURWIN.winlx = win_x;
  CURWIN.winly = win_y;
  
  /* Resize and reformat the text window */
  if (CURWIN.line != NULL)
    {
      struct line* line;
      struct line* next;
      
      CURWIN.ypos = CURWIN.line->baseline - CURWIN.line->ascent;
      CURWIN.xpos = 0;

      line = CURWIN.line;
      while (line != NULL)
	{
	  next = line->next;
	  free(line);
	  line = next;
	}

      CURWIN.line = CURWIN.lastline = NULL;

      if (CURWIN.text != NULL)
	{
	  CURWIN.lasttext = CURWIN.text;
	  while (CURWIN.lasttext->next != NULL)
	    {
	      format_last_text(0);
	      CURWIN.lasttext = CURWIN.lasttext->next;
	    }
	  format_last_text(0);
	}
    }

  /* Resize and reformat the overlay windows */
  for (x=1; x<=2; x++)
    {
      cur_win = x;
      
      if (size_y > max_y)
	{
	  CURWIN.cline = realloc(CURWIN.cline, sizeof(struct cellline)*size_y);

	  /* Allocate new rows */
	  for (y=max_y; y<size_y; y++)
	    {
	      CURWIN.cline[y].cell = malloc(sizeof(int)*size_x);
	      CURWIN.cline[y].fg   = malloc(sizeof(int)*size_x);
	      CURWIN.cline[y].bg   = malloc(sizeof(int)*size_x);
	      CURWIN.cline[y].font = malloc(sizeof(int)*size_x);

	      for (z=0; z<max_x; z++)
		{
		  CURWIN.cline[y].cell[z] = ' ';
		  CURWIN.cline[y].fg[z]   = DEFAULT_FORE+FIRST_ZCOLOUR;
		  CURWIN.cline[y].bg[z]   = DEFAULT_BACK+FIRST_ZCOLOUR;
		  CURWIN.cline[y].font[z] = style_font[4];
		}
	    }
	}
      
      if (size_x > max_x)
	{
	  /* Allocate new columns */
	  for (y=0; y<size_y; y++)
	    {
	      CURWIN.cline[y].cell = realloc(CURWIN.cline[y].cell,
					     sizeof(int)*size_x);
	      CURWIN.cline[y].fg   = realloc(CURWIN.cline[y].fg,
					     sizeof(int)*size_x);
	      CURWIN.cline[y].bg   = realloc(CURWIN.cline[y].bg,
					     sizeof(int)*size_x);
	      CURWIN.cline[y].font = realloc(CURWIN.cline[y].font,
					     sizeof(int)*size_x);
	      for (z=max_x; z<size_x; z++)
		{
		  CURWIN.cline[y].cell[z] = ' ';
		  CURWIN.cline[y].fg[z]   = DEFAULT_FORE+FIRST_ZCOLOUR;
		  CURWIN.cline[y].bg[z]   = DEFAULT_BACK+FIRST_ZCOLOUR;
		  CURWIN.cline[y].font[z] = style_font[4];
		}
	    }
	}
    }

  if (size_x > max_x)
    max_x = size_x;
  if (size_y > max_y)
    max_y = size_y;

  zmachine_resize_display(display_get_info());
  
  cur_win = owin;
}

static LRESULT CALLBACK display_winproc(HWND hwnd,
					UINT message,
					WPARAM wparam,
					LPARAM lparam)
{
  switch (message)
    {
    case WM_PAINT:
      {
	PAINTSTRUCT paint;
	RECT        rct;
	int x;
	
	BeginPaint(hwnd, &paint);

	for (x=0; x<3; x++)
	  draw_window(x, paint.hdc);

	rct.left   = 0;
	rct.right  = 3;
	rct.top    = 0;
	rct.bottom = total_y+1;
	FillRect(paint.hdc, &rct, winbrush[0]);

	rct.left   = win_x+6;
	rct.right  = total_x+1;
	rct.top    = 0;
	rct.bottom = total_y+1;
	FillRect(paint.hdc, &rct, winbrush[0]);

	rct.left   = 0;
	rct.right  = total_x+1;
	rct.top    = 0;
	rct.bottom = 3;
	FillRect(paint.hdc, &rct, winbrush[0]);

	rct.left   = 0;
	rct.right  = total_x+1;
	rct.top    = win_y+6;
	rct.bottom = total_y+1;
	FillRect(paint.hdc, &rct, winbrush[0]);
	
	SelectObject(paint.hdc, winpen[1]);
	
	MoveToEx(paint.hdc, 3, win_y+5, NULL);
	LineTo(paint.hdc, 3, 3);
	MoveToEx(paint.hdc, win_x+5, 3, NULL);
	LineTo(paint.hdc, 3, 3);

	SelectObject(paint.hdc, winpen[2]);
	MoveToEx(paint.hdc, win_x+5, win_y+5, NULL);
	LineTo(paint.hdc, win_x+5, 3);
	MoveToEx(paint.hdc, win_x+5, win_y+5, NULL);
	LineTo(paint.hdc, 3, win_y+5);

	caret_shown = 0;
	draw_caret(paint.hdc);
	
	EndPaint(hwnd, &paint);
      }
      break;

    case WM_SIZE:
      resize_window();
      return DefWindowProc(hwnd, message, wparam, lparam);
      
    case WM_CLOSE:
      DestroyWindow(hwnd);
      break;

    case WM_TIMER:
      switch (wparam)
	{
	case 1:
	  if (caret_flashing)
	    {
	      flash_caret();
	    }
	  SetTimer(mainwin, 1, FLASH_TIME, NULL);
	  break;
	  
	default:
	  zmachine_fatal("Unknown timer event type %i (Programmer is a spoon)", wparam);
	}
      break;

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
    }
  return 0;
}

extern int zoom_main(int, char**);

int WINAPI WinMain(HINSTANCE hInst, 
		   HINSTANCE hPrev, 
		   LPSTR lpCmd,
		   int show)
{
  char** argv = NULL;
  int argc,x;

  WNDCLASSEX class;

  nShow = show;

#ifdef DEBUG
  debug_printf("Zoom " VERSION " compiled for Windows\n\n");
#endif
  
  /* Parse the command string */
  argv = malloc(sizeof(char*));
  argv[0] = malloc(sizeof(char)*strlen("zoom"));
  strcpy(argv[0], "zoom");
  argc = 1;
  for (x=0; lpCmd[x] != 0; x++)
    {
      int len;
      
      while (lpCmd[x] == ' ')
	x++;

      if (lpCmd[x] != 0)
	{
	  argv = realloc(argv,
			 sizeof(char*)*(argc+1));
	  argv[argc] = NULL;
	    
	  len = 0;
	  while (lpCmd[x] != ' ' &&
		 lpCmd[x] != 0)
	    {
	      argv[argc] = realloc(argv[argc],
				   sizeof(char)*(len+2));
	      argv[argc][len++] = lpCmd[x];
	      argv[argc][len]   = 0;
	      x++;
	    }

	  argc++;
	}
    }

  printf_debug("Got %i arguments (commandline '%s')\n", argc, lpCmd);
  for (x=0; x<argc; x++)
    {
      printf_debug("Argument %i: '%s'\n", x, argv[x]);
    }

  /* Allocate the three 'standard' brushes */
  winbrush[0] = CreateSolidBrush(wincolour[0]);
  winbrush[1] = CreateSolidBrush(wincolour[1]);
  winbrush[2] = CreateSolidBrush(wincolour[2]);
  
  winpen[0] = CreatePen(PS_SOLID, 1, wincolour[0]);
  winpen[1] = CreatePen(PS_SOLID, 2, wincolour[1]);
  winpen[2] = CreatePen(PS_SOLID, 2, wincolour[2]);
  caret_pen = CreatePen(PS_SOLID, 2, RGB(220, 0, 0));
  
  /* Create the main Zoom window */
  inst = hInst;
  
  class.cbSize        = sizeof(WNDCLASSEX);
  class.style         = CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
  class.lpfnWndProc   = display_winproc;
  class.cbClsExtra    = class.cbWndExtra = 0;
  class.hInstance     = hInst;
  class.hIcon         = LoadIcon(inst, MAKEINTRESOURCE(ID_ICON));
  class.hCursor       = LoadCursor(NULL, IDC_ARROW);
  class.hbrBackground = NULL;
  class.lpszMenuName  = NULL;
  class.lpszClassName = zoomClass;
  class.hIconSm       = LoadIcon(inst, MAKEINTRESOURCE(ID_SMICON));

  if (!RegisterClassEx(&class))
    {
      MessageBox(0, "Failed to register window class", "Error",
		 MB_ICONEXCLAMATION | MB_OK | MB_SYSTEMMODAL);
      return 0;
    }
  
  mainwin = CreateWindowEx(0,
			   zoomClass,
			   "Zoom " VERSION,
			   WS_OVERLAPPEDWINDOW,
			   CW_USEDEFAULT, CW_USEDEFAULT,
			   100, 100,
			   NULL, NULL,
			   inst, NULL);
  mainwindc = GetDC(mainwin);

  SetTimer(mainwin, 1, FLASH_TIME, NULL);
  
  zoom_main(argc, argv);
  
  return 0;
}

static int process_events(long int timeout,
			  int* buf,
			  int  buflen)
{
  MSG msg;
  int wason;

  wason = caret_on;
  hide_caret();
  if (CURWIN.overlay)
    {
      caret_x = CURWIN.xpos*xfont_x+4;
      caret_y = CURWIN.ypos*xfont_y+4;
    }
  else
    {
      caret_x = CURWIN.xpos+4;
      caret_y = CURWIN.ypos+4;
    }
  caret_height   = 10;
  caret_flashing = 1;

  if (wason)
    show_caret();
  
  while (GetMessage(&msg, NULL, 0, 0))
    {
      TranslateMessage(&msg);

      switch (msg.message)
	{
	case WM_CHAR:
	  if (buf == NULL)
	    {
	      hide_caret();
	      caret_flashing = 0;
	      return msg.wParam;
	    }
	  break;

	default:
	  DispatchMessage(&msg);
	}
    }

  display_exit(0);
}

#endif