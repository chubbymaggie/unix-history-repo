/* ansify.c
   Copyright (C) 1997, 2003 Free Software Foundation, Inc.
   Contributed by James Craig Burley.

This file is part of GNU Fortran.

GNU Fortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Fortran; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"

#define die_unless(c) \
  do if (!(c)) \
    { \
      fprintf (stderr, "%s:%lu: %s\n", argv[1], lineno, #c); \
      die (); \
    } \
  while(0)

static void ATTRIBUTE_NORETURN
die (void)
{
  exit (1);
}

int
main(int argc, char **argv)
{
  int c;
  static unsigned long lineno = 1;

  die_unless (argc == 2);

  printf ("\
/* This file is automatically generated from `%s',\n\
   which you should modify instead.  */\n\
#line 1 \"%s\"\n\
",
	  argv[1], argv[1]);

  while ((c = getchar ()) != EOF)
    {
      switch (c)
	{
	default:
	  putchar (c);
	  break;

	case '\n':
	  ++lineno;
	  putchar (c);
	  break;

	case '"':
	  putchar (c);
	  for (;;)
	    {
	      c = getchar ();
	      die_unless (c != EOF);
	      switch (c)
		{
		case '"':
		  putchar (c);
		  goto next_char;

		case '\n':
		  putchar ('\\');
		  putchar ('n');
		  putchar ('\\');
		  putchar ('\n');
		  ++lineno;
		  break;

		case '\\':
		  putchar (c);
		  c = getchar ();
		  die_unless (c != EOF);
		  putchar (c);
		  if (c == '\n')
		    ++lineno;
		  break;
		  
		default:
		  putchar (c);
		  break;
		}
	    }
	  break;

	case '\'':
	  putchar (c);
	  for (;;)
	    {
	      c = getchar ();
	      die_unless (c != EOF);
	      switch (c)
		{
		case '\'':
		  putchar (c);
		  goto next_char;
		  
		case '\n':
		  putchar ('\\');
		  putchar ('n');
		  putchar ('\\');
		  putchar ('\n');
		  ++lineno;
		  break;
		  
		case '\\':
		  putchar (c);
		  c = getchar ();
		  die_unless (c != EOF);
		  putchar (c);
		  if (c == '\n')
		    ++lineno;
		  break;
		  
		default:
		  putchar (c);
		  break;
		}
	    }
	  break;

	case '/':
	  putchar (c);
	  c = getchar ();
	  putchar (c);
	  if (c != '*')
	    break;
	  for (;;)
	    {
	      c = getchar ();
	      die_unless (c != EOF);

	      switch (c)
		{
		case '\n':
		  ++lineno;
		  putchar (c);
		  break;
		  
		case '*':
		  c = getchar ();
		  die_unless (c != EOF);
		  if (c == '/')
		    {
		      putchar ('*');
		      putchar ('/');
		      goto next_char;
		    }
		  if (c == '\n')
		    {
		      ++lineno;
		      putchar (c);
		    }
		  break;
		  
		default:
		  /* Don't bother outputting content of comments.  */
		  break;
		}
	    }
	  break;
	}
      
    next_char:
      ;
    }

  die_unless (c == EOF);

  return 0;
}
