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

bool print_text(char *buf, unsigned long buflen, struct winsize ws, int start_line) 
{
  char ch;
  int tmpi=0, line=0, newline = 0;
  bool reached_end = true;

  for (unsigned long i = 0; i < buflen; i++)
    {
      ch = buf[i];
      tmpi++;
      
      if (ch == '\n')
	newline = 1;
      
      if (tmpi > ws.ws_col-1)
	{
	  if (line >= start_line)
	    putchar(ch);
	  newline = 1;
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
	  newline=0;
	}
      
      if (line < start_line-1)
	continue;
      
      if (tmpi == 1 && ch == ' ')
	continue;
      
      putchar(ch);
    }

  return reached_end;
}
