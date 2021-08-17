#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "term.h"

struct termios setup_term()
{
  static struct termios oldt, newt;
  
  /* Alternative Buffer */
  fputs("\e[?1049h", stdout);
  
  /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
  tcgetattr(STDIN_FILENO, &oldt);
  /*now the settings will be copied*/
  newt = oldt;
  
  /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
  newt.c_lflag &= ~(ICANON|ECHO);
  
  newt.c_lflag &= ~(IGNBRK); /* Pass though ^C */
  
  newt.c_cc[VTIME]=0;
  newt.c_cc[VMIN]=1;
  
  newt.c_cflag &= ~(CBAUDEX);
  
  cfsetispeed(&newt, B230400);
  cfsetospeed(&newt, B230400);
  
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
  fputs("\e[?1049l", stdout);
}

void show_cursor(bool show)
{
  if (show)
    fputs("\e[?25h", stdout);
  else
    fputs("\e[?25l", stdout);
  fflush(stdout);
}

int parse_input(char input, char *command)
{
  if (command[0] != ':')
  {  
    switch(input)
    {
    case 'q':
    case 3: /* ^C */
      strcat(command, ":quit"); 
      break;
      
    case 'j':
      strcat(command, ":down"); 
      break;
      
    case 'k':
      strcat(command, ":up"); 
      break;

    case '?':
      strcat(command, ":help"); 
      break;

    case 'o':
      strcat(command, ":open "); 
      return 0;
      
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
    case '\n': /* Run command */
      return 1;
      break;
    case '\e': /* Clear on ESC */
      memset(command, 0, strlen(command));
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
  LINK_LINE_LINK,
  LINK_LINE_SPACE,
  LINK_LINE_DESC,
  PREFORMATTED_TOGGLE_LINE,
  PREFORMATTED_TEXT_LINE,
  HEADING_LINE,
  SUBHEADING_LINE,
  SUBSUBHEADING_LINE,
  LIST_LINE,
  QUOTE_LINE,
};

struct print_info print_text(char *buf, struct winsize ws,
			     int start_line, bool gemini) 
{
  char ch;
  int tmpi=0, line=0, preformatted_tmp=0, links_len=0, linki=0;
  bool reached_end = true;
  bool line_start = true;
  bool newline = false;
  bool preformatted_line = true;
  enum line_types line_type = NORMAL_LINE;

  char **links = {NULL};
  
  for (unsigned long i = 0; i < strlen(buf); i++)
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

      if (line_type != PREFORMATTED_TOGGLE_LINE)
	line++;

      tmpi=0;
      newline = false;
      line_start = true;
      preformatted_tmp = 0;
      fputs("\e[39;49;22;23;24;25m", stdout); /* Reset styling */
      fflush(stdout);

      if (linki)
      {
	if (line_type == LINK_LINE_LINK)
	  fputs(links[links_len], stdout);
	
	links[links_len][linki] = 0;
	linki=0;
	links_len++;
      }
      
      if (line_type == PREFORMATTED_TOGGLE_LINE)
      {
	line_type = NORMAL_LINE;
	continue;
      }
      else
	line_type = NORMAL_LINE;
    }

    if (gemini) /* Gemini parsing */
    {
      if (line_start && ch == '`')
      {
	preformatted_tmp++;
	
	if (preformatted_tmp > 2)
	  preformatted_line = !preformatted_line;
	  
	line_type = PREFORMATTED_TOGGLE_LINE;
	
	continue;
      }
    }
    
    if (line < start_line)
      continue;
    
    if (tmpi == 1 && ch == ' ')
      continue;
    
    if (gemini && preformatted_line)
      {
	if (line_start)
	{
	  switch (ch)
	  {   
	  case '>':
	    if (line_type != LINK_LINE)
	      line_type = QUOTE_LINE;
	    else
	    {
	      printf("(\e[5m%d\e[25m) ", links_len);
	      links = realloc(links, (links_len+1)*sizeof(char*));
	      links[links_len] = malloc(1025);
	    }

	    line_start = false;
	    break;
	    
	  case '*':
	    line_start = false;
	    line_type = LIST_LINE;
	    
	    fputs(" •", stdout);
	    continue;
	    
	    break;
	    
	  case '#':
	    if (line_type < HEADING_LINE)
	      line_type = HEADING_LINE;
	    else
	      if (line_type < SUBSUBHEADING_LINE)
		line_type++;
	    continue;
	    
	    break;

	  case '=':
	    line_type = LINK_LINE;
	    break;
	    
	  case '\n':
	    break;
	    
	  case ' ':
	    if (line_type >= HEADING_LINE && line_type <= SUBSUBHEADING_LINE)
	      continue;
	    break;
	    
	  default: 
	    if (line_type == LINK_LINE)
	      line_type = NORMAL_LINE;

	    line_start = false;
	  }
	}

	switch (line_type)
	{
	case NORMAL_LINE:
	case PREFORMATTED_TEXT_LINE:
	case LIST_LINE: /* To make compiler shut up */
	  break;
	case LINK_LINE:
	  if (ch == ' ')
	    line_type++;
	  continue;
	  break;
	case LINK_LINE_LINK:
	  if (isspace(ch))
	    line_type++;
	  else
	    links[links_len][linki++] = ch;
	  continue;
	  break;
	case LINK_LINE_SPACE:
	  if (isspace(ch))
	    continue;
	  else
	    line_type++;
	case LINK_LINE_DESC:
	  break;
	case PREFORMATTED_TOGGLE_LINE:
	  continue; 
	  break;
	case HEADING_LINE:
	  fputs("\e[1;4m", stdout);
	  break;
	case SUBHEADING_LINE:
	  fputs("\e[1m", stdout);
	  break;
	case SUBSUBHEADING_LINE:
	  fputs("\e[4m", stdout);
	  break;
	case QUOTE_LINE:
	  fputs("\e[3m", stdout);
	  break;
	}
    }
    
    putchar(ch);
    fflush(stdout);
  }

  struct print_info ret;
  ret.reached_end = reached_end;
  ret.links = links;
  ret.links_len = links_len;
  
  return ret;
}

void free_info(struct print_info pinfo)
{
  if (pinfo.links != NULL)
  {
    for (int i=0; i < pinfo.links_len; i++)
      free(pinfo.links[i]);
    free(pinfo.links);
  }
}
