#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "../common/pipi_platform.h"

#define MAX_WINS 8
#define MAX_WIDGETS 256
#define TEXT_CAP 512

typedef struct {
    int used; long id; Window xwindow; int width; int height; int alive; Atom wm_delete;
} GuiWindow;
typedef struct {
    int used; int type; long window; int id; int x; int y; int w; int h; char text[TEXT_CAP];
} GuiWidget;

static Display *display_handle = 0;
static int screen_number = 0;
static GuiWindow windows[MAX_WINS];
static GuiWidget widgets[MAX_WIDGETS];
static int next_widget_id = 100;
static int initialized = 0;

static int init_display(void) {
    if (initialized) return display_handle != 0;
    initialized = 1;
    display_handle = XOpenDisplay(0);
    if (display_handle != 0) screen_number = DefaultScreen(display_handle);
    return display_handle != 0;
}
static GuiWindow *get_window(long id) {
    int i; for (i=0;i<MAX_WINS;i++) if (windows[i].used && windows[i].id==id) return &windows[i]; return 0;
}
static void safe_text(char *to, const char *from) {
    size_t n; if (from==0) from=""; n=strlen(from); if (n>=TEXT_CAP) n=TEXT_CAP-1; memcpy(to,from,n); to[n]='\0';
}
static void draw_window(GuiWindow *win) {
    int i; GC gc; XGCValues values; XFontStruct *font;
    if (!win || !display_handle) return;
    values.foreground = BlackPixel(display_handle,screen_number);
    values.background = WhitePixel(display_handle,screen_number);
    gc = XCreateGC(display_handle,win->xwindow,GCForeground|GCBackground,&values);
    font = XLoadQueryFont(display_handle,"fixed"); if (font) XSetFont(display_handle,gc,font->fid);
    XClearWindow(display_handle,win->xwindow);
    for (i=0;i<MAX_WIDGETS;i++) if (widgets[i].used && widgets[i].window==win->id) {
        GuiWidget *w=&widgets[i];
        if (w->type==1) XDrawString(display_handle,win->xwindow,gc,w->x,w->y+14,w->text,(int)strlen(w->text));
        else {
            XDrawRectangle(display_handle,win->xwindow,gc,w->x,w->y,(unsigned int)w->w,(unsigned int)w->h);
            XDrawString(display_handle,win->xwindow,gc,w->x+10,w->y+(w->h+5)/2,w->text,(int)strlen(w->text));
        }
    }
    XFreeGC(display_handle,gc); if (font) XFreeFont(display_handle,font);
}
long pipi_platform_window_create(const char *title, int width, int height) {
    int i; GuiWindow *win; XSetWindowAttributes attrs;
    if (!init_display()) { fprintf(stderr,"PIPI GUI: cannot open X display; check DISPLAY and X11.\n"); return 0; }
    for (i=0;i<MAX_WINS;i++) if (!windows[i].used) break;
    if (i==MAX_WINS) { fprintf(stderr,"PIPI GUI: window limit reached.\n"); return 0; }
    win=&windows[i]; memset(win,0,sizeof(*win)); win->id=(long)i+1; win->width=width; win->height=height; win->alive=1;
    attrs.event_mask=ExposureMask|ButtonPressMask|StructureNotifyMask;
    win->xwindow=XCreateSimpleWindow(display_handle,RootWindow(display_handle,screen_number),70,70,(unsigned int)width,(unsigned int)height,1,BlackPixel(display_handle,screen_number),WhitePixel(display_handle,screen_number));
    XChangeWindowAttributes(display_handle,win->xwindow,CWEventMask,&attrs);
    XStoreName(display_handle,win->xwindow,title?title:"PIPI App");
    win->wm_delete=XInternAtom(display_handle,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(display_handle,win->xwindow,&win->wm_delete,1);
    win->used=1; return win->id;
}
int pipi_platform_label_add(long window,const char *text,int x,int y) {
    int i; GuiWidget *w; if (!get_window(window)) return 0;
    for(i=0;i<MAX_WIDGETS;i++) { if(!widgets[i].used) break; }
    if(i==MAX_WIDGETS) return 0;
    w=&widgets[i]; memset(w,0,sizeof(*w)); w->used=1; w->type=1; w->window=window; w->x=x; w->y=y; w->w=(int)strlen(text)*8; w->h=20; safe_text(w->text,text); return 1;
}
int pipi_platform_button_add(long window,const char *text,int x,int y) {
    int i; GuiWidget *w; if (!get_window(window)) return 0;
    for(i=0;i<MAX_WIDGETS;i++) { if(!widgets[i].used) break; }
    if(i==MAX_WIDGETS) return 0;
    w=&widgets[i]; memset(w,0,sizeof(*w)); w->used=1; w->type=2; w->window=window; w->id=next_widget_id++; w->x=x; w->y=y; w->w=(int)strlen(text)*8+24; if(w->w<90)w->w=90; w->h=30; safe_text(w->text,text); return w->id;
}
int pipi_platform_window_show(long window) { GuiWindow *w=get_window(window); if(!w || !display_handle)return 0; XMapRaised(display_handle,w->xwindow); XFlush(display_handle); return 1; }
int pipi_platform_next_event(long window) {
    GuiWindow *wanted=get_window(window); XEvent event; int i;
    if(!wanted || !display_handle)return -1;
    while(wanted->alive) {
        XNextEvent(display_handle,&event);
        if(event.type==Expose) { for(i=0;i<MAX_WINS;i++) if(windows[i].used && windows[i].xwindow==event.xexpose.window) draw_window(&windows[i]); }
        else if(event.type==ClientMessage) {
            for(i=0;i<MAX_WINS;i++) if(windows[i].used && windows[i].xwindow==event.xclient.window && (Atom)event.xclient.data.l[0]==windows[i].wm_delete) {
                windows[i].alive=0; XDestroyWindow(display_handle,windows[i].xwindow); windows[i].used=0; if(windows[i].id==window)return -1; break;
            }
        } else if(event.type==DestroyNotify) {
            for(i=0;i<MAX_WINS;i++) if(windows[i].used && windows[i].xwindow==event.xdestroywindow.window) { windows[i].used=0; windows[i].alive=0; if(windows[i].id==window)return -1; }
        } else if(event.type==ButtonPress && event.xbutton.window==wanted->xwindow) {
            for(i=0;i<MAX_WIDGETS;i++) if(widgets[i].used && widgets[i].window==window && widgets[i].type==2) {
                GuiWidget *w=&widgets[i]; if(event.xbutton.x>=w->x && event.xbutton.x<=w->x+w->w && event.xbutton.y>=w->y && event.xbutton.y<=w->y+w->h)return w->id;
            }
        }
        wanted=get_window(window); if(!wanted)return -1;
    }
    return -1;
}
int pipi_platform_message(long window,const char *title,const char *text) { (void)window; fprintf(stdout,"[%s] %s\n",title?title:"PIPI",text?text:""); fflush(stdout); return 1; }
void pipi_platform_window_destroy(long window) { GuiWindow *w=get_window(window); int i; if(w && display_handle){XDestroyWindow(display_handle,w->xwindow);w->used=0;w->alive=0;} for(i=0;i<MAX_WIDGETS;i++)if(widgets[i].used&&widgets[i].window==window)widgets[i].used=0; if(display_handle)XFlush(display_handle); }
void pipi_platform_shutdown(void) { int i; if(display_handle){for(i=0;i<MAX_WINS;i++)if(windows[i].used)XDestroyWindow(display_handle,windows[i].xwindow);XCloseDisplay(display_handle);display_handle=0;} memset(windows,0,sizeof(windows));memset(widgets,0,sizeof(widgets));initialized=0; }
