
all: jpvtx

jpvtx: jpvtx.c tables.h
	gcc -Wall -o jpvtx jpvtx.c

clean:
	rm -f  *.o *~ jpvtx

install:
	cp jpvtx /usr/local/bin/

