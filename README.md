# pwm
Pixel's Window Manager. I want to do something different yet again. This is mainly going to be a clone of [https://github.com/joewing/jwm](JWM), but it also contains code from the tutorial series which can be found [https://github.com/jichu4n](here).

# 11-feb-2022
I fixed some issues:
	- The Window Manager would segfault when enough windows were open
	- The WM would infinitely wait for a MapNotify and ConfigureNotify event that would never be received, forcing the WM to be completely unresponsive until killed
	- The WM would call XKillClient instead of using XSendEvent, even if the ```WM_DELETE_WINDOW``` protocol was supported on that window

# requirements to build
- the Xlib development libraries

# requirements to test/run
- xinit (something to do with x servers idk)
- Xephyr (a nested X server program which allows for an X server to be run inside of a window; used for testing)
- xterm (you can use any example app for the test app, this is just the one that I chose. You can change it by editing xinitrc)

# to build
run make.

# to test run
run "make testrun".
