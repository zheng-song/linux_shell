CC=gcc
AR=$(CC)ar

CCFLAGS=-Wall -I/usr/include/SDL -D_REENTRANT -I/usr/lib -L/usr/lib \
-Wl,-rpath,/usr/lib -lSDL -lpthread /usr/lib/libSDL_draw.a -lSDL_draw  -lm

sdl:sdl.c
	$(CC) $(CCFLAGS) -o sdl sdl.c

clean:
	-rm -f sdl