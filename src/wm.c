/* window manager for pwm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "wm.h"

/* windows */
struct windows {
	Window w;
	Window f;
};

/* variables and forw-decls */
static int _wmError(Display *, XErrorEvent *);
static int _wmDetected(Display *, XErrorEvent *);
static void _wmCreateNotify(XCreateWindowEvent *);
static void _wmConfigureRequest(wm *, XConfigureRequestEvent *);
static void _wmMapRequest(wm *, XMapRequestEvent *);
static void _wmUnmapNotify(wm *, XUnmapEvent *);
static void _wmFrame(wm *, Window, int);
static void _wmCloseWindow(wm *, Window);
static int _wm_detected = 0;
static int _b1_pressed = 0;

static int mouse_x;
static int mouse_y;

static GC mgc;
static Display *_mdisp;

static Atom _wm_delete_message;
static Atom _wm_protocols;

/* Motif WM Hints */
struct hints {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
};

static struct hints *hs = NULL; /* window hints */
static struct windows *ws = NULL; /* windows */
static int wsl = 0; /* length of list */
static int wsc = 0; /* capacity of list */

/* free ws list */
static void wsfree() {

	if (ws == NULL)
		return;
	
	/* free gc's */
	if (mgc != NULL) XFreeGC(_mdisp, mgc);
	
	/* free lists */
	if (ws != NULL) { free(ws); ws = NULL; }
	if (hs != NULL) { free(hs); hs = NULL; }
}

/* intialise window manager */
extern void wmInit(wm *w) {

	XInitThreads();

	w->disp = XOpenDisplay(NULL);
	_mdisp = w->disp;
	
	/* could not open X display */
	if (w->disp == NULL) {
	
		/* exit */
		fprintf(stderr, "Failed to open X display '%s'\n", XDisplayName(NULL));
		return;
	}
	
	/* set root window */
	w->root = DefaultRootWindow(w->disp);

	/* set wm protocols */
	_wm_delete_message = XInternAtom(w->disp, "WM_DELETE_WINDOW", False);
	_wm_protocols = XInternAtom(w->disp, "WM_PROTOCOLS", False);
}

/* close window manager */
extern void wmClose(wm *w) {

	printf("closing wm\n");

	XCloseDisplay(w->disp);
}

/* run window manager */
extern void wmRun(wm *w) {
	
	/* atoms */
	Atom wm_protocols = XInternAtom(w->disp, "WM_PROTOCOLS", True);
	Atom wm_delete_message = XInternAtom(w->disp, "WM_DELETE_WINDOW", False);

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
				
		/* get mouse pos */
		Window rr, cr;
		int wx, wy, mr;
		XQueryPointer(w->disp, w->root, &rr, &cr, &wx, &wy, &mouse_x, &mouse_y, &mr);

		/* get next event */
		XEvent e;
		XNextEvent(w->disp, &e);
		
		for (int i = 0; i < wsl; i++) {
			XDrawLine(w->disp, ws[i].f, mgc, 6, 6, 14, 14);
			XDrawLine(w->disp, ws[i].f, mgc, 14, 6, 6, 14);
		}

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
		/* mouse is moved, pressed, etc */
		else if (e.type == ButtonPress) {

			/* check if window is in list */
			int i;
			for (i = 0; i < wsl; i++)
				if (ws[i].f == e.xbutton.window)
					break;
			
			if (i < wsl) {
			
				/* raise window to top */
				XRaiseWindow(w->disp, ws[i].f);
				
				/* set input focus to window */
				XSetInputFocus(w->disp, ws[i].f, RevertToParent, CurrentTime);

				/* button 1 */
				if (e.xbutton.button == Button1) {
					
					_b1_pressed = 1;
					
					/* close window */
					if (e.xbutton.window == ws[i].f && e.xbutton.x > 0 && e.xbutton.y > 0 && e.xbutton.x < 20 && e.xbutton.y < 20) {
					
						/* close window */
						_wmCloseWindow(w, ws[i].w);
					}
				}
			}
		}
		/* button release */
		else if (e.type == ButtonRelease) {

			/* check if window is in list */
			int i;
			for (i = 0; i < wsl; i++)
				if (ws[i].w == e.xbutton.window || ws[i].f == e.xbutton.window)
					break;
			
			if (i < wsl) {
			
				/* disable */
				_b1_pressed = 0;
			}
		}
		/* motion */
		else if (e.type == MotionNotify) {
		
			/* check window */
			int i;
			for (i = 0; i < wsl; i++)
				if (ws[i].f == e.xmotion.window)
					break;
			
			if (i < wsl && _b1_pressed) {
			
				/* calculate position for window to be in */
				XWindowAttributes xwa;
				XGetWindowAttributes(w->disp, ws[i].f, &xwa);
				
				int x = xwa.x + -mouse_x + e.xmotion.x_root;
				int y = xwa.y + -mouse_y + e.xmotion.y_root;
				
				/* move window */
				XMoveWindow(w->disp, ws[i].f, x, y);
			}
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

static int _wmError(Display *d, XErrorEvent *e) {

	/* print error info */
	char buf[4096];
	XGetErrorText(d, e->error_code, buf, 4096);
	
	fprintf(stderr, "\trequest: %d\n\terror code: %d\n\terror: %s\n\tresource id: %d\n", e->request_code, e->error_code, buf, e->resourceid);
}

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
	
	if (i < wsl) {
		if (hs[i].decorations) {
			changes.height += 20;
			XConfigureWindow(w->disp, ws[i].f, e->value_mask, &changes);
			changes.x = 0;
			changes.y = 20;
			changes.height -= 20;
		} else {
			XConfigureWindow(w->disp, ws[i].f, e->value_mask, &changes);
			changes.x = 0;
			changes.y = 0;
		}
	}
	
	/* configure window */
	XConfigureWindow(w->disp, e->window, e->value_mask, &changes);

	printf("Configured window.\n");
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
	unsigned int BORDER_WIDTH = 1;
	unsigned long BORDER_COLOR = 0xff0000;
	unsigned long BG_COLOR = 0xaa0000;

	/* don't frame a window that is already framed */
	for (int i = 0; i < wsl; i++) {
	
		/* if in list */
		if (ws[i].w == wp || ws[i].f == wp)
			return;
	}

	/* retrieve window attributes */
	XWindowAttributes xw_attrs;
	XGetWindowAttributes(w->disp, wp, &xw_attrs);
	
	/* if the window was created before this wm */
	if (wcbwm && (xw_attrs.override_redirect || xw_attrs.map_state != IsViewable)) return;

	printf("Creating window frame...\n");
	
	/* create frame */
	Window frame = XCreateSimpleWindow(
		w->disp,
		w->root,
		xw_attrs.x,
		xw_attrs.y,
		xw_attrs.width,
		xw_attrs.height + 20,
		BORDER_WIDTH,
		BORDER_COLOR,
		BG_COLOR);
	
	/* select events on frame */
	XSelectInput(
		w->disp,
		frame,
		SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
	
	printf("Adding window to save set...\n");
	
	/* add window to save set so that it can be restored in the event of a crash */
	XAddToSaveSet(w->disp, wp);
	
	printf("Reparenting window...\n");
	
	/* reparent client window */
	XReparentWindow(
		w->disp,
		wp,
		frame,
		0, 0);
	
	XMoveWindow(w->disp, wp, 0, 20);
	
	printf("Mapping window...\n");
	
	/* map frame */
	XMapWindow(w->disp, frame);
	
	/* create ws list if it hasn't been created */
	if (ws == NULL) {
	
		wsc = 8;
		ws = (struct windows *)malloc(sizeof(struct windows *) * wsc);
		hs = (struct hints *)malloc(sizeof(struct hints *) * wsc);
		atexit(wsfree);
	}
	//ws = (ws == NULL)? (struct windows *)malloc(sizeof(struct windows *) * wsc): ws;
	
	/* list is too small */
	if (wsl >= wsc) {
	
		wsc *= 2;
		hs = (struct hints *)realloc(hs, sizeof(struct hints *) * wsc);
		ws = (struct windows *)realloc(ws, sizeof(struct windows *) * wsc);
	}
	
	/* draw onto frame */
	if (mgc == NULL) mgc = XCreateGC(w->disp, frame, 0, NULL);
	XSetForeground(w->disp, mgc, 0xaaaaaa);
	XDrawLine(w->disp, frame, mgc, 6, 6, 14, 14);
	XDrawLine(w->disp, frame, mgc, 14, 6, 6, 14);
	
	/* add item */
	//ws[wsl++] = (struct windows){wp, frame, mgc, w->disp};
	int i = wsl++;

	ws[i].w = wp;
	ws[i].f = frame;
	//ws[i].fgc = mgc;
	//ws[i].d = w->disp;
	
	printf("Focusing on window...\n");
	
	/* set focus of input to window frame */
	XSetInputFocus(w->disp, frame, RevertToPointerRoot, CurrentTime);
	
	printf("Enabling button input on window frame...\n");
	
	/* left mouse button for moving windows */
	XGrabButton(w->disp,
		    Button1,
		    Mod1Mask,
		    frame,
		    0,
		    ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		    GrabModeAsync,
		    GrabModeAsync,
		    None,
		    None);
	
	printf("Finished framing window.\n");
	
	Atom actr = 0;
	int acfmtr = 0;
	unsigned long nir = 0;
	unsigned long bar = 0;
	unsigned char *data = NULL;

	/* get hints property */
	Atom property = XInternAtom(w->disp, "_MOTIF_WM_HINTS", 0);
	
	int status = XGetWindowProperty(w->disp, wp, property, 0L, sizeof(struct hints), False, property, &actr, &acfmtr, &nir, &bar, &data);
	
	if (status != Success || actr == 0 || data == NULL) {

		/* set to defaults */
		memset(&hs[i], 0, sizeof(struct hints));
		hs[i].decorations = 1;

		fprintf(stderr, "Warning: failed to read Motif Window Manager Hints!\n");
		return; /* we may need to change this line in the future... */
	}
	
	struct hints *h = (struct hints *)data;
	
	/* copy hints data to hs */
	hs[i].decorations = h->decorations;
	
	/* free the resulting data */
	if (data != NULL) XFree(data);
	
	if (!hs[i].decorations) {
		
		/* disable border */
		XWindowChanges xwa;
		
		xwa.border_width = 0;
		xwa.height = xw_attrs.height;
		XMoveWindow(w->disp, wp, 0, 0);
		
		XConfigureWindow(w->disp, frame, CWBorderWidth | CWHeight, &xwa);
	}
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
	
	printf("Unmapping frame...\n");
	
	/* unmap frame */
	XUnmapWindow(w->disp, ws[i].f);
	
	printf("Reparenting window...\n");
	
	/* reparent client window to root */
	XReparentWindow(w->disp,
			e->window,
			w->root,
			0, 0);
	
	printf("Removing window from save set...\n");
	
	/* remove window from save set */
	XRemoveFromSaveSet(w->disp, e->window);
	
	/* free context */
	//XFreeGC(w->disp, ws[i].fgc);
	
	printf("Destroying window...\n");
	
	/* destroy window */
	XDestroyWindow(w->disp, ws[i].w);
	
	/* remove reference from list */
	memcpy((void *)&ws[i], (void *)&ws[i+1], ((wsl--) - i - 1) * sizeof(struct windows));
	memcpy((void *)&hs[i], (void *)&hs[i+1], ((wsl+1) - i - 1) * sizeof(struct hints));
}

/* close a window */
static void _wmCloseWindow(wm *w, Window wp) {

	/* get protocols */
	Atom *sprot; /* supported protocols */
	int nsprot; /* number of supported protocols */
	XGetWMProtocols(w->disp, wp, &sprot, &nsprot);
	
	int i;
	for (i = 0; i < nsprot; i++) {

		if (sprot[i] == _wm_delete_message) break;
	}
	
	/* send message */
	if (i < nsprot) {

		printf("Closing window...\n");

		XEvent msg;
		memset(&msg, 0, sizeof(msg));
		msg.xclient.type = ClientMessage; /* send a message */
		msg.xclient.message_type = _wm_protocols; /* send a protocol message */
		msg.xclient.window = wp; /* the primary window */
		msg.xclient.format = 32;
		msg.xclient.data.l[0] = _wm_delete_message; /* tell it that we want to delete the window */
		XSendEvent(w->disp, wp, false, 0, &msg); /* send event */
	}
	
	/* kill the window */
	else {

		printf("Killing window...\n");
		
		XKillClient(w->disp, wp);
	}
}
