#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../common/pipi_platform.h"

#define MAX_WINS 8
#define MAX_WIDGETS 256
#define TEXT_CAP 512

typedef struct { int used; long id; HWND hwnd; int alive; } GuiWindow;
typedef struct { int used; int type; long window; int id; HWND hwnd; int x; int y; int w; int h; char text[TEXT_CAP]; } GuiWidget;
static GuiWindow windows[MAX_WINS];
static GuiWidget widgets[MAX_WIDGETS];
static HINSTANCE instance_handle;
static int class_ready=0;
static int next_widget_id=100;
static int queued_event=0;
static long active_window=0;
static const char *class_name="PipiNativeGuiWindow";

static GuiWindow *get_window(long id) { int i; for(i=0;i<MAX_WINS;i++)if(windows[i].used&&windows[i].id==id)return &windows[i]; return 0; }
static void copy_text(char *to,const char *from) { size_t n; if(!from)from="";n=strlen(from);if(n>=TEXT_CAP)n=TEXT_CAP-1;memcpy(to,from,n);to[n]='\0'; }
static LRESULT CALLBACK pipi_window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    int i;
    if(msg==WM_COMMAND && HIWORD(wp)==BN_CLICKED) { queued_event=(int)LOWORD(wp); return 0; }
    if(msg==WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if(msg==WM_DESTROY) {
        for(i=0;i<MAX_WINS;i++)if(windows[i].used&&windows[i].hwnd==hwnd){windows[i].used=0;windows[i].alive=0;}
        if(active_window>0 && !get_window(active_window))PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}
static int register_class(void) {
    WNDCLASSA wc;
    if(class_ready)return 1;
    instance_handle=GetModuleHandleA(0);memset(&wc,0,sizeof(wc));wc.lpfnWndProc=pipi_window_proc;wc.hInstance=instance_handle;wc.lpszClassName=class_name;wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    class_ready=1;return 1;
}
long pipi_platform_window_create(const char *title,int width,int height) {
    int i;GuiWindow *w;if(!register_class())return 0;for(i=0;i<MAX_WINS;i++)if(!windows[i].used)break;if(i==MAX_WINS)return 0;
    w=&windows[i];memset(w,0,sizeof(*w));w->id=(long)i+1;w->hwnd=CreateWindowExA(0,class_name,title?title:"PIPI App",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,width,height,0,0,instance_handle,0);if(!w->hwnd)return 0;w->used=1;w->alive=1;return w->id;
}
int pipi_platform_label_add(long window,const char *text,int x,int y) { int i;GuiWindow *win=get_window(window);GuiWidget *w;if(!win)return 0;for(i=0;i<MAX_WIDGETS;i++)if(!widgets[i].used)break;if(i==MAX_WIDGETS)return 0;w=&widgets[i];memset(w,0,sizeof(*w));w->used=1;w->type=1;w->window=window;w->x=x;w->y=y;copy_text(w->text,text);w->hwnd=CreateWindowExA(0,"STATIC",w->text,WS_CHILD|WS_VISIBLE,x,y,(int)strlen(w->text)*8+30,22,win->hwnd,0,instance_handle,0);return w->hwnd?1:0; }
int pipi_platform_button_add(long window,const char *text,int x,int y) { int i;GuiWindow *win=get_window(window);GuiWidget *w;if(!win)return 0;for(i=0;i<MAX_WIDGETS;i++)if(!widgets[i].used)break;if(i==MAX_WIDGETS)return 0;w=&widgets[i];memset(w,0,sizeof(*w));w->used=1;w->type=2;w->window=window;w->id=next_widget_id++;w->x=x;w->y=y;copy_text(w->text,text);w->hwnd=CreateWindowExA(0,"BUTTON",w->text,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,y,(int)strlen(w->text)*8+36,30,win->hwnd,(HMENU)(INT_PTR)w->id,instance_handle,0);return w->hwnd?w->id:0; }
int pipi_platform_window_show(long window) { GuiWindow *w=get_window(window);if(!w)return 0;ShowWindow(w->hwnd,SW_SHOWNORMAL);UpdateWindow(w->hwnd);return 1; }
int pipi_platform_next_event(long window) { MSG msg;GuiWindow *w=get_window(window);int result;if(!w)return -1;active_window=window;for(;;){if(queued_event){int e=queued_event;queued_event=0;return e;}if(!get_window(window))return -1;result=(int)GetMessageA(&msg,0,0,0);if(result<=0)return -1;TranslateMessage(&msg);DispatchMessageA(&msg);} }
int pipi_platform_message(long window,const char *title,const char *text) { GuiWindow *w=get_window(window);return MessageBoxA(w?w->hwnd:0,text?text:"",title?title:"PIPI",MB_OK|MB_ICONINFORMATION); }
void pipi_platform_window_destroy(long window) { int i;GuiWindow *w=get_window(window);if(w){DestroyWindow(w->hwnd);w->used=0;}for(i=0;i<MAX_WIDGETS;i++)if(widgets[i].used&&widgets[i].window==window)widgets[i].used=0; }
void pipi_platform_shutdown(void) { int i;for(i=0;i<MAX_WINS;i++)if(windows[i].used)DestroyWindow(windows[i].hwnd);memset(windows,0,sizeof(windows));memset(widgets,0,sizeof(widgets));queued_event=0;active_window=0; }
