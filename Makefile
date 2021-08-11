LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
OBJS += main.o url_parser.o term.o net.o

gemini: ${OBJS}
	clang ${OBJS} -o $@ $(LIBS) $(CFLAGS) 
