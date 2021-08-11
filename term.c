#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "term.h"

struct termios setup_term()
{
  static struct termios oldt, newt;

/* Alternative Buffer */
  printf("\033[?1049h");

  /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
  tcgetattr(STDIN_FILENO, &oldt);
  /*now the settings will be copied*/
  newt = oldt;
  
  /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
  newt.c_lflag &= ~(ICANON);

  newt.c_cc[VTIME]=0;
  newt.c_cc[VMIN]=1;

  newt.c_cflag &= ~(CBAUDEX);

  if(cfsetispeed(&newt, B230400) < 0 || cfsetospeed(&newt, B230400) < 0)
    {
      printf("Error\n");
      return (struct termios) {0};
    }

  /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);

  return oldt;
}

void reset_term(struct termios oldt)
{
  /*restore the old settings*/
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

  /* Return to normal buffer */
  printf("\033[?1049l");
}

int parse_input(char input, char *command)
{
  if (command[0] != ':')
    {  
      switch(input)
	{
	case 'q':
	  strcat(command, ":quit"); 
	  break;
      
	case 'j':
	  strcat(command, ":down"); 
	  break;
	
	case 'k':
	  strcat(command, ":up"); 
	  break;

	case ':':
	  strcat(command, ":");
	default:
	  return 0;
	}
      return 1;
    }
  else
    //Selects the course of action specified by the option
    switch(input)
      {
      case 0x7f:
	command[strlen(command)-1] = 0x0;
	break;
      case '\n':
	return 1;
	break;
      default:
	command[strlen(command)] = input;
	break;
      }

  return 0;
}

enum line_types
{
  NORMAL_LINE,
  LINK_LINE,
  PREFORMATTED_TOGGLE_LINE,
  PREFORMATTED_TEXT_LINE,
  HEADING_LINE,
  SUBHEADING_LINE,
  SUBSUBHEADING_LINE,
  LIST_LINE,
  QUOTE_LINE,
};

bool print_text(char *buf, unsigned long buflen, struct winsize ws, int start_line) 
{
  char ch;
  int tmpi=0, line=0, preformatted_tmp=0;
  bool reached_end = true;
  bool line_start = true;
  bool newline = false;
  bool print_heading = false;
  bool preformatted_line = false;
  enum line_types line_type = NORMAL_LINE;

  for (unsigned long i = 0; i < buflen; i++)
    {
      ch = buf[i];
      tmpi++;
      
      if (ch == '\n')
	newline = true;
      
      if (tmpi > ws.ws_col-1)
	{
	  if (line >= start_line)
	    putchar(ch);
	  newline = true;
	}
      
      if (line-start_line > ws.ws_row-2)
	{
	  reached_end = false;
	  break;
	}
      
      if (newline)
	{
	  ch = '\n';
	  line++;
	  tmpi=0;
	  newline = false;
	  line_start = true;
	  preformatted_tmp = 0;
	  printf("\033[39;49;22;23;24;25m"); /* Reset styling */

	  if (line_type == PREFORMATTED_TOGGLE_LINE)
	    {
	      line_type = NORMAL_LINE;
	      goto end;
	    }
	  else
	    line_type = NORMAL_LINE;
	}
      
      if (line < start_line-1)
	continue;
      
      if (tmpi == 1 && ch == ' ')
	continue;

      if (preformatted_line)
	{
	  line_type = PREFORMATTED_TEXT_LINE;
	  if (line_start && ch == '`')
	    {
	      preformatted_tmp++;
	      
	      if (preformatted_tmp > 2)
		preformatted_line = false;

	      line_type = PREFORMATTED_TOGGLE_LINE;
	      
	      goto end;
	    }
	}
      else
	{
	  if (line_start)
	    {
	      switch (ch)
		{
		case '`':
		  preformatted_tmp++;
		  
		  if (preformatted_tmp > 2)
		    preformatted_line = !preformatted_line;

		  line_type = PREFORMATTED_TOGGLE_LINE;
		  
		  goto end;
		  
		case '>':
		  line_start = false;
		  line_type = QUOTE_LINE;
		  break;
		  
		case '*':
		  line_start = false;
		  line_type = LIST_LINE;
		  
		  printf("•");
		  goto end;
		  
		  break;
		  
		case '#':
		  if (line_type < HEADING_LINE)
		    line_type = HEADING_LINE;
		  else
		    if (line_type < SUBSUBHEADING_LINE)
		      line_type++;
		  goto end;
		  
		  break;
		  
		case '\n':
		  break;
		  
		default: 
		  if (line_type >= HEADING_LINE && line_type <= SUBSUBHEADING_LINE)
		    print_heading = true;
		  
		  line_start = false;
		}
	    }
	  
	  switch (line_type)
	    {
	    case NORMAL_LINE:
	    case LIST_LINE:
	      break;
	    case LINK_LINE:
	      break;
	    case PREFORMATTED_TOGGLE_LINE:
	      goto end;
	      break;
	    case PREFORMATTED_TEXT_LINE:
	      printf("\033[93m");
	      break;
	    case HEADING_LINE:
	      printf("\033[91;1m");
	      break;
	    case SUBHEADING_LINE:
	      printf("\033[92;1m");
	      break;
	    case SUBSUBHEADING_LINE:
	      printf("\033[96;1m");
	      break;
	    case QUOTE_LINE:
	      printf("\033[32m");
	      break;
	    }
	  
	  if (print_heading)
	    {
	      putchar('#');
	      print_heading = false;
	    }
	}

      putchar(ch);
      fflush(stdout);

      end:; /* Skip printing character */
    }

  return reached_end;
}
