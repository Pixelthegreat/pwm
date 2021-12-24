CFLAGS=-g -c -Iinclude/
LDFLAGS=-g -lX11

all: pwm

# main executable #
pwm: obj/main.o obj/wm.o
	cc -o bin/pwm obj/main.o obj/wm.o $(LDFLAGS)

# main object #
obj/main.o: src/main.c
	cc src/main.c -o obj/main.o $(CFLAGS)

# window manager #
obj/wm.o: src/wm.c include/wm.h
	cc src/wm.c -o obj/wm.o $(CFLAGS)

# clean files #
clean:
	rm obj/* bin/pwm

# test run #
testrun:
	sudo xinit ./xinitrc -- "/usr/bin/Xephyr" :100 -ac -screen 640x480 -host-cursor
