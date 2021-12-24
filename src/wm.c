/* window manager for pwm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm.h"

/* literally just two windows */
struct windows {
	Window w, f;
};

/* variables and forw-decls */
static int _wmError(Display *, XErrorEvent *);
static int _wmDetected(Display *, XErrorEvent *);
static void _wmCreateNotify(XCreateWindowEvent *);
static void _wmConfigureRequest(wm *, XConfigureRequestEvent *);
static void _wmMapRequest(wm *, XMapRequestEvent *);
static void _wmUnmapNotify(wm *, XUnmapEvent *);
static void _wmFrame(wm *, Window, int);
static int _wm_detected = 0;

struct windows *ws = NULL;
int wsl = 0;
int wsc = 8;

/* free ws list */
static void wsfree() {
	
	/* free list */
	free(ws);
}

/* intialise window manager */
extern void wmInit(wm *w) {

	w->disp = XOpenDisplay(NULL);
	
	/* could not open X display */
	if (w->disp == NULL) {
	
		/* exit */
		fprintf(stderr, "Failed to open X display '%s'\n", XDisplayName(NULL));
		return;
	}
	
	/* set root window */
	w->root = DefaultRootWindow(w->disp);
}

/* close window manager */
extern void wmClose(wm *w) {

	printf("closing wm\n");

	XCloseDisplay(w->disp);
}

/* run window manager */
extern void wmRun(wm *w) {

	/* setup so that we can exit if another wm is running already */
	XSetErrorHandler(_wmDetected);
	XSelectInput(
		w->disp,
		w->root,
		SubstructureRedirectMask | SubstructureNotifyMask);

	/* sync display */
	XSync(w->disp, 0);
	
	if (_wm_detected) {
	
		/* error */
		fprintf(stderr, "Detected another window manager on '%s'\n", XDisplayString(w->disp));
		return;
	}
	
	/* set our default error handler */
	XSetErrorHandler(_wmError);

	/* grab server to make sure that windows don't change until they're framed */
	XGrabServer(w->disp);
	
	/* query windows that already exist */
	Window rr, rp;
	Window *tlw;
	unsigned int ntlw;
	
	XQueryTree(w->disp,
		   w->root,
		   &rr,
		   &rp,
		   &tlw,
		   &ntlw);
	
	/* not the same root windows */
	if (rr != w->root)
		return;
	
	/* frame each top level window */
	for (unsigned int i = 0; i < ntlw; i++)
		_wmFrame(w, tlw[i], 1);
	
	/* free list and ungrab server */
	XFree(tlw);
	XUngrabServer(w->disp);
	
	/* main loop */
	int running = 1;
	while (running) {
	
		/* get next event */
		XEvent e;
		XNextEvent(w->disp, &e);
		
		/* window is created */
		if (e.type == CreateNotify) {
		
			_wmCreateNotify(&e.xcreatewindow);
		}
		/* window requests to be configured */
		if (e.type == ConfigureRequest) {
		
			_wmConfigureRequest(w, &e.xconfigurerequest);
		}
		/* window can map */
		if (e.type == MapRequest) {
		
			_wmMapRequest(w, &e.xmaprequest);
		}
		/* window is destroyed */
		else if (e.type == DestroyNotify) {}
		/* window is reparented */
		else if (e.type == ReparentNotify) {}
		/* window is mapped */
		else if (e.type == MapNotify) {}
		/* window is configured */
		else if (e.type == ConfigureNotify) {}
		/* window is unmapped */
		else if (e.type == UnmapNotify) {
		
			_wmUnmapNotify(w, &e.xunmap);
		}
	}
}

/* error handlers */
static int _wmDetected(Display *d, XErrorEvent *e) {

	if (e->error_code != BadAccess)
		return 0;
	
	_wm_detected = 1;
	return 0;
}

static int _wmError(Display *d, XErrorEvent *e) {}

/* create a window */
static void _wmCreateNotify(XCreateWindowEvent *e) {

	/* do nothing */
}

/* configure a window */
static void _wmConfigureRequest(wm *w, XConfigureRequestEvent *e) {

	/* make changes */
	XWindowChanges changes;
	changes.x = e->x;
	changes.y = e->y;
	changes.width = e->width;
	changes.height = e->height;
	changes.border_width = e->border_width;
	changes.sibling = e->above;
	changes.stack_mode = e->detail;
	
	/* reconfigure old frame window (if necessary) */
	int i;
	for (i = 0; i < wsl; i++)
		if (ws[i].w == e->window)
			break;
	
	if (i < wsl)
		XConfigureWindow(w->disp, ws[i].f, e->value_mask, &changes);
	
	/* configure window */
	XConfigureWindow(w->disp, e->window, e->value_mask, &changes);
}

/* map a window */
static void _wmMapRequest(wm *w, XMapRequestEvent *e) {

	/* frame window */
	_wmFrame(w, e->window, 0);

	/* map window */
	XMapWindow(w->disp, e->window);
}

static void _wmFrame(wm *w, Window wp, int wcbwm) {

	/* visual properties for window frame */
	unsigned int BORDER_WIDTH = 3;
	unsigned long BORDER_COLOR = 0xff0000;
	unsigned long BG_COLOR = 0xaa0000;
	
	/* retrieve window attributes */
	XWindowAttributes xw_attrs;
	XGetWindowAttributes(w->disp, wp, &xw_attrs);
	
	/* if the window was created before this wm */
	if (wcbwm && (xw_attrs.override_redirect || xw_attrs.map_state != IsViewable)) return;
	
	/* create frame */
	Window frame = XCreateSimpleWindow(
		w->disp,
		w->root,
		xw_attrs.x,
		xw_attrs.y,
		xw_attrs.width,
		xw_attrs.height,
		BORDER_WIDTH,
		BORDER_COLOR,
		BG_COLOR);
	
	/* select events on frame */
	XSelectInput(
		w->disp,
		frame,
		SubstructureRedirectMask | SubstructureNotifyMask);
	
	/* add window to save set so that it can be restored in the event of a crash */
	XAddToSaveSet(w->disp, wp);
	
	/* reparent client window */
	XReparentWindow(
		w->disp,
		wp,
		frame,
		0, 0);
	
	/* map frame */
	XMapWindow(w->disp, frame);
	
	/* create ws list if it hasn't been created */
	if (ws == NULL) {
	
		ws = (struct windows *)malloc(sizeof(struct windows *) * wsc);
	}
	
	/* list is too small */
	if (wsl >= wsc) {
	
		wsc *= 2;
		ws = (struct windows *)realloc(ws, sizeof(struct windows *) * wsc);
	}
	
	/* add item */
	ws[wsl++] = (struct windows){wp, frame};
	
	/* atexit the free function */
	atexit(wsfree);
}

/* unmap a window */
static void _wmUnmapNotify(wm *w, XUnmapEvent *e) {

	/* get window index */
	int i;
	for (i = 0; i < wsl; i++)
		if (ws[i].w == e->window)
			break;
	
	/* invalid unmap request */
	if (i >= wsl || e->event == w->root)
		return;
	
	/* unmap frame */
	XUnmapWindow(w->disp, ws[i].f);
	
	/* reparent client window to root */
	XReparentWindow(w->disp,
			e->window,
			w->root,
			0, 0);
	
	/* remove window from save set */
	XRemoveFromSaveSet(w->disp, e->window);
	
	/* destroy window */
	XDestroyWindow(w->disp, ws[i].w);
	
	/* remove reference from list */
	memcpy((void *)&ws[i], (void *)&ws[i+1], ((wsl--) - i - 1) * sizeof(struct windows));
}
