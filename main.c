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
  
  if (url->scheme[0] == 0)
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

struct response
{
  int status;
  char meta[1025];
  char *body;
};

struct response *read_response_header(char *buf)
{
  char status[3] = "";
  bool in_meta = false;
  
  struct response *resp = malloc(sizeof(struct response));
  
  int i=0;
  
  do
  {
    if (buf[0] == 0) /* malformed response */
    {
      strcpy(status, "00");
      break;
    }
    
    if (!in_meta)
    {
      if (buf[0] == ' ')
      {
	in_meta = true;
	i=0;
      }
      else
      {
	if (strlen(status) < 2)
	{
	  status[i] = buf[0];
	  i++;
	}
	else
	  strcpy(status, "00");
      }
    }
    else
    {
      if (strlen(resp->meta) < 1024)
      {
	resp->meta[i] = buf[0];
	i++;
      }
      else
	break;
    }
  }
  while (buf++[0] != '\r');
  if (buf[0] != '\n')
    strcpy(status, "00");
  else
    buf++;
  
  resp->meta[i-1] = 0x0;
  resp->status = (int) strtol(status, NULL, 10);
  
  if (status[0] == '2')
    resp->body = buf;
  else
    resp->body = NULL;
  
  return resp;
}

void free_response(struct response *resp)
{
  free(resp);
}

void parse_input_url(char *get_request, char *server_name, char *server_port, char *scheme)
{
  char *tmp;

  if (!strncmp(get_request, "about:", 6))
  {
    memmove(get_request, get_request+6, strlen(get_request+6));
    get_request[strlen(get_request+6)] = 0;
    strcpy(scheme, "about");

    return;
  }
  else if (!(tmp = strstr(get_request, "://")))
    strpre(get_request, "://");
  else if (!strncmp(get_request, "file", 4))
  {
    memmove(get_request, get_request+7, strlen(get_request+7));
    get_request[strlen(get_request+7)] = 0;
    strcpy(scheme, "file");

    return;
  }
    
  
  struct parsed_url *url;
  url = parse_url(get_request);
  
  strcpy(scheme, url->scheme);
  
  setup_request(url, get_request);
  
  /* Setup server details */ 
  strcpy(server_name, url->host);
    
  if (url->port)  
    strcpy(server_port, url->port);
  else
    strcpy(server_port, "1965");
  
  parsed_url_free(url);
}

char *read_file(char *buf, char *file_name)
{
  char ch;
  int size;
  int i = 0;

  FILE *fp = fopen(file_name, "r");
   
  if(fp != NULL)
  {
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buf = realloc(buf, size+1);
    
    while((ch = getc(fp)) != EOF)
      buf[i++] = ch;
    
    fclose(fp);
    buf[i] = 0;
  }
  else
  {
    buf = realloc(buf, 20);

    strcpy(buf, "File not found");
 }
  
  return buf;
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
  
  bool is_running = true;
  struct winsize ws;
  char command[100] = "";
  char error_msg[100] = "";
  unsigned long i;
  int start_line=0;
  static struct termios oldt;
  bool new_request = true;
  
  char server_name[255];
  char server_port[10];
  char get_request[1025];
  char scheme[100];
  
  /*** INIT ***/
  
  /* Args */ 
  if (argc > 1)
    strcpy(get_request, argv[1]);
  else
    strcpy(get_request, "about:newtab");
  
  /* Net */
  init_session(&server_fd, &entropy, &ctr_drbg, &conf, &cacert);
  init_rng(&entropy, &ctr_drbg, pers);
  load_tofu_certs(&cacert, certs_path);
  
  /* Term */ 
  
  oldt = setup_term();
  show_cursor(false);
  
  /*** Running ***/
  
  int buflen = 1;
  char *buf = malloc(buflen);
  struct response *resp;
  
  while(is_running == true)
  {
    /* Screensize */
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    
    /* Hide cursor */
    show_cursor(false);
    
    /* Clear screen */
    fputs("\e[H\e[2J\e[3J", stdout);
    fflush(stdout);
    
    /* Clears the keyboard buffer */
    fflush(stdin);
    
    fputs("Loading...\n", stdout);
    /* Send recive requests */
    if (new_request)
    {
    request:
      parse_input_url(get_request, server_name, server_port, scheme);
      
      if (!strcmp(scheme, "gemini") || scheme[0] == 0)
      {
	open_conn(&server_fd, server_name, server_port);
	config(&server_fd, &ctr_drbg, &ssl, &conf, &cacert, server_name);
	check_cert(&ssl, &cacert, certs_path, server_name);
	handshake(&ssl);
	request(&ssl, get_request);
	buf = read_response(&ssl, buf, &buflen); 
	close_conn(&ssl);
	
	free_response(resp);
	resp = read_response_header(buf);
      }
      else if (!strcmp(scheme, "file"))
	buf = read_file(buf, get_request);
      else if (!strcmp(scheme, "about"))
      {
	strpre(get_request, "built-in/");
	strcat(get_request, ".gmi");
	buf = read_file(buf, get_request);
      }
      
      new_request = false;
    }
    
    fputs("\e[H\e[2J\e[3J", stdout);
    
    bool reached_end = false;
    if (!strcmp(scheme, "gemini") || scheme[0] == 0)
    {
      char error_text[20] = "";
      
      switch (resp->status)
      {
      case 0:  /* Internal error */
	strcpy(error_text, "Internal");
	goto error;
      case 10: /* Input */
      case 11: /* Sensitive Input */
	break;
      case 20: /* Print text */
	reached_end = print_text(resp->body, ws, start_line, true);
	break;
      case 30: /* Redirect temporary */
      case 31: /* Redirect permanent */
	strcpy(get_request, resp->meta);
	goto request;
	break;
      case 40: /* Errors */
	strcpy(error_text, "Temporary failure");
	goto server_error;
      case 41:
	strcpy(error_text, "Server unavailable");
	goto server_error;
      case 42:
	strcpy(error_text, "CGI error");
	goto server_error;
      case 43:
	strcpy(error_text, "Proxy error");
	goto server_error;
      case 44:
	strcpy(error_text, "Slow down");
	goto server_error;
      case 50:
	strcpy(error_text, "Permanent failure");
	goto server_error;
      case 51:
	strcpy(error_text, "Not found");
	goto server_error;
      case 52:
	strcpy(error_text, "Gone");
	goto server_error;
      case 53:
	strcpy(error_text, "Proxy refused");
	goto server_error;
      case 59:
	strcpy(error_text, "Bad request");
	goto server_error;
      case 60:
	strcpy(error_text, "Cert required");
	goto server_error;
      case 61:
	strcpy(error_text, "Cert not allowed");
	goto server_error;
      case 62:
	strcpy(error_text, "Cert not valid");
	goto server_error;
      default:
	strcpy(error_text, "Unknown status");
	
	/* Print errors */
      error:
	printf("CLIENT ERROR: %s", error_text);
	fflush(stdout);
	break;
      server_error:
	printf("SERVER ERROR: %s: \"%s\"", error_text, resp->meta);
	fflush(stdout);
	break;
      }
    }
    else if (!strcmp(scheme, "file"))
    {
      bool is_gmi = false;

      if (!strcmp(get_request+strlen(get_request)-3, "gmi"))
	is_gmi = true;

      reached_end = print_text(buf, ws, start_line, is_gmi);
    }
    else if (!strcmp(scheme, "about"))
      reached_end = print_text(buf, ws, start_line, true);
    
    /* Set cursor to bottom and display command */
    
    char *display_text;
    
    if (error_msg[0] != 0)
      display_text = error_msg;
    else
    {
      display_text = command;
      /* Show cursor */
      show_cursor(true);
    }
    
    printf("\e[%d;H%s", ws.ws_row, display_text);
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
	is_running = false;
      
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
	  new_request = true;
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
  
  /* Term */
  reset_term(oldt);
  show_cursor(true);
  
  if(exit_code != MBEDTLS_EXIT_SUCCESS)
  {
    char error_buf[100];
    mbedtls_strerror(exit_code, error_buf, 100);
    printf("Last error was: %d - %s\n\n", exit_code, error_buf);
  }
  
  return exit_code;
}
