#include <stdbool.h>
#include <sys/ioctl.h>
#include <termios.h>

struct termios setup_term();
void reset_term(struct termios oldt);
int parse_input(char input, char *command);
bool print_text(char *buf, unsigned long buflen, struct winsize ws, int start_line);
