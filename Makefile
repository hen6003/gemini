LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
OBJS += main.o

gemini: ${OBJS}
	clang ${OBJS} -o $@ $(LIBS) $(CFLAGS) 
