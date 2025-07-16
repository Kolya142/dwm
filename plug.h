#pragma once

#include <X11/Xlib.h>
typedef struct {
	char stext[256];
	int screen;
	int sw, sh;           /* X display screen geometry width, height */
	int bh;               /* bar height */
	int lrpad;            /* sum of left and right padding for text */
	int (*xerrorxlib)(Display *, XErrorEvent *);
	unsigned int numlockmask;
	void (*handler[LASTEvent]) (XEvent *);
	Atom wmatom[WMLast], netatom[NetLast];
	int running;
	Cur *cursor[CurLast];
	Clr **scheme;
	Display *dpy;
	Drw *drw;
	Monitor *mons, *selmon;
	Window root, wmcheckwin;
	XWindowAttributes mrwa;
	Monitor *mnmon;
	const char *broken;
	void *dl;
} Plug;
