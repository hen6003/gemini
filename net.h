#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/base64.h>

void init_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert);

int init_rng(mbedtls_entropy_context *entropy, mbedtls_ctr_drbg_context *ctr_drbg, char *pers);

int load_tofu_certs(mbedtls_x509_crt *cacert, char *certs_path);

int open_conn(mbedtls_net_context *server_fd, char *server_name, char *server_port);

int config(mbedtls_net_context *server_fd,
	   mbedtls_ctr_drbg_context *ctr_drbg,
	   mbedtls_ssl_context *ssl,
	   mbedtls_ssl_config *conf,
	   mbedtls_x509_crt *cacert,
	   char *server_name);

int check_cert(mbedtls_ssl_context *ssl, mbedtls_x509_crt *cacert,
	       char *certs_path, char *server_name);

int handshake(mbedtls_ssl_context *ssl);

int request(mbedtls_ssl_context *ssl, char *request);

char *read_response(mbedtls_ssl_context *ssl, char *buf, int *buflen);

void close_conn(mbedtls_ssl_context *ssl);

void free_session(mbedtls_net_context *server_fd,
		  mbedtls_entropy_context *entropy,
		  mbedtls_ctr_drbg_context *ctr_drbg,
		  mbedtls_ssl_config *conf,
		  mbedtls_x509_crt *cacert);
