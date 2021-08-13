#include <string.h>
#include <unistd.h>

#include "net.h"

static void my_debug(void *ctx, int level,
		     const char *file, int line,
		     const char *str)
{
  ((void)level);
  
  fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
  fflush( (FILE *)ctx );
}

void init_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert)
{
  mbedtls_net_init(server_fd);
  mbedtls_ctr_drbg_init(ctr_drbg);
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

int open_conn(mbedtls_net_context *server_fd, char *server_name, char *server_port)
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
  
  /* Setup SSL context */
  mbedtls_ssl_init(ssl);
  
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

int check_cert(mbedtls_ssl_context *ssl, mbedtls_x509_crt *cacert, char *certs_path, char *server_name)
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
      
      printf("Verifying peer X.509 certificate failed\n");
      
      mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", ret);
      
      printf("%s\n", buf);
    } else {
      /* Save cert if it didn't exist */
      peer_cert = *mbedtls_ssl_get_peer_cert(ssl);
      fp = fopen(buf, "w"); 
      
      for (int i = 0; (unsigned long) i < peer_cert.raw.len; i++)
	fprintf(fp, "%c", peer_cert.raw.p[i]);
      
      mbedtls_x509_crt_parse_der(cacert, peer_cert.raw.p, peer_cert.raw.len);
      
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

char *read_response(mbedtls_ssl_context *ssl, char *buf, int *buflen)
{
  int len, ret;
  char tmp[1024];
  
  buf[0] = 0;
  
  do
  {
    len = sizeof(tmp)- 1;
    memset(tmp, 0, sizeof(tmp));
    ret = mbedtls_ssl_read(ssl, (unsigned char *) tmp, len);
    
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
      fputs("\n\nEOF\n\n", stdout);
      break;
    }
    
    len = ret;
    *buflen += len;
    buf = realloc(buf, *buflen);
    strcat(buf, tmp);
    buf[*buflen-1] = 0;
  }
  while(1);
  
  return buf;
}

void close_conn(mbedtls_ssl_context *ssl)
{
  mbedtls_ssl_close_notify(ssl);
  mbedtls_ssl_free(ssl);
}

void free_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert)
{
  mbedtls_net_free(server_fd); 
  mbedtls_x509_crt_free(cacert);
  mbedtls_ssl_config_free(conf);
  mbedtls_ctr_drbg_free(ctr_drbg);
  mbedtls_entropy_free(entropy);
}
