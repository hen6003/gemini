LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
OBJS += main.o url_parser.o term.o net.o
CFLAGS += -Wall

COMMIT = `git rev-parse HEAD`

all: gemini

gemini: ${OBJS} version.gmi
	clang ${OBJS} -o $@ $(LIBS) $(CFLAGS)

version.gmi:
	echo "# Version" > built-in/$@
	echo "" >> built-in/$@
	echo "* Commit: "$(COMMIT) >> built-in/$@

clean:
	rm -f *.o gemini

force: clean gemini

debug:
	CFLAGS="-g -O0" ${MAKE} force

.PHONY: version.gmi clean debug force
