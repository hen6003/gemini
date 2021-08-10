LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
OBJS += main.o url_parser.o term.c

gemini: ${OBJS}
	clang ${OBJS} -o $@ $(LIBS) $(CFLAGS) 
