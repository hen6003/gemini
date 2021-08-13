LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
OBJS += main.o url_parser.o term.o net.o
CFLAGS += -Wall

gemini: ${OBJS}
	clang ${OBJS} -o $@ $(LIBS) $(CFLAGS) 

debug:
	CFLAGS="-g -O0" ${MAKE}
