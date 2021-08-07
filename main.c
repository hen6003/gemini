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

static void my_debug(void *ctx, int level,
                      const char *file, int line,
                      const char *str)
{
  ((void)level);
  
  mbedtls_fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
  fflush( (FILE *)ctx );
}

char *server_name = "gemini.circumlunar.space";
char *server_port = "1965";
char *get_request = "gemini://gemini.circumlunar.space/\r\n";

int main()
{
  int ret = 1, len;
  int exit_code = MBEDTLS_EXIT_FAILURE;
  mbedtls_net_context server_fd;
  uint32_t flags;
  unsigned char buf[1024];
  const char *pers = "gemini_client";
  
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
  
  /*
   * 0. Initialize the RNG and the session data
   */
  mbedtls_net_init(&server_fd);
  mbedtls_ssl_init(&ssl);
  mbedtls_ssl_config_init(&conf);
  mbedtls_x509_crt_init(&cacert);
  mbedtls_ctr_drbg_init(&ctr_drbg); 
  
  mbedtls_entropy_init(&entropy);
  if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
				     (const unsigned char *)pers,
				     strlen(pers)))!= 0)
    {
      printf("Seeding the random number generator failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
      goto exit;
    }
  
  /*
   * 0. Initialize certificates
   */

  /* Load cached TOFU certs */

  char certs_path[] = "./certs";

  /* print all the files and directories within directory */
  if ((ret = mbedtls_x509_crt_parse_path(&cacert, certs_path))!=0)
    {
      printf("Loading the certificates from '%s' failed\n"
	     "  !  mbedtls_x509_crt_parse_path returned -0x%x\n\n", certs_path, (unsigned int)-ret);
      goto exit;
    } 
  
  /*
   * 1. Start the connection
   */

  if((ret = mbedtls_net_connect(&server_fd, server_name,
				server_port, MBEDTLS_NET_PROTO_TCP))!= 0)
    {
      printf("Connecting to tcp failed\n  ! mbedtls_net_connect returned %d\n\n", ret);
      goto exit;
    }
  
  /*
   * 2. Setup stuff
   */

  if((ret = mbedtls_ssl_config_defaults(&conf,
					MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
      goto exit;
    } 
  
  /* OPTIONAL is not optimal for security,
   * but makes interop easier in this simplified example */
  mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
  mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
  mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
  
  if((ret = mbedtls_ssl_setup(&ssl, &conf))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
      goto exit;
    }

  if((ret = mbedtls_ssl_set_hostname(&ssl, server_name))!= 0)
    {
      printf("Setting up the SSL/TLS structure failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
      goto exit;
    }

  mbedtls_ssl_conf_read_timeout(&conf, 10000);
  mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
  
  /*
   * 4. Handshake
   */

  while((ret = mbedtls_ssl_handshake(&ssl))!= 0)
    {
      if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
	  printf("Performing the SSL/TLS handshake failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int)-ret);
	  goto exit;
        }
    } 
  
  /*
   * 5. Verify the server certificate
   */
  
  /* Check TOFU */
  if((flags = mbedtls_ssl_get_verify_result(&ssl))!= 0)
    {
      mbedtls_x509_crt peer_cert;
      FILE *fp;
      char buf[512];

      sprintf(buf, "%s/%s.crt", certs_path, server_name);

      /* Check if PEM exists */
      if (access(buf, F_OK) != -1) {
	/* If it does and it didn't validate, exit */

        mbedtls_printf("Verifying peer X.509 certificate failed\n");
	
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);

        mbedtls_printf("%s\n", buf);
	goto exit;
      } else {
	/* Save cert if it didn't exist */
	peer_cert = *mbedtls_ssl_get_peer_cert(&ssl);
	fp = fopen(buf, "w"); 
	
	for (int i = 0; (unsigned long) i < peer_cert.raw.len; i++)
	  fprintf(fp, "%c", peer_cert.raw.p[i]);
	
	fclose(fp);
      }
    }
 
  /*
   * 3. Write the GET request
   */

  while((ret = mbedtls_ssl_write(&ssl, (unsigned char *) get_request, strlen(get_request)+1))<= 0)
    {
      if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
	  printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
	  goto exit;
        }
    }
  
  len = ret;

  /*
   * 7. Read the response
   */
  
  do
    {
      len = sizeof(buf)- 1;
      memset(buf, 0, sizeof(buf));
      ret = mbedtls_ssl_read(&ssl, buf, len);
      
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
  
  mbedtls_ssl_close_notify(&ssl);
  
  exit_code = MBEDTLS_EXIT_SUCCESS;
  
exit:  
  if(exit_code != MBEDTLS_EXIT_SUCCESS)
    {
      char error_buf[100];
      mbedtls_strerror(ret, error_buf, 100);
      printf("Last error was: %d - %s\n\n", ret, error_buf);
    }
  
  mbedtls_net_free(&server_fd);
  
  mbedtls_x509_crt_free(&cacert);
  mbedtls_ssl_free(&ssl);
  mbedtls_ssl_config_free(&conf);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  
  mbedtls_exit(exit_code);
}
