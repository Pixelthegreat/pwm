/* window manager for pwm */
#ifndef PWM_WM_H
#define PWM_WM_H

#include <X11/Xlib.h>

/* window system */
typedef struct _pwm_wm {
	Display *disp; /* display that x server is accessing */
	Window root; /* main (root) window */
} wm;

/* initialise wm */
extern void wmInit(wm *);

/* close window manager */
extern void wmClose(wm *);

/* run wm */
extern void wmRun(wm *);

#endif /* PWM_WM_H */
