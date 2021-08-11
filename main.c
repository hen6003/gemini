#include <mbedtls/platform.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string.h>

#include "url_parser.h"
#include "term.h"
#include "net.h"


void strpre(char* s, const char* t)
{
    size_t len = strlen(t);
    memmove(s + len, s, strlen(s) + 1);
    memcpy(s, t, len);
}

void setup_request(struct parsed_url *url, char *get_request)
{
  char *scheme;
  char port[10];
  char *path;
  char *query;

  if (!strcmp(url->scheme, ""))
    scheme = "gemini";
  else
    scheme = url->scheme;

  if (url->port)
    {
      strcpy(port, ":");
      strcat(port, url->port);
    }
  else
    strcpy(port, "");

  if (url->path)
    path = url->path;
  else
    path = "";

  if (url->query)
    {
      query = malloc(strlen(url->query)+1);
      strcpy(query, "?");
      strcat(query, url->query);
    }
  else
    {
      query = malloc(1);
      strcpy(query, "");
    }
  
  sprintf(get_request, "%s://%s%s/%s%s\r\n", scheme, url->host, port, path, query);
  free(query);
}

void parse_input_url(char *get_request, char *server_name, char *server_port)
{
  if (!(strstr(get_request, "://")))
    strpre(get_request, "://");

  struct parsed_url *url;
  url = parse_url(get_request);

  setup_request(url, get_request);

  /* Setup server details */

  strcpy(server_name, url->host);

  if (url->port)  
    strcpy(server_port, url->port);
  else
    strcpy(server_port, "1965");

  parsed_url_free(url);
}

int main(int argc, char **argv)
{
  int exit_code = 0;
  
  mbedtls_net_context server_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
  char *pers = "gemini_client";
  char certs_path[] = "./certs";
  
  bool isRunning = true;
  struct winsize ws;
  char command[100] = "";
  char error_msg[100] = "";
  unsigned long i;
  int start_line=0;
  static struct termios oldt;
  bool newRequest = true;

  char *server_name = malloc(255);
  char *server_port = malloc(10);
  char *get_request = malloc(1024);

  /*** INIT ***/

  /* Args */ 
  if (argc > 1)
    strcpy(get_request, argv[1]);
  else
    exit(1);

  parse_input_url(get_request, server_name, server_port);

  /* Net */
  init_session(&server_fd, &entropy, &ctr_drbg, &conf, &cacert);
  init_rng(&entropy, &ctr_drbg, pers);
  load_tofu_certs(&cacert, certs_path);
  
  /* Term */ 
  
  oldt = setup_term();

  /*** Running ***/

  int buflen = 1;
  char *buf = malloc(buflen);

  while(isRunning == true)
    {
      /* Screensize */
      ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
      
      /* Clear screen */
      printf("\033[H\033[2J\033[3J");
      fflush(stdout);
      
      /* Clears the keyboard buffer */
      fflush(stdin);

      /* Send recive requests */
      if (newRequest)
	{
	  parse_input_url(get_request, server_name, server_port);
	  open_conn(&server_fd, server_name, server_port);
	  config(&server_fd, &ctr_drbg, &ssl, &conf, &cacert, server_name);
	  check_cert(&ssl, certs_path, server_name);
	  handshake(&ssl);
	  request(&ssl, get_request);
	  buf = read_response(&ssl, buf, &buflen); 
	  close_conn(&ssl);

	  newRequest = false;
	}

      /* Print text */
      bool reached_end;
      reached_end = print_text(buf, strlen(buf), ws, start_line);

      /* Set cursor to bottom and display command */

      char *display_text;

      if (error_msg[0] != 0)
	display_text = error_msg;
      else
	display_text = command;

      printf("\033[%d;H%s", ws.ws_row, display_text);
      fflush(stdout);

      /* Clear error message */
      memset(error_msg, 0, sizeof(error_msg));

      /* Get character */
      char *token;
      switch (parse_input(getchar(), command))
	{
	case 0:
	  break;
	case 1:
	  token = strtok(command, " ");

	  if (!strcmp(token, ":quit"))
	    isRunning = false;

	  if (!strcmp(token, ":down"))
	    if (!reached_end)
	      start_line++;

	  if (!strcmp(token, ":up"))
	    if (start_line > 0)
	      start_line--;

	  if (!strcmp(token, ":open"))
	    {
	      token = strtok(NULL, " ");
	      if (token != NULL)
		{
		  strcpy(get_request, token);
		  newRequest = true;
		}
	      else	
		strcpy(error_msg, "Invalid URL");
	    }

	  /* Reset command */
	  for (i = 0; i < sizeof(command); i++)
	    command[i] = 0x0;
 
	  break;
	}
    }
  free(buf);

  /*** EXIT ***/

  /* Free */
  free_session(&server_fd, &entropy, &ctr_drbg, &conf, &cacert);
  free(server_name);
  free(server_port);
  free(get_request);

  /* Term */
  reset_term(oldt);

  if(exit_code != MBEDTLS_EXIT_SUCCESS)
    {
      char error_buf[100];
      mbedtls_strerror(exit_code, error_buf, 100);
      printf("Last error was: %d - %s\n\n", exit_code, error_buf);
    }

  return exit_code;
}
