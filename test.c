#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(void)
{
  //Variable used for reading the user input
  char option;
  //Variable used for controlling the while loop
  bool isRunning = true;
  struct winsize ws;
  
  while(isRunning == true)
    {
      /* Screensize */
      ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); 
      
      //Clears the screen
      puts("\033[H\033[2J");
      
      //Clears the keyboard buffer
      fflush(stdin);
      
      //Outputs the options to console
      puts("\n[1]Option1"
	   "\n[x]Exit");
      
      //Reads the user's option
      printf("\033[%d;%dH\n", ws.ws_row, ws.ws_col);
      option = getchar();

      //Selects the course of action specified by the option
      switch(option)
        {
	case '1':
	  //TO DO CODE
	  break;
	case '2':
	  //TO DO CODE
	  break;
	case '3':
	  //TO DO CODE
	  break;
	case '4':
	  //TO DO CODE
	  break;
	  //...
	case 'q':
	  //Exits the system
	  isRunning = false;
	  return 0;
	default:
	  //User enters wrong input
	  //TO DO CODE
	  break;
        }
    }
  return 0;
}
