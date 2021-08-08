#include <mbedtls/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/base64.h>

#include "url_parser.h"

static void my_debug(void *ctx, int level,
                      const char *file, int line,
                      const char *str)
{
  ((void)level);
  
  mbedtls_fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
  fflush( (FILE *)ctx );
}

void init_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_context *ssl,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert)
{
  mbedtls_net_init(server_fd);
  mbedtls_ctr_drbg_init(ctr_drbg);
  mbedtls_ssl_init(ssl);
  mbedtls_ssl_config_init(conf);
  mbedtls_x509_crt_init(cacert);
  mbedtls_entropy_init(entropy);
}

int init_rng(mbedtls_entropy_context *entropy, mbedtls_ctr_drbg_context *ctr_drbg, char *pers)
{
  int ret = 0;

  if((ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy,
				  (unsigned char *) pers, strlen(pers)))!= 0)
    printf("Seeding the random number generator failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
  
  return ret;
}

int load_tofu_certs(mbedtls_x509_crt *cacert, char *certs_path)
{
  int ret;
  
  /* print all the files and directories within directory */
  if ((ret = mbedtls_x509_crt_parse_path(cacert, certs_path))!=0)
      printf("Loading the certificates from '%s' failed\n"
	     "  !  mbedtls_x509_crt_parse_path returned -0x%x\n\n", certs_path, (unsigned int)-ret);

  return ret;
}

int net_connect(mbedtls_net_context *server_fd, char *server_name, char *server_port)
{
  int ret;

  if((ret = mbedtls_net_connect(server_fd, server_name,
				server_port, MBEDTLS_NET_PROTO_TCP))!= 0)
      printf("Connecting to tcp failed\n  ! mbedtls_net_connect returned %d\n\n", ret);

  return ret;
}

int config(mbedtls_net_context *server_fd,
	   mbedtls_ctr_drbg_context *ctr_drbg,
	   mbedtls_ssl_context *ssl,
	   mbedtls_ssl_config *conf,
	   mbedtls_x509_crt *cacert,
	   char *server_name)
{
  int ret;

  if((ret = mbedtls_ssl_config_defaults(conf,
					MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
      goto exit;
    } 
  
  /* OPTIONAL is not optimal for security,
   * but makes interop easier in this simplified example */
  mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_ca_chain(conf, cacert, NULL);
  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);
  mbedtls_ssl_conf_dbg(conf, my_debug, stdout);
  
  if((ret = mbedtls_ssl_setup(ssl, conf))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
      goto exit;
    }

  if((ret = mbedtls_ssl_set_hostname(ssl, server_name))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
      goto exit;
    }

  mbedtls_ssl_conf_read_timeout(conf, 10000);
  mbedtls_ssl_set_bio(ssl, server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

  exit:
  return ret;
}

int check_cert(mbedtls_ssl_context *ssl, char *certs_path, char *server_name)
{
  uint32_t ret;
  
  if((ret = mbedtls_ssl_get_verify_result(ssl))!= 0)
    {
      mbedtls_x509_crt peer_cert;
      FILE *fp;
      char buf[512];

      sprintf(buf, "%s/%s.crt", certs_path, server_name);

      /* Check if PEM exists */
      if (access(buf, F_OK) != -1) {
	/* If it does and it didn't validate, exit */

        mbedtls_printf("Verifying peer X.509 certificate failed\n");
	
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", ret);

        mbedtls_printf("%s\n", buf);
      } else {
	/* Save cert if it didn't exist */
	peer_cert = *mbedtls_ssl_get_peer_cert(ssl);
	fp = fopen(buf, "w"); 
	
	for (int i = 0; (unsigned long) i < peer_cert.raw.len; i++)
	  fprintf(fp, "%c", peer_cert.raw.p[i]);
	
	fclose(fp);
      }
    }

  return ret;
}

int handshake(mbedtls_ssl_context *ssl)
{
  int ret;

  while((ret = mbedtls_ssl_handshake(ssl))!= 0)
    {
      if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
	  printf("Performing the SSL/TLS handshake failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int)-ret);
	  break;
        }
    }

  return ret;
}

int request(mbedtls_ssl_context *ssl, char *request)
{
  int ret;

  while((ret = mbedtls_ssl_write(ssl, (unsigned char *) request, strlen(request)+1))<= 0)
    {
      if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
	  printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
	  break;
        }
    }
  
  return ret;
}

int read_response(mbedtls_ssl_context *ssl)
{
  int len, ret;
  char buf[1024];
  do
    {
      len = sizeof(buf)- 1;
      memset(buf, 0, sizeof(buf));
      ret = mbedtls_ssl_read(ssl, (unsigned char *) buf, len);
      
      if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
	continue;
      
      if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
	break;
      
      if(ret < 0)
        {
	  printf("read failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
	  break;
        }
      
      if(ret == 0)
        {
	  printf("\n\nEOF\n\n");
	  break;
        }
      
      len = ret;
      printf("%s", (char *)buf);
    }
  while(1);

  return ret;
}

void close_conn(mbedtls_ssl_context *ssl)
{
  mbedtls_ssl_close_notify(ssl);
}

void free_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_context *ssl,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert)
{
  mbedtls_net_free(server_fd); 
  mbedtls_x509_crt_free(cacert);
  mbedtls_ssl_free(ssl);
  mbedtls_ssl_config_free(conf);
  mbedtls_ctr_drbg_free(ctr_drbg);
  mbedtls_entropy_free(entropy);
}

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
 
  char *server_name = malloc(255);
  char *server_port = malloc(10);
  char *get_request = malloc(1024);

  if (argc > 1)
    strcpy(get_request, argv[1]);
  else
    exit(1);

  /* Parse url */
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
  
  /* Connect */
  init_session(&server_fd, &entropy, &ctr_drbg, &ssl, &conf, &cacert);
  init_rng(&entropy, &ctr_drbg, pers);
  load_tofu_certs(&cacert, certs_path);
  net_connect(&server_fd, server_name, server_port);
  config(&server_fd, &ctr_drbg, &ssl, &conf, &cacert, server_name);
  check_cert(&ssl, certs_path, server_name);
  handshake(&ssl);
  request(&ssl, get_request);
  read_response(&ssl);
  close_conn(&ssl);
  free_session(&server_fd, &entropy, &ctr_drbg, &ssl, &conf, &cacert);
  free(server_name);
  free(server_port);

  if(exit_code != MBEDTLS_EXIT_SUCCESS)
    {
      char error_buf[100];
      mbedtls_strerror(exit_code, error_buf, 100);
      printf("Last error was: %d - %s\n\n", exit_code, error_buf);
    }

  return exit_code;
}
