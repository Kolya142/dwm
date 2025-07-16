/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/X.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(plug->numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
				 * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(plug->drw, (X)) + plug->lrpad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
	NetWMFullscreen, NetActiveWindow, NetWMWindowType,
	NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
	ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (**func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;

	int maximx, maximy, maximw, maximh;
  
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isminimized;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (**func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (**arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

/* Rule declarations */
void __applyrules(Client *c);
int		 __applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
void		 __arrange(Monitor *m);
void		 __arrangemon(Monitor *m);
void		 __attach(Client *c);
void		 __attachstack(Client *c);
void		 __buttonpress(XEvent *e);
void		 __checkotherwm(void);
void		 __cleanup(void);
void		 __cleanupmon(Monitor *mon);
void		 __clientmessage(XEvent *e);
void		 __configure(Client *c);
void		 __configurenotify(XEvent *e);
void		 __configurerequest(XEvent *e);
Monitor*	 __createmon(void);
void		 __destroynotify(XEvent *e);
void		 __detach(Client *c);
void		 __detachstack(Client *c);
Monitor*	 __dirtomon(int dir);
void		 __drawbar(Monitor *m);
void		 __drawbars(void);
void		 __enternotify(XEvent *e);
void		 __expose(XEvent *e);
void		 __focus(Client *c);
void		 __focusin(XEvent *e);
void		 __focusmon(const Arg *arg);
void		 __focusstack(const Arg *arg);
Atom		 __getatomprop(Client *c, Atom prop);
int		 __getrootptr(int *x, int *y);
long		 __getstate(Window w);
int		 __gettextprop(Window w, Atom atom, char *text, unsigned int size);
void		 __grabbuttons(Client *c, int focused);
void		 __grabkeys(void);
void		 __incnmaster(const Arg *arg);
void		 __keypress(XEvent *e);
void		 __killclient(const Arg *arg);
void		 __manage(Window w, XWindowAttributes *wa);
void		 __mappingnotify(XEvent *e);
void		 __maprequest(XEvent *e);
void		 __monocle(Monitor *m);
void		 __motionnotify(XEvent *e);
void		 __movemouse(const Arg *arg);
Client*		 __nexttiled(Client *c);
void		 __pop(Client *c);
void		 __propertynotify(XEvent *e);
void		 __quit(const Arg *arg);
Monitor*	 __recttomon(int x, int y, int w, int h);
void		 __resize(Client *c, int x, int y, int w, int h, int interact);
void		 __resizeclient(Client *c, int x, int y, int w, int h);
void		 __resizemouse(const Arg *arg);
void		 __restack(Monitor *m);
void		 __run(void);
void		 __scan(void);
int		 __sendevent(Client *c, Atom proto);
void		 __sendmon(Client *c, Monitor *m);
void		 __setclientstate(Client *c, long state);
void		 __setfocus(Client *c);
void		 __setfullscreen(Client *c, int fullscreen);
void		 __setlayout(const Arg *arg);
void		 __setmfact(const Arg *arg);
void		 __setup(void);
void		 __seturgent(Client *c, int urg);
void		 __showhide(Client *c);
void		 __spawn(const Arg *arg);
void		 __tag(const Arg *arg);
void		 __tagmon(const Arg *arg);
void		 __tile(Monitor *m);
void		 __togglebar(const Arg *arg);
void		 __togglefloating(const Arg *arg);
void		 __toggletag(const Arg *arg);
void		 __toggleview(const Arg *arg);
void		 __unfocus(Client *c, int setfocus);
void		 __unmanage(Client *c, int destroyed);
void		 __unmapnotify(XEvent *e);
void		 __updatebarpos(Monitor *m);
void		 __updatebars(void);
void		 __updateclientlist(void);
int		 __updategeom(void);
void		 __updatenumlockmask(void);
void		 __updatesizehints(Client *c);
void		 __updatestatus(void);
void		 __updatetitle(Client *c);
void		 __updatewindowtype(Client *c);
void		 __updatewmhints(Client *c);
void		 __view(const Arg *arg);
Client*		 __wintoclient(Window w);
Monitor*	 __wintomon(Window w);
int		 __xerror(Display *dpy, XErrorEvent *ee);
int		 __xerrordummy(Display *dpy, XErrorEvent *ee);
int		 __xerrorstart(Display *dpy, XErrorEvent *ee);
void		 __zoom(const Arg *arg);
void		 __minimize(Client *c);
void		 __restore(Client *c);
void __toggleminimize(const Arg *arg);
void __reloads(const Arg *arg);

void		 (*applyrules		)(Client *c);
int		 (*applysizehints	)(Client *c, int *x, int *y, int *w, int *h, int interact);
void		 (*arrange		)(Monitor *m);
void		 (*arrangemon		)(Monitor *m);
void		 (*attach		)(Client *c);
void		 (*attachstack	)	(Client *c);
void		 (*buttonpress	)	(XEvent *e);
void		 (*checkotherwm	)	(void);
void		 (*cleanup		)(void);
void		 (*cleanupmon		)(Monitor *mon);
void		 (*clientmessage	)(XEvent *e);
void		 (*configure		)(Client *c);
void		 (*configurenotify	)(XEvent *e);
void		 (*configurerequest	)(XEvent *e);
Monitor*	 (*createmon		)(void);
void		 (*destroynotify	)(XEvent *e);
void		 (*detach		)(Client *c);
void		 (*detachstack	)	(Client *c);
Monitor*	 (*dirtomon		)(int dir);
void		 (*drawbar		)(Monitor *m);
void		 (*drawbars		)(void);
void		 (*enternotify	)	(XEvent *e);
void		 (*expose		)(XEvent *e);
void		 (*focus		)(Client *c);
void		 (*focusin		)(XEvent *e);
void		 (*focusmon		)(const Arg *arg);
void		 (*focusstack		)(const Arg *arg);
Atom		 (*getatomprop	)	(Client *c, Atom prop);
int		 (*getrootptr		)(int *x, int *y);
long		 (*getstate		)(Window w);
int		 (*gettextprop	)	(Window w, Atom atom, char *text, unsigned int size);
void		 (*grabbuttons	)	(Client *c, int focused);
void		 (*grabkeys		)(void);
void		 (*incnmaster		)(const Arg *arg);
void		 (*keypress		)(XEvent *e);
void		 (*killclient		)(const Arg *arg);
void		 (*manage		)(Window w, XWindowAttributes *wa);
void		 (*mappingnotify	)(XEvent *e);
void		 (*maprequest		)(XEvent *e);
void		 (*monocle		)(Monitor *m);
void		 (*motionnotify	)	(XEvent *e);
void		 (*movemouse		)(const Arg *arg);
Client*		 (*nexttiled		)(Client *c);
void		 (*pop		)	(Client *c);
void		 (*propertynotify	)(XEvent *e);
void		 (*quit		)	(const Arg *arg);
Monitor*	 (*recttomon		)(int x, int y, int w, int h);
void		 (*resize		)(Client *c, int x, int y, int w, int h, int interact);
void		 (*resizeclient	)	(Client *c, int x, int y, int w, int h);
void		 (*resizemouse	)	(const Arg *arg);
void		 (*restack		)(Monitor *m);
void		 (*run		)	(void);
void		 (*scan		)	(void);
int		 (*sendevent		)(Client *c, Atom proto);
void		 (*sendmon		)(Client *c, Monitor *m);
void		 (*setclientstate	)(Client *c, long state);
void		 (*setfocus		)(Client *c);
void		 (*setfullscreen	)(Client *c, int fullscreen);
void		 (*setlayout		)(const Arg *arg);
void		 (*setmfact		)(const Arg *arg);
void		 (*setup		)(void);
void		 (*seturgent		)(Client *c, int urg);
void		 (*showhide		)(Client *c);
void		 (*spawn		)(const Arg *arg);
void		 (*tag		)	(const Arg *arg);
void		 (*tagmon		)(const Arg *arg);
void		 (*tile		)	(Monitor *m);
void		 (*togglebar		)(const Arg *arg);
void		 (*togglefloating	)(const Arg *arg);
void		 (*toggletag		)(const Arg *arg);
void		 (*toggleview		)(const Arg *arg);
void		 (*unfocus		)(Client *c, int setfocus);
void		 (*unmanage		)(Client *c, int destroyed);
void		 (*unmapnotify	)	(XEvent *e);
void		 (*updatebarpos	)	(Monitor *m);
void		 (*updatebars		)(void);
void		 (*updateclientlist	)(void);
int		 (*updategeom		)(void);
void		 (*updatenumlockmask	)(void);
void		 (*updatesizehints	)(Client *c);
void		 (*updatestatus	)	(void);
void		 (*updatetitle	)	(Client *c);
void		 (*updatewindowtype	)(Client *c);
void		 (*updatewmhints	)(Client *c);
void		 (*view		)	(const Arg *arg);
Client*		 (*wintoclient	)	(Window w);
Monitor*	 (*wintomon		)(Window w);
int		 (*xerror		)(Display *dpy, XErrorEvent *ee);
int		 (*xerrordummy	)	(Display *dpy, XErrorEvent *ee);
int		 (*xerrorstart	)	(Display *dpy, XErrorEvent *ee);
void		 (*zoom		)	(const Arg *arg);
void		 (*minimize		)(Client *c);
void		 (*restore		)(Client *c);
void		 (*toggleminimize	)(const Arg *arg);
void		 (*reloads		)(const Arg *arg);

/* variables */
#include "plug.h"

Plug *plug;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */

#define TIMEBARWIDTH 64
char *get_time_bar() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	if (!tm) return NULL;
    
	char *text = malloc(TIMEBARWIDTH);
	if (!text) return NULL;

	if (!strftime(text, TIMEBARWIDTH, "%d/%m/%Y %H-%M-%S", tm)) {
		free(text);
		return NULL;
	}

	return text;
}

void
__applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint((*plug).dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : (*plug).broken;
	instance = ch.res_name  ? ch.res_name  : (*plug).broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		    && (!r->class || strstr(class, r->class))
		    && (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = (*plug).mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
__applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > (*plug).sw)
			*x = (*plug).sw - WIDTH(c);
		if (*y > (*plug).sh)
			*y = (*plug).sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < (*plug).bh)
		*h = (*plug).bh;
	if (*w < (*plug).bh)
		*w = (*plug).bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
__arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = (*plug).mons; m; m = m->next)
		     showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = (*plug).mons; m; m = m->next)
		       arrangemon(m);
}

void
__arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		(*m->lt[m->sellt]->arrange)(m);
}

void
__attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
__attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
__buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != (*plug).selmon) {
		unfocus((*plug).selmon->sel, 1);
		(*plug).selmon = m;
		focus(NULL);
	}
	if (ev->window == (*plug).selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + TEXTW((*plug).selmon->ltsymbol))
			click = ClkLtSymbol;
		else if (ev->x > (*plug).selmon->ww - (int)TEXTW((*plug).stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack((*plug).selmon);
		XAllowEvents((*plug).dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			(*buttons[i].func)(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
__checkotherwm(void)
{
	(*plug).xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput((*plug).dpy, DefaultRootWindow((*plug).dpy), SubstructureRedirectMask);
	XSync((*plug).dpy, False);
	XSetErrorHandler(xerror);
	XSync((*plug).dpy, False);
}

void
__cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	(*plug).selmon->lt[(*plug).selmon->sellt] = &foo;
	for (m = (*plug).mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey((*plug).dpy, AnyKey, AnyModifier, (*plug).root);
	while ((*plug).mons)
		cleanupmon((*plug).mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free((*plug).drw, (*plug).cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free((*plug).scheme[i]);
	free((*plug).scheme);
	XDestroyWindow((*plug).dpy, (*plug).wmcheckwin);
	drw_free((*plug).drw);
	XSync((*plug).dpy, False);
	XSetInputFocus((*plug).dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetActiveWindow]);
}

void
__cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == (*plug).mons)
		(*plug).mons = (*plug).mons->next;
	else {
		for (m = (*plug).mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow((*plug).dpy, mon->barwin);
	XDestroyWindow((*plug).dpy, mon->barwin);
	free(mon);
}

void
__clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == (*plug).netatom[NetWMState]) {
		if (cme->data.l[1] == (*plug).netatom[NetWMFullscreen]
		    || cme->data.l[2] == (*plug).netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
					  || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == (*plug).netatom[NetActiveWindow]) {
		if (c != (*plug).selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
__configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = (*plug).dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent((*plug).dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
__configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == (*plug).root) {
		dirty = ((*plug).sw != ev->width || (*plug).sh != ev->height);
		(*plug).sw = ev->width;
		(*plug).sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize((*plug).drw, (*plug).sw, (*plug).bh + 20);
			updatebars();
			for (m = (*plug).mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow((*plug).dpy, m->barwin, m->wx, m->by, m->ww, (*plug).bh + 20);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
__configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		c->isfloating = 1;
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !(*plug).selmon->lt[(*plug).selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow((*plug).dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow((*plug).dpy, ev->window, ev->value_mask, &wc);
	}
	XSync((*plug).dpy, False);
}

Monitor *
__createmon(void)
{
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	return m;
}

void
__destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}

void
__detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
__detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
__dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = (*plug).selmon->next))
			m = (*plug).mons;
	} else if ((*plug).selmon == (*plug).mons)
		for (m = (*plug).mons; m->next; m = m->next);
	else
		for (m = (*plug).mons; m->next != (*plug).selmon; m = m->next);
	return m;
}

void
__drawbar(Monitor *m)
{
	int x, w, tw = 0;
	int boxs = (*plug).drw->fonts->h / 9;
	int boxw = (*plug).drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	if (!m->showbar)
		return;

	/* draw status first so it can be overdrawn by tags later */
	if (m == (*plug).selmon) { /* status is only drawn on selected monitor */
		drw_setscheme((*plug).drw, (*plug).scheme[SchemeNorm]);
		char *ctime = get_time_bar();
		tw = TEXTW(ctime) - (*plug).lrpad - 2;
		drw_text((*plug).drw, m->ww - tw, 0, TEXTW(ctime), (*plug).bh, 0, ctime, 0);
		free(ctime);
		// tw = TEXTW(stext) - lrpad - 2; /* 2px right padding */
		// drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme((*plug).drw, (*plug).scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text((*plug).drw, x, 0, w, (*plug).bh, (*plug).lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect((*plug).drw, x + boxs, boxs, boxw, boxw,
				 m == (*plug).selmon && (*plug).selmon->sel && (*plug).selmon->sel->tags & 1 << i,
				 urg & 1 << i);
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme((*plug).drw, (*plug).scheme[SchemeNorm]);
	x = drw_text((*plug).drw, x, 0, w, (*plug).bh, (*plug).lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > (*plug).bh) {
		if (m->sel) {
			drw_setscheme((*plug).drw, (*plug).scheme[m == (*plug).selmon ? SchemeSel : SchemeNorm]);
			drw_text((*plug).drw, x, 0, w, (*plug).bh, (*plug).lrpad / 2, m->sel->name, 0);
			if (m->sel->isfloating)
				drw_rect((*plug).drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme((*plug).drw, (*plug).scheme[SchemeNorm]);
			drw_rect((*plug).drw, x, 0, w, (*plug).bh, 1, 1);
		}
	}
	drw_map((*plug).drw, m->barwin, 0, 0, m->ww, (*plug).bh);
}

void
__drawbars(void)
{
	Monitor *m;

	for (m = (*plug).mons; m; m = m->next)
		drawbar(m);
}

void
__enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != (*plug).root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != (*plug).selmon) {
		unfocus((*plug).selmon->sel, 1);
		(*plug).selmon = m;
	} else if (!c || c == (*plug).selmon->sel)
		return;
	focus(c);
	XRaiseWindow((*plug).dpy, ev->window);
	setclientstate(c, NormalState);
}

void
__expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void
__focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = (*plug).selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if ((*plug).selmon->sel && (*plug).selmon->sel != c)
		unfocus((*plug).selmon->sel, 0);
	if (c) {
		if (c->mon != (*plug).selmon)
			(*plug).selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder((*plug).dpy, c->win, (*plug).scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus((*plug).dpy, (*plug).root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetActiveWindow]);
	}
	(*plug).selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
__focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if ((*plug).selmon->sel && ev->window != (*plug).selmon->sel->win)
		setfocus((*plug).selmon->sel);
}

void
__focusmon(const Arg *arg)
{
	Monitor *m;

	if (!(*plug).mons->next)
		return;
	if ((m = dirtomon(arg->i)) == (*plug).selmon)
		return;
	unfocus((*plug).selmon->sel, 0);
	(*plug).selmon = m;
	focus(NULL);
}

void
__focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!(*plug).selmon->sel || ((*plug).selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
		for (c = (*plug).selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = (*plug).selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = (*plug).selmon->clients; i != (*plug).selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack((*plug).selmon);
	}
}

Atom
__getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty((*plug).dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
			       &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
__getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer((*plug).dpy, (*plug).root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
__getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty((*plug).dpy, w, (*plug).wmatom[WMState], 0L, 2L, False, (*plug).wmatom[WMState],
			       &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
__gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty((*plug).dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList((*plug).dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
__grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, (*plug).numlockmask, (*plug).numlockmask|LockMask };
		XUngrabButton((*plug).dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton((*plug).dpy, AnyButton, AnyModifier, c->win, False,
				    BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton((*plug).dpy, buttons[i].button,
						    buttons[i].mask | modifiers[j],
						    c->win, False, BUTTONMASK,
						    GrabModeAsync, GrabModeSync, None, None);
	}
}

void
__grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, (*plug).numlockmask, (*plug).numlockmask|LockMask };
		int start, end, skip;
		KeySym *syms;

		XUngrabKey((*plug).dpy, AnyKey, AnyModifier, (*plug).root);
		XDisplayKeycodes((*plug).dpy, &start, &end);
		syms = XGetKeyboardMapping((*plug).dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey((*plug).dpy, k,
							 keys[i].mod | modifiers[j],
							 (*plug).root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}

void
__incnmaster(const Arg *arg)
{
	(*plug).selmon->nmaster = MAX((*plug).selmon->nmaster + arg->i, 0);
	arrange((*plug).selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		    && unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
__keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym((*plug).dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		    && keys[i].func)
			(*keys[i].func)(&(keys[i].arg));
}

void
__killclient(const Arg *arg)
{
	if (!(*plug).selmon->sel)
		return;
	if (!sendevent((*plug).selmon->sel, (*plug).wmatom[WMDelete])) {
		XGrabServer((*plug).dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode((*plug).dpy, DestroyAll);
		XKillClient((*plug).dpy, (*plug).selmon->sel->win);
		XSync((*plug).dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer((*plug).dpy);
	}
}

void
__manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->isfloating = 1;
	c->isfullscreen = 0;
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updatetitle(c);
	if (XGetTransientForHint((*plug).dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = (*plug).selmon;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow((*plug).dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder((*plug).dpy, w, (*plug).scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput((*plug).dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow((*plug).dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
			(unsigned char *) &(c->win), 1);
	XMoveResizeWindow((*plug).dpy, c->win, c->x + 2 * (*plug).sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == (*plug).selmon)
		unfocus((*plug).selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow((*plug).dpy, c->win);
	focus(NULL);
}

void
__mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
__maprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes((*plug).dpy, ev->window, &(*plug).mrwa) || (*plug).mrwa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &(*plug).mrwa);
}

void
__monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
__motionnotify(XEvent *e)
{
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != (*plug).root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != (*plug).mnmon && (*plug).mnmon) {
		unfocus((*plug).selmon->sel, 1);
		(*plug).selmon = m;
		focus(NULL);
	}
	(*plug).mnmon = m;
}

void
__movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = (*plug).selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack((*plug).selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer((*plug).dpy, (*plug).root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
			 None, (*plug).cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent((*plug).dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			(*plug).handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs((*plug).selmon->wx - nx) < snap)
				nx = (*plug).selmon->wx;
			else if (abs(((*plug).selmon->wx + (*plug).selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = (*plug).selmon->wx + (*plug).selmon->ww - WIDTH(c);
			if (abs((*plug).selmon->wy - ny) < snap)
				ny = (*plug).selmon->wy;
			else if (abs(((*plug).selmon->wy + (*plug).selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = (*plug).selmon->wy + (*plug).selmon->wh - HEIGHT(c);
			Client *d = (*plug).selmon->clients;
			while (d) {
				if (d != c) {
					if (abs(d->x - nx) < snap) {
						nx = d->x;
					}
					if (abs((d->x - c->w) - nx) < snap) {
						nx = d->x - c->w;
					}
					if (abs((d->x + c->w) - nx) < snap) {
						nx = d->x + c->w;
					}
					if (abs(d->y - ny) < snap) {
						ny = d->y;
					}
					if (abs((d->y - c->h) - ny) < snap) {
						ny = d->y - c->h;
					}
					if (abs((d->y + c->h) - ny) < snap) {
						ny = d->y + c->h;
					}
				}
				d = d->next;
			}
			if (!c->isfloating && (*plug).selmon->lt[(*plug).selmon->sellt]->arrange
			    && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!(*plug).selmon->lt[(*plug).selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer((*plug).dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != (*plug).selmon) {
		sendmon(c, m);
		(*plug).selmon = m;
		focus(NULL);
	}
}

Client *
__nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
__pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
__propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == (*plug).root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint((*plug).dpy, c->win, &trans)) &&
			    (c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == (*plug).netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == (*plug).netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
__quit(const Arg *arg)
{
	(*plug).running = 0;
}

Monitor *
__recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = (*plug).selmon;
	int a, area = 0;

	for (m = (*plug).mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
__resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
__resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow((*plug).dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync((*plug).dpy, False);
}

void
__resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = (*plug).selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack((*plug).selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer((*plug).dpy, (*plug).root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
			 None, (*plug).cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer((*plug).dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent((*plug).dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			(*plug).handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= (*plug).selmon->wx && c->mon->wx + nw <= (*plug).selmon->wx + (*plug).selmon->ww
			    && c->mon->wy + nh >= (*plug).selmon->wy && c->mon->wy + nh <= (*plug).selmon->wy + (*plug).selmon->wh)
			{
				if (!c->isfloating && (*plug).selmon->lt[(*plug).selmon->sellt]->arrange
				    && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!(*plug).selmon->lt[(*plug).selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer((*plug).dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer((*plug).dpy, CurrentTime);
	while (XCheckMaskEvent((*plug).dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != (*plug).selmon) {
		sendmon(c, m);
		(*plug).selmon = m;
		focus(NULL);
	}
}

void
__restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow((*plug).dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow((*plug).dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync((*plug).dpy, False);
	while (XCheckMaskEvent((*plug).dpy, EnterWindowMask, &ev));
}

void
__run(void)
{
	XEvent ev;
	/* main event loop */
	XSync((*plug).dpy, False);
	while ((*plug).running) {
		if (XPending(plug->dpy) > 0) {
			XNextEvent((*plug).dpy, &ev);
			// printf("%d\n", ev.type);
			if ((*plug).handler[ev.type])
				(*plug).handler[ev.type](&ev); /* call handler */
		}
		drawbars();
	}
}

void
__scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree((*plug).dpy, (*plug).root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes((*plug).dpy, wins[i], &wa)
			    || wa.override_redirect || XGetTransientForHint((*plug).dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes((*plug).dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint((*plug).dpy, wins[i], &d1)
			    && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
__sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
__setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty((*plug).dpy, c->win, (*plug).wmatom[WMState], (*plug).wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
}

int
__sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols((*plug).dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = (*plug).wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent((*plug).dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
__setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus((*plug).dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetActiveWindow],
				XA_WINDOW, 32, PropModeReplace,
				(unsigned char *) &(c->win), 1);
	}
	sendevent(c, (*plug).wmatom[WMTakeFocus]);
}

void
__setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty((*plug).dpy, c->win, (*plug).netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)&(*plug).netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow((*plug).dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty((*plug).dpy, c->win, (*plug).netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
__setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != (*plug).selmon->lt[(*plug).selmon->sellt])
		(*plug).selmon->sellt ^= 1;
	if (arg && arg->v)
		(*plug).selmon->lt[(*plug).selmon->sellt] = (Layout *)arg->v;
	strncpy((*plug).selmon->ltsymbol, (*plug).selmon->lt[(*plug).selmon->sellt]->symbol, sizeof (*plug).selmon->ltsymbol);
	if ((*plug).selmon->sel)
		arrange((*plug).selmon);
	else
		drawbar((*plug).selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
__setmfact(const Arg *arg)
{
	float f;

	if (!arg || !(*plug).selmon->lt[(*plug).selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + (*plug).selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	(*plug).selmon->mfact = f;
	arrange((*plug).selmon);
}

void
__setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	(*plug).screen = DefaultScreen((*plug).dpy);
	(*plug).sw = DisplayWidth((*plug).dpy, (*plug).screen);
	(*plug).sh = DisplayHeight((*plug).dpy, (*plug).screen);
	(*plug).root = RootWindow((*plug).dpy, (*plug).screen);
	(*plug).drw = drw_create((*plug).dpy, (*plug).screen, (*plug).root, (*plug).sw, (*plug).sh);
	if (!drw_fontset_create((*plug).drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	(*plug).lrpad = (*plug).drw->fonts->h;
	(*plug).bh = (*plug).drw->fonts->h + 2;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom((*plug).dpy, "UTF8_STRING", False);
	(*plug).wmatom[WMProtocols] = XInternAtom((*plug).dpy, "WM_PROTOCOLS", False);
	(*plug).wmatom[WMDelete] = XInternAtom((*plug).dpy, "WM_DELETE_WINDOW", False);
	(*plug).wmatom[WMState] = XInternAtom((*plug).dpy, "WM_STATE", False);
	(*plug).wmatom[WMTakeFocus] = XInternAtom((*plug).dpy, "WM_TAKE_FOCUS", False);
	(*plug).netatom[NetActiveWindow] = XInternAtom((*plug).dpy, "_NET_ACTIVE_WINDOW", False);
	(*plug).netatom[NetSupported] = XInternAtom((*plug).dpy, "_NET_SUPPORTED", False);
	(*plug).netatom[NetWMName] = XInternAtom((*plug).dpy, "_NET_WM_NAME", False);
	(*plug).netatom[NetWMState] = XInternAtom((*plug).dpy, "_NET_WM_STATE", False);
	(*plug).netatom[NetWMCheck] = XInternAtom((*plug).dpy, "_NET_SUPPORTING_WM_CHECK", False);
	(*plug).netatom[NetWMFullscreen] = XInternAtom((*plug).dpy, "_NET_WM_STATE_FULLSCREEN", False);
	(*plug).netatom[NetWMWindowType] = XInternAtom((*plug).dpy, "_NET_WM_WINDOW_TYPE", False);
	(*plug).netatom[NetWMWindowTypeDialog] = XInternAtom((*plug).dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	(*plug).netatom[NetClientList] = XInternAtom((*plug).dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	(*plug).cursor[CurNormal] = drw_cur_create((*plug).drw, XC_left_ptr);
	(*plug).cursor[CurResize] = drw_cur_create((*plug).drw, XC_sizing);
	(*plug).cursor[CurMove] = drw_cur_create((*plug).drw, XC_fleur);
	/* init appearance */
	(*plug).scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		(*plug).scheme[i] = drw_scm_create((*plug).drw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	(*plug).wmcheckwin = XCreateSimpleWindow((*plug).dpy, (*plug).root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty((*plug).dpy, (*plug).wmcheckwin, (*plug).netatom[NetWMCheck], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *) &(*plug).wmcheckwin, 1);
	XChangeProperty((*plug).dpy, (*plug).wmcheckwin, (*plug).netatom[NetWMName], utf8string, 8,
			PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetWMCheck], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *) &(*plug).wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) (*plug).netatom, NetLast);
	XDeleteProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetClientList]);
	/* select events */
	wa.cursor = (*plug).cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes((*plug).dpy, (*plug).root, CWEventMask|CWCursor, &wa);
	XSelectInput((*plug).dpy, (*plug).root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
__seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints((*plug).dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints((*plug).dpy, c->win, wmh);
	XFree(wmh);
}

void
__showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow((*plug).dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow((*plug).dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
__spawn(const Arg *arg)
{
	struct sigaction sa;

	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + (*plug).selmon->num;
	if (fork() == 0) {
		if ((*plug).dpy)
			close(ConnectionNumber((*plug).dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void
__tag(const Arg *arg)
{
	if ((*plug).selmon->sel && arg->ui & TAGMASK) {
		(*plug).selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange((*plug).selmon);
	}
}

void
__tagmon(const Arg *arg)
{
	if (!(*plug).selmon->sel || !(*plug).mons->next)
		return;
	sendmon((*plug).selmon->sel, dirtomon(arg->i));
}

void
__tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww;
	for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i);
			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
			if (my + HEIGHT(c) < m->wh)
				my += HEIGHT(c);
		} else {
			h = (m->wh - ty) / (n - i);
			resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
			if (ty + HEIGHT(c) < m->wh)
				ty += HEIGHT(c);
		}
}

void
__togglebar(const Arg *arg)
{
	(*plug).selmon->showbar = !(*plug).selmon->showbar;
	updatebarpos((*plug).selmon);
	XMoveResizeWindow((*plug).dpy, (*plug).selmon->barwin, (*plug).selmon->wx, (*plug).selmon->by, (*plug).selmon->ww, (*plug).bh);
	arrange((*plug).selmon);
}

void
__togglefloating(const Arg *arg)
{
	if (!(*plug).selmon->sel)
		return;
	if ((*plug).selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	(*plug).selmon->sel->isfloating = !(*plug).selmon->sel->isfloating || (*plug).selmon->sel->isfixed;
	if ((*plug).selmon->sel->isfloating)
		resize((*plug).selmon->sel, (*plug).selmon->sel->x, (*plug).selmon->sel->y,
		       (*plug).selmon->sel->w, (*plug).selmon->sel->h, 0);
	arrange((*plug).selmon);
}

void
__toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!(*plug).selmon->sel)
		return;
	newtags = (*plug).selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		(*plug).selmon->sel->tags = newtags;
		focus(NULL);
		arrange((*plug).selmon);
	}
}

void
__toggleview(const Arg *arg)
{
	unsigned int newtagset = (*plug).selmon->tagset[(*plug).selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		(*plug).selmon->tagset[(*plug).selmon->seltags] = newtagset;
		focus(NULL);
		arrange((*plug).selmon);
	}
}

void
__unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder((*plug).dpy, c->win, (*plug).scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus((*plug).dpy, (*plug).root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetActiveWindow]);
	}
}

void
__unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer((*plug).dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput((*plug).dpy, c->win, NoEventMask);
		XConfigureWindow((*plug).dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton((*plug).dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync((*plug).dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer((*plug).dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
__unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void
__updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = (*plug).mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow((*plug).dpy, (*plug).root, m->wx, m->by, m->ww, (*plug).bh, 0, DefaultDepth((*plug).dpy, (*plug).screen),
					  CopyFromParent, DefaultVisual((*plug).dpy, (*plug).screen),
					  CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor((*plug).dpy, m->barwin, (*plug).cursor[CurNormal]->cursor);
		XMapRaised((*plug).dpy, m->barwin);
		XSetClassHint((*plug).dpy, m->barwin, &ch);
	}
}

void
__updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= (*plug).bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + (*plug).bh : m->wy;
	} else
		m->by = -(*plug).bh;
}

void
__updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetClientList]);
	for (m = (*plug).mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty((*plug).dpy, (*plug).root, (*plug).netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
}

int
__updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive((*plug).dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens((*plug).dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = (*plug).mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = (*plug).mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				(*plug).mons = createmon();
		}
		for (i = 0, m = (*plug).mons; i < nn && m; m = m->next, i++)
			if (i >= n
			    || unique[i].x_org != m->mx || unique[i].y_org != m->my
			    || unique[i].width != m->mw || unique[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = (*plug).mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = (*plug).mons;
				attach(c);
				attachstack(c);
			}
			if (m == (*plug).selmon)
				(*plug).selmon = (*plug).mons;
			cleanupmon(m);
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!(*plug).mons)
			(*plug).mons = createmon();
		if ((*plug).mons->mw != (*plug).sw || (*plug).mons->mh != (*plug).sh) {
			dirty = 1;
			(*plug).mons->mw = (*plug).mons->ww = (*plug).sw;
			(*plug).mons->mh = (*plug).mons->wh = (*plug).sh;
			updatebarpos((*plug).mons);
		}
	}
	if (dirty) {
		(*plug).selmon = (*plug).mons;
		(*plug).selmon = wintomon((*plug).root);
	}
	return dirty;
}

void
__updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	(*plug).numlockmask = 0;
	modmap = XGetModifierMapping((*plug).dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			    == XKeysymToKeycode((*plug).dpy, XK_Num_Lock))
				(*plug).numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
__updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints((*plug).dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
__updatestatus(void)
{
	if (!gettextprop((*plug).root, XA_WM_NAME, (*plug).stext, sizeof((*plug).stext)))
		strcpy((*plug).stext, "dwm-"VERSION);
	drawbar((*plug).selmon);
}

void
__updatetitle(Client *c)
{
	if (!gettextprop(c->win, (*plug).netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, (*plug).broken);
}

void
__updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, (*plug).netatom[NetWMState]);
	Atom wtype = getatomprop(c, (*plug).netatom[NetWMWindowType]);

	if (state == (*plug).netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == (*plug).netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
__updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints((*plug).dpy, c->win))) {
		if (c == (*plug).selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints((*plug).dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
__view(const Arg *arg)
{
	if ((arg->ui & TAGMASK) == (*plug).selmon->tagset[(*plug).selmon->seltags])
		return;
	(*plug).selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		(*plug).selmon->tagset[(*plug).selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange((*plug).selmon);
}

Client *
__wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = (*plug).mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
__wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == (*plug).root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = (*plug).mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return (*plug).selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
__xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return (*plug).xerrorxlib(dpy, ee); /* may call exit */
}

int
__xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
__xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
__zoom(const Arg *arg)
{
	Client *c = (*plug).selmon->sel;

	if (!(*plug).selmon->lt[(*plug).selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled((*plug).selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

void __minimize(Client *c) {
	if (!c || c->isminimized) return;

	c->maximx = c->x;
	c->maximy = c->y;
	c->maximw = c->w;
	c->maximh = c->h;

	Client *d = c->mon->clients;
	int x = c->mon->mx;
	c->isminimized = 1;
	c->isfloating = 1;
	c->isfullscreen = 0;
	while (d) {
		if (d->isminimized && d->mon == c->mon) {
			resize(d, x, d->mon->my + (*plug).drw->fonts->h + 2, 50, 20, 0);
			x += 50;
		}
		d = d->next;
	}
	c->isfixed = 1;
	arrange(c->mon);
}
void __restore(Client *c) {
	if (!c || !c->isminimized) return;
    
	resize(c, c->maximx, c->maximy, c->maximw, c->maximh, 1);

	Client *d = c->mon->clients;
	int x = c->mon->mx;
	c->isminimized = 0;
	c->isfixed = 0;
	while (d) {
		if (d->isminimized && d->mon == c->mon) {
			resize(d, x, d->mon->my + (*plug).drw->fonts->h + 2, 50, 10, 0);
			x += 50;
		}
		d = d->next;
	}
	arrange(c->mon);
}
void __toggleminimize(const Arg* arg) {
	if ((*plug).selmon->sel->isminimized)
		restore((*plug).selmon->sel);
	else
		minimize((*plug).selmon->sel);
}

void __reloads(const Arg *arg) {
	system("cd /home/sony/dwm/ && make -B &> /home/sony/dwm/reloads.log");
	void *handle = dlopen("/home/sony/dwm/dwm.so", RTLD_LAZY);
	if (!handle) return;

	if ((*plug).dl) dlclose((*plug).dl);
	(*plug).dl = handle;

	memcpy(dlsym((*plug).dl, "plug"), &plug, sizeof(Plug *));

	((void(*)())dlsym((*plug).dl, "applysymlinks"))();
	
	applyrules = dlsym((*plug).dl, "__applyrules");
	applysizehints = dlsym((*plug).dl, "__applysizehints");
	arrange = dlsym((*plug).dl, "__arrange");
	arrangemon = dlsym((*plug).dl, "__arrangemon");
	attach = dlsym((*plug).dl, "__attach");
	attachstack = dlsym((*plug).dl, "__attachstack");
	buttonpress = dlsym((*plug).dl, "__buttonpress");
	checkotherwm = dlsym((*plug).dl, "__checkotherwm");
	cleanup = dlsym((*plug).dl, "__cleanup");
	cleanupmon = dlsym((*plug).dl, "__cleanupmon");
	clientmessage = dlsym((*plug).dl, "__clientmessage");
	configure = dlsym((*plug).dl, "__configure");
	configurenotify = dlsym((*plug).dl, "__configurenotify");
	configurerequest = dlsym((*plug).dl, "__configurerequest");
	createmon = dlsym((*plug).dl, "__createmon");
	destroynotify = dlsym((*plug).dl, "__destroynotify");
	detach = dlsym((*plug).dl, "__detach");
	detachstack = dlsym((*plug).dl, "__detachstack");
	dirtomon = dlsym((*plug).dl, "__dirtomon");
	drawbar = dlsym((*plug).dl, "__drawbar");
	drawbars = dlsym((*plug).dl, "__drawbars");
	enternotify = dlsym((*plug).dl, "__enternotify");
	expose = dlsym((*plug).dl, "__expose");
	focus = dlsym((*plug).dl, "__focus");
	focusin = dlsym((*plug).dl, "__focusin");
	focusmon = dlsym((*plug).dl, "__focusmon");
	focusstack = dlsym((*plug).dl, "__focusstack");
	getatomprop = dlsym((*plug).dl, "__getatomprop");
	getrootptr = dlsym((*plug).dl, "__getrootptr");
	getstate = dlsym((*plug).dl, "__getstate");
	gettextprop = dlsym((*plug).dl, "__gettextprop");
	grabbuttons = dlsym((*plug).dl, "__grabbuttons");
	grabkeys = dlsym((*plug).dl, "__grabkeys");
	incnmaster = dlsym((*plug).dl, "__incnmaster");
	keypress = dlsym((*plug).dl, "__keypress");
	killclient = dlsym((*plug).dl, "__killclient");
	manage = dlsym((*plug).dl, "__manage");
	mappingnotify = dlsym((*plug).dl, "__mappingnotify");
	maprequest = dlsym((*plug).dl, "__maprequest");
	monocle = dlsym((*plug).dl, "__monocle");
	motionnotify = dlsym((*plug).dl, "__motionnotify");
	movemouse = dlsym((*plug).dl, "__movemouse");
	nexttiled = dlsym((*plug).dl, "__nexttiled");
	pop = dlsym((*plug).dl, "__pop");
	propertynotify = dlsym((*plug).dl, "__propertynotify");
	recttomon = dlsym((*plug).dl, "__recttomon");
	resize = dlsym((*plug).dl, "__resize");
	resizeclient = dlsym((*plug).dl, "__resizeclient");
	resizemouse = dlsym((*plug).dl, "__resizemouse");
	restack = dlsym((*plug).dl, "__restack");
	run = dlsym((*plug).dl, "__run");
	scan = dlsym((*plug).dl, "__scan");
	sendevent = dlsym((*plug).dl, "__sendevent");
	sendmon = dlsym((*plug).dl, "__sendmon");
	setclientstate = dlsym((*plug).dl, "__setclientstate");
	setfocus = dlsym((*plug).dl, "__setfocus");
	setfullscreen = dlsym((*plug).dl, "__setfullscreen");
	setlayout = dlsym((*plug).dl, "__setlayout");
	setmfact = dlsym((*plug).dl, "__setmfact");
	setup = dlsym((*plug).dl, "__setup");
	seturgent = dlsym((*plug).dl, "__seturgent");
	showhide = dlsym((*plug).dl, "__showhide");
	spawn = dlsym((*plug).dl, "__spawn");
	tag = dlsym((*plug).dl, "__tag");
	tagmon = dlsym((*plug).dl, "__tagmon");
	tile = dlsym((*plug).dl, "__tile");
	togglebar = dlsym((*plug).dl, "__togglebar");
	togglefloating = dlsym((*plug).dl, "__togglefloating");
	toggletag = dlsym((*plug).dl, "__toggletag");
	toggleview = dlsym((*plug).dl, "__toggleview");
	unfocus = dlsym((*plug).dl, "__unfocus");
	unmanage = dlsym((*plug).dl, "__unmanage");
	unmapnotify = dlsym((*plug).dl, "__unmapnotify");
	updatebarpos = dlsym((*plug).dl, "__updatebarpos");
	updatebars = dlsym((*plug).dl, "__updatebars");
	updateclientlist = dlsym((*plug).dl, "__updateclientlist");
	updategeom = dlsym((*plug).dl, "__updategeom");
	updatenumlockmask = dlsym((*plug).dl, "__updatenumlockmask");
	updatesizehints = dlsym((*plug).dl, "__updatesizehints");
	updatestatus = dlsym((*plug).dl, "__updatestatus");
	updatetitle = dlsym((*plug).dl, "__updatetitle");
	updatewindowtype = dlsym((*plug).dl, "__updatewindowtype");
	updatewmhints = dlsym((*plug).dl, "__updatewmhints");
	view = dlsym((*plug).dl, "__view");
	wintoclient = dlsym((*plug).dl, "__wintoclient");
	wintomon = dlsym((*plug).dl, "__wintomon");
	xerror = dlsym((*plug).dl, "__xerror");
	xerrordummy = dlsym((*plug).dl, "__xerrordummy");
	xerrorstart = dlsym((*plug).dl, "__xerrorstart");
	zoom = dlsym((*plug).dl, "__zoom");
	minimize = dlsym((*plug).dl, "__minimize");
	restore = dlsym((*plug).dl, "__restore");
	toggleminimize = dlsym((*plug).dl, "__toggleminimize");
	reloads = dlsym((*plug).dl, "__reloads");
	minimize = dlsym((*plug).dl, "__minimize");
	restore = dlsym((*plug).dl, "__restore");
	toggleminimize = dlsym((*plug).dl, "__toggleminimize");
	reloads = dlsym((*plug).dl, "__reloads");
	(*plug).handler[ButtonPress] = buttonpress;
	(*plug).handler[ClientMessage] = clientmessage;
	(*plug).handler[ConfigureNotify] = configurenotify;
	(*plug).handler[ConfigureRequest] = configurerequest;
	(*plug).handler[DestroyNotify] = destroynotify;
	(*plug).handler[EnterNotify] = enternotify;
	(*plug).handler[Expose] = expose;
	(*plug).handler[FocusIn] = focusin;
	(*plug).handler[KeyPress] = keypress;
	(*plug).handler[MappingNotify] = mappingnotify;
	(*plug).handler[MapRequest] = maprequest;
	(*plug).handler[MotionNotify] = motionnotify;
	(*plug).handler[PropertyNotify] = propertynotify;
	(*plug).handler[UnmapNotify] = unmapnotify;
        (*plug).running = 0;
}

void applysymlinks() {
	applyrules = __applyrules;
	applysizehints = __applysizehints;
	arrange = __arrange;
	arrangemon = __arrangemon;
	attach = __attach;
	attachstack = __attachstack;
	buttonpress = __buttonpress;
	checkotherwm = __checkotherwm;
	cleanup = __cleanup;
	cleanupmon = __cleanupmon;
	clientmessage = __clientmessage;
	configure = __configure;
	configurenotify = __configurenotify;
	configurerequest = __configurerequest;
	createmon = __createmon;
	destroynotify = __destroynotify;
	detach = __detach;
	detachstack = __detachstack;
	dirtomon = __dirtomon;
	drawbar = __drawbar;
	drawbars = __drawbars;
	enternotify = __enternotify;
	expose = __expose;
	focus = __focus;
	focusin = __focusin;
	focusmon = __focusmon;
	focusstack = __focusstack;
	getatomprop = __getatomprop;
	getrootptr = __getrootptr;
	getstate = __getstate;
	gettextprop = __gettextprop;
	grabbuttons = __grabbuttons;
	grabkeys = __grabkeys;
	incnmaster = __incnmaster;
	keypress = __keypress;
	killclient = __killclient;
	manage = __manage;
	mappingnotify = __mappingnotify;
	maprequest = __maprequest;
	monocle = __monocle;
	motionnotify = __motionnotify;
	movemouse = __movemouse;
	nexttiled = __nexttiled;
	pop = __pop;
	propertynotify = __propertynotify;
	recttomon = __recttomon;
	resize = __resize;
	resizeclient = __resizeclient;
	resizemouse = __resizemouse;
	restack = __restack;
	run = __run;
	scan = __scan;
	sendevent = __sendevent;
	sendmon = __sendmon;
	setclientstate = __setclientstate;
	setfocus = __setfocus;
	setfullscreen = __setfullscreen;
	setlayout = __setlayout;
	setmfact = __setmfact;
	setup = __setup;
	seturgent = __seturgent;
	showhide = __showhide;
	spawn = __spawn;
	tag = __tag;
	tagmon = __tagmon;
	tile = __tile;
	togglebar = __togglebar;
	togglefloating = __togglefloating;
	toggletag = __toggletag;
	toggleview = __toggleview;
	unfocus = __unfocus;
	unmanage = __unmanage;
	unmapnotify = __unmapnotify;
	updatebarpos = __updatebarpos;
	updatebars = __updatebars;
	updateclientlist = __updateclientlist;
	updategeom = __updategeom;
	updatenumlockmask = __updatenumlockmask;
	updatesizehints = __updatesizehints;
	updatestatus = __updatestatus;
	updatetitle = __updatetitle;
	updatewindowtype = __updatewindowtype;
	updatewmhints = __updatewmhints;
	view = __view;
	wintoclient = __wintoclient;
	wintomon = __wintomon;
	xerror = __xerror;
	xerrordummy = __xerrordummy;
	xerrorstart = __xerrorstart;
	zoom = __zoom;
	minimize = __minimize;
	restore = __restore;
	toggleminimize = __toggleminimize;
	reloads = __reloads;
	(*plug).handler[ButtonPress] = buttonpress;
	(*plug).handler[ClientMessage] = clientmessage;
	(*plug).handler[ConfigureNotify] = configurenotify;
	(*plug).handler[ConfigureRequest] = configurerequest;
	(*plug).handler[DestroyNotify] = destroynotify;
	(*plug).handler[EnterNotify] = enternotify;
	(*plug).handler[Expose] = expose;
	(*plug).handler[FocusIn] = focusin;
	(*plug).handler[KeyPress] = keypress;
	(*plug).handler[MappingNotify] = mappingnotify;
	(*plug).handler[MapRequest] = maprequest;
	(*plug).handler[MotionNotify] = motionnotify;
	(*plug).handler[PropertyNotify] = propertynotify;
	(*plug).handler[UnmapNotify] = unmapnotify;
}

int
main(int argc, char *argv[])
{
	plug = malloc(sizeof(Plug));
	*plug = (Plug) {
		.broken = "broken",
		.numlockmask = 0,
		.running = 1,
		.dl = NULL,
	};
	plug->handler[ButtonPress] = __buttonpress;
	plug->handler[ClientMessage] = __clientmessage;
	plug->handler[ConfigureRequest] = __configurerequest;
	plug->handler[ConfigureNotify] = __configurenotify;
	plug->handler[DestroyNotify] = __destroynotify;
	plug->handler[EnterNotify] = __enternotify;
	plug->handler[Expose] = __expose;
	plug->handler[FocusIn] = __focusin;
	plug->handler[KeyPress] = __keypress;
	plug->handler[MappingNotify] = __mappingnotify;
	plug->handler[MapRequest] = __maprequest;
	plug->handler[MotionNotify] = __motionnotify;
	plug->handler[PropertyNotify] = __propertynotify;
	plug->handler[UnmapNotify] = __unmapnotify;
	applysymlinks();
    
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!((*plug).dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	while ((*plug).dl) {
		(*plug).running = 1;
		run();
	}
	cleanup();
	XCloseDisplay((*plug).dpy);
	if ((*plug).dl) {
		dlclose((*plug).dl);
		(*plug).dl = NULL;
	}
	return EXIT_SUCCESS;
}
