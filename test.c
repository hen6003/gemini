#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

struct termios setup_term()
{
  static struct termios oldt, newt;

/* Alternative Buffer */
  puts("\033[?1049h");

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
  puts("\033[?1049l");
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

int main(void)
{
  bool isRunning = true;
  struct winsize ws;
  char command[100] = "";
  unsigned long i;
  int start_line=0;
  static struct termios oldt;
  
  oldt = setup_term();

  char buf[] = "What is Lorem Ipsum?\n\nLorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.\nWhy do we use it?\n\nIt is a long established fact that a reader will be distracted by the readable content of a page when looking at its layout. The point of using Lorem Ipsum is that it has a more-or-less normal distribution of letters, as opposed to using 'Content here, content here', making it look like readable English. Many desktop publishing packages and web page editors now use Lorem Ipsum as their default model text, and a search for 'lorem ipsum' will uncover many web sites still in their infancy. Various versions have evolved over the years, sometimes by accident, sometimes on purpose (injected humour and the like).\n\nWhere does it come from?\n\nContrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of \"de Finibus Bonorum et Malorum\" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, \"Lorem ipsum dolor sit amet..\", comes from a line in section 1.10.32.\n\nThe standard chunk of Lorem Ipsum used since the 1500s is reproduced below for those interested. Sections 1.10.32 and 1.10.33 from \"de Finibus Bonorum et Malorum\" by Cicero are also reproduced in their exact original form, accompanied by English versions from the 1914 translation by H. Rackham.\nWhere can I get some?\n\nThere are many variations of passages of Lorem Ipsum available, but the majority have suffered alteration in some form, by injected humour, or randomised words which don't look even slightly believable. If you are going to use a passage of Lorem Ipsum, you need to be sure there isn't anything embarrassing hidden in the middle of text. All the Lorem Ipsum generators on the Internet tend to repeat predefined chunks as necessary, making this the first true generator on the Internet. It uses a dictionary of over 200 Latin words, combined with a handful of model sentence structures, to generate Lorem Ipsum which looks reasonable. The generated Lorem Ipsum is therefore always free from repetition, injected humour, or non-characteristic words etc.\n";

  while(isRunning == true)
    {
      /* Screensize */
      ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
      
      /* Clear screen */
      printf("\033[H\033[2J\033[3J");
      fflush(stdout);
      
      //Clears the keyboard buffer
      fflush(stdin);

      int tmpi=0, line=0, newline = 0;
      bool reached_end=true;
      char ch;
      /* Print buf */
      for (i = 0; i < strlen(buf); i++)
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

      /* Set cursor to bottom and display command */
      printf("\033[%d;H%s", ws.ws_row, command);
      fflush(stdout);

      /* Get character */
      switch (parse_input(getchar(), command))
	{
	case 0:
	  break;
	case 1:
	  if (!strcmp(command, ":quit"))
	    isRunning = false;

	  if (!strcmp(command, ":down"))
	    if (!reached_end)
	      start_line++;

	  if (!strcmp(command, ":up"))
	    if (start_line > 0)
	      start_line--;

	  /* Reset command */
	  for (i = 0; i < strlen(command); i++)
	    command[i] = 0x0;
 
	  //isRunning = false;
	  break;
	}
    }

  reset_term(oldt);

  return 0;
}
