#include <stdbool.h>
#include <sys/ioctl.h>
#include <termios.h>

struct print_info
{
  bool reached_end;
  char **links;
  int links_len;
};

struct termios setup_term();
void reset_term(struct termios oldt);
int parse_input(char input, char *command);
struct print_info print_text(char *buf, struct winsize ws,
			     int start_line, bool gemini);
void show_cursor(bool show);
void free_info(struct print_info pinfo);
