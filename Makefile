all: myhttp http_test
LIBS = -pthread
myhttp: myhttp.c
	gcc -g -W -Wall ${LIBS} -o $@ $<
http_test: http_test.c
	gcc -g -W -Wall -o $@ $<
clean:
	rm myhttp http_test