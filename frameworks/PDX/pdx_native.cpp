/*
 * PDX 0.8.0 - game-oriented Direct3D 9 framework for PIPI.
 * This is an external native module. It deliberately does not modify the
 * PIPI compiler, VM, bytecode format, or Native Module ABI.
 * Build with MinGW-w64 C++98 and the Direct3D 9 import library.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../include/pipi_native.h"

#define PDX_MAX_TEXTURES 64
#define PDX_TEXT_CACHE 64
#define PDX_TEXT_MAX 255
#define PDX_MAX_DIMENSION 8192
#define PDX_PI 3.14159265358979323846

struct PDXVertex {
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};
struct PDXTexture {
    IDirect3DTexture9 *texture;
    int width;
    int height;
};
struct PDXTextEntry {
    char text[PDX_TEXT_MAX + 1];
    int r, g, b, size, width, height;
    IDirect3DTexture9 *texture;
    unsigned long stamp;
};

static IDirect3D9 *g_d3d = NULL;
static IDirect3DDevice9 *g_device = NULL;
static HWND g_window = NULL;
static HINSTANCE g_instance = NULL;
static int g_running = 0;
static int g_scene = 0;
static int g_window_class_registered = 0;
static D3DPRESENT_PARAMETERS g_present;
static const char *g_class_name = "PIPI_PDX_D3D9_WINDOW";
static struct PDXTexture g_textures[PDX_MAX_TEXTURES];
static struct PDXTextEntry g_text_cache[PDX_TEXT_CACHE];
static unsigned long g_text_stamp = 1UL;
static int g_keys_now[256];
static int g_keys_before[256];
static int g_mouse_now[3];
static int g_mouse_before[3];
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static double g_camera_x = 0.0;
static double g_camera_y = 0.0;
static double g_camera_zoom = 1.0;
static double g_delta_time = 1.0 / 60.0;
static double g_elapsed_time = 0.0;
static int g_target_fps = 60;
static LARGE_INTEGER g_qpc_frequency;
static LARGE_INTEGER g_open_time;
static LARGE_INTEGER g_last_poll;
static LARGE_INTEGER g_frame_start;
static int g_qpc_ready = 0;

static void copy_error(char *out, size_t cap, const char *message) {
    if (out && cap > 0U) {
        strncpy(out, message, cap - 1U);
        out[cap - 1U] = '\0';
    }
}

static void set_null(PipiValue *v) {
    v->type = PIPI_VALUE_NULL; v->number = 0.0; v->string = ""; v->boolean = 0;
}
static void set_bool(PipiValue *v, int b) {
    v->type = PIPI_VALUE_BOOLEAN; v->number = 0.0; v->string = ""; v->boolean = b ? 1 : 0;
}
static void set_num(PipiValue *v, double n) {
    v->type = PIPI_VALUE_NUMBER; v->number = n; v->string = ""; v->boolean = 0;
}
static int all_numbers(int argc, const PipiValue *argv) {
    int i;
    for (i = 0; i < argc; ++i) if (argv[i].type != PIPI_VALUE_NUMBER) return 0;
    return 1;
}
static int one_string(int argc, const PipiValue *argv) {
    return argc == 1 && argv[0].type == PIPI_VALUE_STRING;
}
static int equal_ci(const char *a, const char *b) {
    unsigned char ca, cb;
    if (!a || !b) return 0;
    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    } while (ca && cb);
    return ca == cb;
}
static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}
static D3DCOLOR rgb_color(int r, int g, int b) {
    return D3DCOLOR_XRGB(clamp_int(r, 0, 255), clamp_int(g, 0, 255), clamp_int(b, 0, 255));
}
static int qpc_now(LARGE_INTEGER *out) {
    if (!g_qpc_ready || !out) return 0;
    return QueryPerformanceCounter(out) ? 1 : 0;
}
static double qpc_seconds(LARGE_INTEGER a, LARGE_INTEGER b) {
    if (g_qpc_frequency.QuadPart == 0) return 0.0;
    return (double)(a.QuadPart - b.QuadPart) / (double)g_qpc_frequency.QuadPart;
}
static void update_timing(void) {
    LARGE_INTEGER now;
    if (!qpc_now(&now)) return;
    g_delta_time = qpc_seconds(now, g_last_poll);
    if (g_delta_time < 0.0) g_delta_time = 0.0;
    if (g_delta_time > 0.25) g_delta_time = 0.25;
    g_elapsed_time = qpc_seconds(now, g_open_time);
    g_last_poll = now;
}
static void camera_point(double x, double y, float *sx, float *sy) {
    *sx = (float)((x - g_camera_x) * g_camera_zoom);
    *sy = (float)((y - g_camera_y) * g_camera_zoom);
}
static int recover_device(void) {
    HRESULT hr;
    if (!g_device) return 0;
    hr = g_device->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) return 0;
    if (hr == D3DERR_DEVICENOTRESET) {
        hr = g_device->Reset(&g_present);
        if (FAILED(hr)) return 0;
    } else if (FAILED(hr)) return 0;
    return 1;
}
static void setup_untextured(void) {
    if (!g_device) return;
    g_device->SetTexture(0, NULL);
    g_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
}
static void setup_textured(void) {
    if (!g_device) return;
    g_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}
static int draw_filled_rect_raw(float x, float y, float w, float h, D3DCOLOR color) {
    struct PDXVertex v[4];
    if (!g_device || !g_scene || w <= 0.0f || h <= 0.0f) return 0;
    v[0].x=x; v[0].y=y; v[0].z=0.0f; v[0].rhw=1.0f; v[0].color=color; v[0].u=0.0f; v[0].v=0.0f;
    v[1].x=x+w; v[1].y=y; v[1].z=0.0f; v[1].rhw=1.0f; v[1].color=color; v[1].u=1.0f; v[1].v=0.0f;
    v[2].x=x; v[2].y=y+h; v[2].z=0.0f; v[2].rhw=1.0f; v[2].color=color; v[2].u=0.0f; v[2].v=1.0f;
    v[3].x=x+w; v[3].y=y+h; v[3].z=0.0f; v[3].rhw=1.0f; v[3].color=color; v[3].u=1.0f; v[3].v=1.0f;
    setup_untextured();
    return SUCCEEDED(g_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(struct PDXVertex))) ? 1 : 0;
}
static int draw_line_raw(float x1, float y1, float x2, float y2, D3DCOLOR color) {
    struct PDXVertex v[2];
    if (!g_device || !g_scene) return 0;
    v[0].x=x1; v[0].y=y1; v[0].z=0.0f; v[0].rhw=1.0f; v[0].color=color; v[0].u=0.0f; v[0].v=0.0f;
    v[1].x=x2; v[1].y=y2; v[1].z=0.0f; v[1].rhw=1.0f; v[1].color=color; v[1].u=0.0f; v[1].v=0.0f;
    setup_untextured();
    return SUCCEEDED(g_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(struct PDXVertex))) ? 1 : 0;
}
static void release_cached_resources(void) {
    int i;
    for (i = 0; i < PDX_MAX_TEXTURES; ++i) {
        if (g_textures[i].texture) g_textures[i].texture->Release();
        g_textures[i].texture = NULL; g_textures[i].width = 0; g_textures[i].height = 0;
    }
    for (i = 0; i < PDX_TEXT_CACHE; ++i) {
        if (g_text_cache[i].texture) g_text_cache[i].texture->Release();
        memset(&g_text_cache[i], 0, sizeof(g_text_cache[i]));
    }
}
static void release_graphics(void) {
    if (g_device) {
        if (g_scene) { g_device->EndScene(); g_scene = 0; }
        release_cached_resources();
        g_device->Release(); g_device = NULL;
    }
    if (g_d3d) { g_d3d->Release(); g_d3d = NULL; }
}
static LRESULT CALLBACK pdx_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)wparam; (void)lparam;
    if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (message == WM_DESTROY) { g_running = 0; if (g_window == hwnd) g_window = NULL; PostQuitMessage(0); return 0; }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}
static int open_window(const char *title, int width, int height, char *error, size_t error_size) {
    WNDCLASSA wc;
    RECT rect;
    HRESULT hr;
    ATOM atom;
    if (width < 64 || height < 64 || width > PDX_MAX_DIMENSION || height > PDX_MAX_DIMENSION) {
        copy_error(error, error_size, "window dimensions must be in range 64..8192"); return 0;
    }
    if (g_window || g_device || g_d3d) { if (g_window) DestroyWindow(g_window); g_window = NULL; release_graphics(); }
    g_instance = GetModuleHandleA(NULL);
    if (!g_window_class_registered) {
        memset(&wc, 0, sizeof(wc)); wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = pdx_window_proc; wc.hInstance = g_instance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = g_class_name;
        atom = RegisterClassA(&wc);
        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { copy_error(error,error_size,"RegisterClassA failed for PDX window"); return 0; }
        g_window_class_registered = 1;
    }
    rect.left=0; rect.top=0; rect.right=width; rect.bottom=height;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_window = CreateWindowExA(0, g_class_name, title ? title : "PDX",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right-rect.left, rect.bottom-rect.top,
        NULL, NULL, g_instance, NULL);
    if (!g_window) { copy_error(error,error_size,"CreateWindowExA failed"); return 0; }
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) { DestroyWindow(g_window); g_window=NULL; copy_error(error,error_size,"Direct3DCreate9 failed; check DirectX 9 runtime"); return 0; }
    memset(&g_present, 0, sizeof(g_present));
    g_present.Windowed=TRUE; g_present.SwapEffect=D3DSWAPEFFECT_DISCARD;
    g_present.BackBufferFormat=D3DFMT_UNKNOWN; g_present.BackBufferCount=1;
    g_present.hDeviceWindow=g_window; g_present.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    hr=g_d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,g_window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&g_present,&g_device);
    if (FAILED(hr)) { release_graphics(); DestroyWindow(g_window); g_window=NULL; copy_error(error,error_size,"IDirect3D9::CreateDevice failed"); return 0; }
    g_running=1; g_scene=0; g_camera_x=0.0; g_camera_y=0.0; g_camera_zoom=1.0;
    memset(g_keys_now,0,sizeof(g_keys_now)); memset(g_keys_before,0,sizeof(g_keys_before));
    memset(g_mouse_now,0,sizeof(g_mouse_now)); memset(g_mouse_before,0,sizeof(g_mouse_before));
    g_delta_time=1.0/60.0; g_elapsed_time=0.0; g_target_fps=60;
    g_qpc_ready=QueryPerformanceFrequency(&g_qpc_frequency) ? 1 : 0;
    if (g_qpc_ready) { QueryPerformanceCounter(&g_open_time); g_last_poll=g_open_time; g_frame_start=g_open_time; }
    srand((unsigned int)(GetTickCount() ^ GetCurrentProcessId()));
    ShowWindow(g_window,SW_SHOW); UpdateWindow(g_window);
    return 1;
}
static int key_code(const char *name) {
    static const struct { const char *name; int code; } keys[] = {
        {"SPACE",VK_SPACE},{"ESCAPE",VK_ESCAPE},{"ESC",VK_ESCAPE},{"ENTER",VK_RETURN},{"RETURN",VK_RETURN},
        {"TAB",VK_TAB},{"SHIFT",VK_SHIFT},{"CTRL",VK_CONTROL},{"CONTROL",VK_CONTROL},{"ALT",VK_MENU},
        {"LEFT",VK_LEFT},{"RIGHT",VK_RIGHT},{"UP",VK_UP},{"DOWN",VK_DOWN},
        {"BACKSPACE",VK_BACK},{"DELETE",VK_DELETE},{"HOME",VK_HOME},{"END",VK_END},
        {"PAGEUP",VK_PRIOR},{"PAGEDOWN",VK_NEXT},{"INSERT",VK_INSERT},
        {"F1",VK_F1},{"F2",VK_F2},{"F3",VK_F3},{"F4",VK_F4},{"F5",VK_F5},{"F6",VK_F6},
        {"F7",VK_F7},{"F8",VK_F8},{"F9",VK_F9},{"F10",VK_F10},{"F11",VK_F11},{"F12",VK_F12},
        {NULL,0}
    };
    int i;
    if (!name || !*name) return 0;
    if (strlen(name) == 1U) {
        unsigned char ch=(unsigned char)name[0];
        if (ch >= 'a' && ch <= 'z') ch=(unsigned char)(ch-'a'+'A');
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) return (int)ch;
    }
    for (i=0; keys[i].name; ++i) if (equal_ci(keys[i].name,name)==0) return keys[i].code;
    return 0;
}
static int mouse_code(const char *name) {
    if (!name) return -1;
    if (equal_ci(name,"LEFT")==0) return 0;
    if (equal_ci(name,"RIGHT")==0) return 1;
    if (equal_ci(name,"MIDDLE")==0) return 2;
    return -1;
}
static void poll_events(void) {
    MSG message;
    POINT point;
    int i;
    if (!g_window) { g_running=0; return; }
    while (PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
        if (message.message == WM_QUIT) g_running=0;
        TranslateMessage(&message); DispatchMessageA(&message);
    }
    if (!g_window) g_running=0;
    for (i=0;i<256;i++) {
        g_keys_before[i]=g_keys_now[i];
        g_keys_now[i]=(GetAsyncKeyState(i)&0x8000) ? 1 : 0;
    }
    g_mouse_before[0]=g_mouse_now[0]; g_mouse_before[1]=g_mouse_now[1]; g_mouse_before[2]=g_mouse_now[2];
    g_mouse_now[0]=(GetAsyncKeyState(VK_LBUTTON)&0x8000) ? 1 : 0;
    g_mouse_now[1]=(GetAsyncKeyState(VK_RBUTTON)&0x8000) ? 1 : 0;
    g_mouse_now[2]=(GetAsyncKeyState(VK_MBUTTON)&0x8000) ? 1 : 0;
    if (g_window && GetCursorPos(&point)) { ScreenToClient(g_window,&point); g_mouse_x=point.x; g_mouse_y=point.y; }
    update_timing();
}
static int draw_textured(IDirect3DTexture9 *texture, float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1) {
    struct PDXVertex v[4];
    if (!g_device || !g_scene || !texture || w<=0.0f || h<=0.0f) return 0;
    v[0].x=x;v[0].y=y;v[0].z=0.0f;v[0].rhw=1.0f;v[0].color=0xFFFFFFFFUL;v[0].u=u0;v[0].v=v0;
    v[1].x=x+w;v[1].y=y;v[1].z=0.0f;v[1].rhw=1.0f;v[1].color=0xFFFFFFFFUL;v[1].u=u1;v[1].v=v0;
    v[2].x=x;v[2].y=y+h;v[2].z=0.0f;v[2].rhw=1.0f;v[2].color=0xFFFFFFFFUL;v[2].u=u0;v[2].v=v1;
    v[3].x=x+w;v[3].y=y+h;v[3].z=0.0f;v[3].rhw=1.0f;v[3].color=0xFFFFFFFFUL;v[3].u=u1;v[3].v=v1;
    setup_textured();
    g_device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    g_device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    g_device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
    g_device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    g_device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    g_device->SetTexture(0,texture);
    return SUCCEEDED(g_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(struct PDXVertex))) ? 1 : 0;
}
static int create_text_texture(struct PDXTextEntry *entry, const char *text, int r, int g, int b, int size) {
    HDC dc=NULL;
    HBITMAP dib=NULL, old_bitmap=NULL;
    HFONT font=NULL, old_font=NULL;
    BITMAPINFO info;
    unsigned char *pixels=NULL;
    SIZE extent;
    D3DLOCKED_RECT lock;
    int width,height,x,y;
    DWORD *dst;
    if (!g_device || !text) return 0;
    dc=CreateCompatibleDC(NULL); if (!dc) return 0;
    font=CreateFontA(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Arial");
    if (!font) { DeleteDC(dc); return 0; }
    old_font=(HFONT)SelectObject(dc,font);
    if (!GetTextExtentPoint32A(dc,text,(int)strlen(text),&extent)) { SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);return 0; }
    width=extent.cx+6; height=extent.cy+6;
    if (width<8) width=8; if (height<8) height=8;
    if (width>2048 || height>256) { SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);return 0; }
    memset(&info,0,sizeof(info)); info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height; info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    if (!dib || !pixels) { if(dib)DeleteObject(dib);SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);return 0; }
    old_bitmap=(HBITMAP)SelectObject(dc,dib);
    memset(pixels,0,(size_t)width*(size_t)height*4U);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    TextOutA(dc,3,3,text,(int)strlen(text));
    if (FAILED(g_device->CreateTexture((UINT)width,(UINT)height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&entry->texture,NULL))) {
        SelectObject(dc,old_bitmap);DeleteObject(dib);SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);return 0;
    }
    if (FAILED(entry->texture->LockRect(0,&lock,NULL,0))) {
        entry->texture->Release();entry->texture=NULL;SelectObject(dc,old_bitmap);DeleteObject(dib);SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);return 0;
    }
    for (y=0;y<height;y++) {
        const unsigned char *srcrow=pixels+(size_t)y*(size_t)width*4U;
        dst=(DWORD *)((unsigned char *)lock.pBits+(size_t)y*(size_t)lock.Pitch);
        for (x=0;x<width;x++) {
            unsigned int blue=srcrow[x*4], green=srcrow[x*4+1], red=srcrow[x*4+2];
            unsigned int alpha=(blue+green+red)/3U;
            dst[x]=((DWORD)alpha<<24)|((DWORD)r<<16)|((DWORD)g<<8)|(DWORD)b;
        }
    }
    entry->texture->UnlockRect(0);
    entry->width=width;entry->height=height;
    SelectObject(dc,old_bitmap);DeleteObject(dib);SelectObject(dc,old_font);DeleteObject(font);DeleteDC(dc);
    return 1;
}
static struct PDXTextEntry *get_text_entry(const char *text, int r, int g, int b, int size) {
    int i,slot=-1;
    unsigned long oldest=0xFFFFFFFFUL;
    for (i=0;i<PDX_TEXT_CACHE;i++) {
        struct PDXTextEntry *e=&g_text_cache[i];
        if (e->texture && e->r==r && e->g==g && e->b==b && e->size==size && strcmp(e->text,text)==0) {
            e->stamp=g_text_stamp++; return e;
        }
        if (!e->texture) slot=i;
        else if (slot<0 && e->stamp<oldest) { oldest=e->stamp;slot=i; }
    }
    if (slot<0) slot=0;
    if (g_text_cache[slot].texture) g_text_cache[slot].texture->Release();
    memset(&g_text_cache[slot],0,sizeof(g_text_cache[slot]));
    strncpy(g_text_cache[slot].text,text,PDX_TEXT_MAX);g_text_cache[slot].text[PDX_TEXT_MAX]='\0';
    g_text_cache[slot].r=r;g_text_cache[slot].g=g;g_text_cache[slot].b=b;g_text_cache[slot].size=size;
    if (!create_text_texture(&g_text_cache[slot],g_text_cache[slot].text,r,g,b,size)) { memset(&g_text_cache[slot],0,sizeof(g_text_cache[slot]));return NULL; }
    g_text_cache[slot].stamp=g_text_stamp++;
    return &g_text_cache[slot];
}
static int texture_id_valid(int id) { return id>=1 && id<=PDX_MAX_TEXTURES && g_textures[id-1].texture!=NULL; }
static int load_bmp_texture(const char *path, char *error, size_t error_size) {
    HBITMAP bitmap;
    BITMAP bm;
    BITMAPINFO info;
    HDC dc;
    unsigned char *pixels=NULL;
    int i,slot=-1,y,x;
    struct PDXTexture *entry;
    D3DLOCKED_RECT lock;
    if (!g_device) { copy_error(error,error_size,"loadTexture requires an open PDX window");return -1; }
    bitmap=(HBITMAP)LoadImageA(NULL,path,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);
    if (!bitmap) { copy_error(error,error_size,"loadTexture only supports readable Windows BMP files");return -1; }
    if (!GetObjectA(bitmap,sizeof(bm),&bm) || bm.bmWidth<=0 || bm.bmHeight==0 || bm.bmWidth>4096 || bm.bmHeight>4096 || bm.bmHeight < -4096) {
        DeleteObject(bitmap);copy_error(error,error_size,"BMP dimensions are invalid or exceed 4096x4096");return -1;
    }
    for(i=0;i<PDX_MAX_TEXTURES;i++)if(!g_textures[i].texture){slot=i;break;}
    if(slot<0){DeleteObject(bitmap);copy_error(error,error_size,"PDX texture limit reached (64 loaded textures)");return -1;}
    memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bm.bmWidth;
    info.bmiHeader.biHeight=-abs(bm.bmHeight);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    pixels=(unsigned char *)malloc((size_t)bm.bmWidth*(size_t)abs(bm.bmHeight)*4U);
    if(!pixels){DeleteObject(bitmap);copy_error(error,error_size,"not enough memory to load BMP");return -1;}
    dc=GetDC(NULL);
    if(!GetDIBits(dc,bitmap,0,(UINT)abs(bm.bmHeight),pixels,&info,DIB_RGB_COLORS)){ReleaseDC(NULL,dc);free(pixels);DeleteObject(bitmap);copy_error(error,error_size,"GetDIBits failed while loading BMP");return -1;}
    ReleaseDC(NULL,dc);DeleteObject(bitmap);
    entry=&g_textures[slot];
    if(FAILED(g_device->CreateTexture((UINT)bm.bmWidth,(UINT)abs(bm.bmHeight),1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&entry->texture,NULL))){free(pixels);copy_error(error,error_size,"Direct3D could not create BMP texture");return -1;}
    if(FAILED(entry->texture->LockRect(0,&lock,NULL,0))){entry->texture->Release();entry->texture=NULL;free(pixels);copy_error(error,error_size,"Direct3D could not lock BMP texture");return -1;}
    for(y=0;y<abs(bm.bmHeight);y++){
        const unsigned char *src=pixels+(size_t)y*(size_t)bm.bmWidth*4U;
        DWORD *dst=(DWORD *)((unsigned char *)lock.pBits+(size_t)y*(size_t)lock.Pitch);
        for(x=0;x<bm.bmWidth;x++){
            unsigned int blue=src[x*4],green=src[x*4+1],red=src[x*4+2],alpha=src[x*4+3];
            if(red==255U && green==0U && blue==255U) alpha=0U;
            else if(alpha==0U) alpha=255U;
            dst[x]=((DWORD)alpha<<24)|((DWORD)red<<16)|((DWORD)green<<8)|(DWORD)blue;
        }
    }
    entry->texture->UnlockRect(0);free(pixels);entry->width=bm.bmWidth;entry->height=abs(bm.bmHeight);return slot+1;
}
static int has_function(const char *name) {
    static const char *names[] = {
        "open","beginFrame","fillRect","drawRect","drawLine","fillCircle","drawCircle","endFrame",
        "pollEvents","setTitle","close","keyDown","keyPressed","keyReleased","mouseX","mouseY",
        "mouseDown","mousePressed","deltaTime","time","setTargetFPS","setCamera","resetCamera","drawText",
        "loadTexture","drawTexture","drawTextureRegion","drawTilemap","freeTexture","textureWidth","textureHeight",
        "playSound","stopSound","rectsOverlap","clamp","randomInt","distance",NULL
    };
    int i;
    for(i=0;names[i];i++)if(strcmp(name,names[i])==0)return 1;
    return 0;
}
static int call_function(const PipiHostAPI *host,const char *name,int argc,const PipiValue *argv,
                         PipiValue *result,char *error,size_t error_size) {
    (void)host;
    set_null(result);
    if(strcmp(name,"open")==0){
        if(argc!=3||argv[0].type!=PIPI_VALUE_STRING||!all_numbers(2,argv+1)){copy_error(error,error_size,"open(title,width,height) expects string,number,number");return 0;}
        if(!open_window(argv[0].string,(int)argv[1].number,(int)argv[2].number,error,error_size))return 0;
        set_bool(result,1);return 1;
    }
    if(strcmp(name,"close")==0){if(argc!=0){copy_error(error,error_size,"close() expects no arguments");return 0;}if(g_window)DestroyWindow(g_window);g_window=NULL;g_running=0;release_graphics();PlaySoundA(NULL,NULL,0);return 1;}
    if(strcmp(name,"pollEvents")==0){if(argc!=0){copy_error(error,error_size,"pollEvents() expects no arguments");return 0;}poll_events();set_bool(result,g_running);return 1;}
    if(strcmp(name,"beginFrame")==0){
        HRESULT hr;int r,g,b;
        if(argc!=3||!all_numbers(argc,argv)||!g_device){copy_error(error,error_size,"beginFrame(r,g,b) requires an open PDX window and three numbers");return 0;}
        if(!recover_device()){set_bool(result,0);return 1;}
        r=(int)argv[0].number;g=(int)argv[1].number;b=(int)argv[2].number;
        hr=g_device->Clear(0,NULL,D3DCLEAR_TARGET,rgb_color(r,g,b),1.0f,0);
        if(FAILED(hr)||FAILED(g_device->BeginScene())){copy_error(error,error_size,"Direct3D failed to begin frame");return 0;}
        g_scene=1;qpc_now(&g_frame_start);set_bool(result,1);return 1;
    }
    if(strcmp(name,"fillRect")==0||strcmp(name,"drawRect")==0){
        float x,y,w,h;int r,g,b,ok;
        if(argc!=7||!all_numbers(argc,argv)||!g_scene){copy_error(error,error_size,"fillRect/drawRect(x,y,w,h,r,g,b) requires an active frame and seven numbers");return 0;}
        camera_point(argv[0].number,argv[1].number,&x,&y);w=(float)(argv[2].number*g_camera_zoom);h=(float)(argv[3].number*g_camera_zoom);
        if(w<=0||h<=0){copy_error(error,error_size,"rectangle dimensions must be positive");return 0;}
        r=(int)argv[4].number;g=(int)argv[5].number;b=(int)argv[6].number;
        if(strcmp(name,"fillRect")==0)ok=draw_filled_rect_raw(x,y,w,h,rgb_color(r,g,b));
        else {float t=1.0f*g_camera_zoom;ok=draw_filled_rect_raw(x,y,w,t,rgb_color(r,g,b));ok=draw_filled_rect_raw(x,y+h-t,w,t,rgb_color(r,g,b))&&ok;ok=draw_filled_rect_raw(x,y,t,h,rgb_color(r,g,b))&&ok;ok=draw_filled_rect_raw(x+w-t,y,t,h,rgb_color(r,g,b))&&ok;}
        if(!ok){copy_error(error,error_size,"Direct3D rectangle drawing failed");return 0;}set_bool(result,1);return 1;
    }
    if(strcmp(name,"drawLine")==0){float x1,y1,x2,y2; if(argc!=7||!all_numbers(argc,argv)||!g_scene){copy_error(error,error_size,"drawLine(x1,y1,x2,y2,r,g,b) requires an active frame");return 0;}camera_point(argv[0].number,argv[1].number,&x1,&y1);camera_point(argv[2].number,argv[3].number,&x2,&y2);if(!draw_line_raw(x1,y1,x2,y2,rgb_color((int)argv[4].number,(int)argv[5].number,(int)argv[6].number))){copy_error(error,error_size,"Direct3D line drawing failed");return 0;}set_bool(result,1);return 1;}
    if(strcmp(name,"fillCircle")==0||strcmp(name,"drawCircle")==0){
        struct PDXVertex v[66];float x,y,rad;int i,segments=32;D3DCOLOR color;D3DPRIMITIVETYPE primitive;
        if(argc!=6||!all_numbers(argc,argv)||!g_scene){copy_error(error,error_size,"fillCircle/drawCircle(x,y,radius,r,g,b) requires an active frame");return 0;}
        camera_point(argv[0].number,argv[1].number,&x,&y);rad=(float)(argv[2].number*g_camera_zoom);if(rad<=0){copy_error(error,error_size,"circle radius must be positive");return 0;}color=rgb_color((int)argv[3].number,(int)argv[4].number,(int)argv[5].number);
        if(strcmp(name,"fillCircle")==0){v[0].x=x;v[0].y=y;v[0].z=0;v[0].rhw=1;v[0].color=color;v[0].u=0;v[0].v=0;for(i=0;i<=segments;i++){double a=(2.0*PDX_PI*(double)i)/(double)segments;v[i+1].x=x+(float)cos(a)*rad;v[i+1].y=y+(float)sin(a)*rad;v[i+1].z=0;v[i+1].rhw=1;v[i+1].color=color;v[i+1].u=0;v[i+1].v=0;}primitive=D3DPT_TRIANGLEFAN;setup_untextured();if(FAILED(g_device->DrawPrimitiveUP(primitive,segments,v,sizeof(struct PDXVertex)))){copy_error(error,error_size,"Direct3D circle drawing failed");return 0;}}
        else {for(i=0;i<=segments;i++){double a=(2.0*PDX_PI*(double)i)/(double)segments;v[i].x=x+(float)cos(a)*rad;v[i].y=y+(float)sin(a)*rad;v[i].z=0;v[i].rhw=1;v[i].color=color;v[i].u=0;v[i].v=0;}setup_untextured();if(FAILED(g_device->DrawPrimitiveUP(D3DPT_LINESTRIP,segments,v,sizeof(struct PDXVertex)))){copy_error(error,error_size,"Direct3D circle drawing failed");return 0;}}
        set_bool(result,1);return 1;
    }
    if(strcmp(name,"endFrame")==0){
        HRESULT hr;LARGE_INTEGER now;double frame_seconds, target_seconds;
        if(argc!=0||!g_device||!g_scene){copy_error(error,error_size,"endFrame() requires an active frame");return 0;}
        g_scene=0;hr=g_device->EndScene();if(FAILED(hr)){copy_error(error,error_size,"Direct3D EndScene failed");return 0;}
        hr=g_device->Present(NULL,NULL,NULL,NULL);if(FAILED(hr)&&hr!=D3DERR_DEVICELOST&&hr!=D3DERR_DEVICENOTRESET){copy_error(error,error_size,"Direct3D Present failed");return 0;}
        if(g_target_fps>0&&qpc_now(&now)){frame_seconds=qpc_seconds(now,g_frame_start);target_seconds=1.0/(double)g_target_fps;if(frame_seconds<target_seconds){DWORD sleep_ms=(DWORD)((target_seconds-frame_seconds)*1000.0);if(sleep_ms>0)Sleep(sleep_ms);}}
        set_bool(result,1);return 1;
    }
    if(strcmp(name,"setTitle")==0){if(!one_string(argc,argv)||!g_window){copy_error(error,error_size,"setTitle(title) requires an open PDX window");return 0;}SetWindowTextA(g_window,argv[0].string?argv[0].string:"PDX");set_bool(result,1);return 1;}
    if(strcmp(name,"keyDown")==0||strcmp(name,"keyPressed")==0||strcmp(name,"keyReleased")==0){int code;if(!one_string(argc,argv)){copy_error(error,error_size,"keyDown/keyPressed/keyReleased expect a key name string");return 0;}code=key_code(argv[0].string);if(!code){copy_error(error,error_size,"unknown key; use A-Z, 0-9, arrows, SPACE, ESCAPE, ENTER, SHIFT, CTRL, ALT or F1-F12");return 0;}if(strcmp(name,"keyDown")==0)set_bool(result,g_keys_now[code]);else if(strcmp(name,"keyPressed")==0)set_bool(result,g_keys_now[code]&&!g_keys_before[code]);else set_bool(result,!g_keys_now[code]&&g_keys_before[code]);return 1;}
    if(strcmp(name,"mouseX")==0||strcmp(name,"mouseY")==0){if(argc!=0){copy_error(error,error_size,"mouseX/mouseY expect no arguments");return 0;}set_num(result,strcmp(name,"mouseX")==0?g_mouse_x:g_mouse_y);return 1;}
    if(strcmp(name,"mouseDown")==0||strcmp(name,"mousePressed")==0){int code;if(!one_string(argc,argv)){copy_error(error,error_size,"mouseDown/mousePressed expect LEFT, RIGHT or MIDDLE");return 0;}code=mouse_code(argv[0].string);if(code<0){copy_error(error,error_size,"mouse button must be LEFT, RIGHT or MIDDLE");return 0;}set_bool(result,strcmp(name,"mouseDown")==0?g_mouse_now[code]:(g_mouse_now[code]&&!g_mouse_before[code]));return 1;}
    if(strcmp(name,"deltaTime")==0||strcmp(name,"time")==0){if(argc!=0){copy_error(error,error_size,"deltaTime/time expect no arguments");return 0;}set_num(result,strcmp(name,"deltaTime")==0?g_delta_time:g_elapsed_time);return 1;}
    if(strcmp(name,"setTargetFPS")==0){if(argc!=1||!all_numbers(argc,argv)){copy_error(error,error_size,"setTargetFPS(fps) expects one number");return 0;}g_target_fps=(int)argv[0].number;if(g_target_fps<0)g_target_fps=0;if(g_target_fps>1000)g_target_fps=1000;set_bool(result,1);return 1;}
    if(strcmp(name,"setCamera")==0){if(argc!=3||!all_numbers(argc,argv)){copy_error(error,error_size,"setCamera(x,y,zoom) expects three numbers");return 0;}if(argv[2].number<=0.0||argv[2].number>16.0){copy_error(error,error_size,"camera zoom must be in (0,16]");return 0;}g_camera_x=argv[0].number;g_camera_y=argv[1].number;g_camera_zoom=argv[2].number;set_bool(result,1);return 1;}
    if(strcmp(name,"resetCamera")==0){if(argc!=0){copy_error(error,error_size,"resetCamera() expects no arguments");return 0;}g_camera_x=0;g_camera_y=0;g_camera_zoom=1;set_bool(result,1);return 1;}
    if(strcmp(name,"drawText")==0){
        struct PDXTextEntry *entry;float x,y,w,h;int r,g,b,size;
        if(argc!=7||argv[0].type!=PIPI_VALUE_STRING||!all_numbers(6,argv+1)||!g_scene){copy_error(error,error_size,"drawText(text,x,y,r,g,b,size) requires string, six numbers and an active frame");return 0;}
        r=clamp_int((int)argv[3].number,0,255);g=clamp_int((int)argv[4].number,0,255);b=clamp_int((int)argv[5].number,0,255);size=clamp_int((int)argv[6].number,8,96);
        if (strlen(argv[0].string ? argv[0].string : "") > PDX_TEXT_MAX) { copy_error(error,error_size,"drawText text is limited to 255 bytes"); return 0; }
        entry=get_text_entry(argv[0].string?argv[0].string:"",r,g,b,size);if(!entry){copy_error(error,error_size,"failed to rasterize text (ASCII font cache supports up to 255 bytes)");return 0;}
        camera_point(argv[1].number,argv[2].number,&x,&y);w=(float)(entry->width*g_camera_zoom);h=(float)(entry->height*g_camera_zoom);
        if(!draw_textured(entry->texture,x,y,w,h,0,0,1,1)){copy_error(error,error_size,"failed to draw PDX text");return 0;}set_bool(result,1);return 1;
    }
    if(strcmp(name,"loadTexture")==0){int id;if(!one_string(argc,argv)){copy_error(error,error_size,"loadTexture(path) expects one string");return 0;}id=load_bmp_texture(argv[0].string,error,error_size);if(id<0)return 0;set_num(result,id);return 1;}
    if(strcmp(name,"drawTexture")==0){
        int id;float x,y,w,h; if(argc!=5||!all_numbers(4,argv+1)||!g_scene){copy_error(error,error_size,"drawTexture(id,x,y,w,h) requires five arguments and active frame");return 0;}if(argv[0].type!=PIPI_VALUE_NUMBER){copy_error(error,error_size,"texture id must be numeric");return 0;}id=(int)argv[0].number;if(!texture_id_valid(id)){copy_error(error,error_size,"invalid or freed PDX texture id");return 0;}camera_point(argv[1].number,argv[2].number,&x,&y);w=(float)(argv[3].number*g_camera_zoom);h=(float)(argv[4].number*g_camera_zoom);if(!draw_textured(g_textures[id-1].texture,x,y,w,h,0,0,1,1)){copy_error(error,error_size,"failed to draw texture");return 0;}set_bool(result,1);return 1;
    }
    if(strcmp(name,"drawTextureRegion")==0){
        int id;float x,y,w,h;double u,v,uw,vh;
        if(argc!=9||argv[0].type!=PIPI_VALUE_NUMBER||!all_numbers(8,argv+1)||!g_scene){copy_error(error,error_size,"drawTextureRegion(id,x,y,w,h,u,v,uw,vh) expects nine numbers and active frame");return 0;}
        id=(int)argv[0].number;if(!texture_id_valid(id)){copy_error(error,error_size,"invalid or freed PDX texture id");return 0;}
        camera_point(argv[1].number,argv[2].number,&x,&y);w=(float)(argv[3].number*g_camera_zoom);h=(float)(argv[4].number*g_camera_zoom);u=argv[5].number;v=argv[6].number;uw=argv[7].number;vh=argv[8].number;
        if(u<0||v<0||uw<=0||vh<=0||u+uw>1.00001||v+vh>1.00001){copy_error(error,error_size,"UV coordinates must stay within 0..1");return 0;}
        if(!draw_textured(g_textures[id-1].texture,x,y,w,h,(float)u,(float)v,(float)(u+uw),(float)(v+vh))){copy_error(error,error_size,"failed to draw texture region");return 0;}set_bool(result,1);return 1;
    }
    if(strcmp(name,"drawTilemap")==0){
        int id,cols,rows,tile_px,tile_cols,tile_rows,cells,i,drawn=0;
        double origin_x,origin_y,scale;
        const char *map;
        if(argc!=8||argv[0].type!=PIPI_VALUE_NUMBER||argv[1].type!=PIPI_VALUE_STRING||!all_numbers(6,argv+2)||!g_scene){copy_error(error,error_size,"drawTilemap(id,map,cols,rows,tilePixels,x,y,scale) expects id, string, six numbers and an active frame");return 0;}
        id=(int)argv[0].number;map=argv[1].string?argv[1].string:"";cols=(int)argv[2].number;rows=(int)argv[3].number;tile_px=(int)argv[4].number;origin_x=argv[5].number;origin_y=argv[6].number;scale=argv[7].number;
        if(!texture_id_valid(id)){copy_error(error,error_size,"invalid or freed PDX atlas texture id");return 0;}
        if(cols<1||rows<1||cols>256||rows>256||cols*rows>16384||tile_px<1||tile_px>1024||scale<=0.0||scale>16.0){copy_error(error,error_size,"tilemap limits: cols/rows 1..256, at most 16384 cells, tile pixels 1..1024, scale in (0,16]");return 0;}
        cells=cols*rows;if(strlen(map)<(size_t)cells){copy_error(error,error_size,"tilemap string is shorter than cols * rows; use 0-9/A-F for tiles and '.' for empty cells");return 0;}
        if(g_textures[id-1].width%tile_px!=0||g_textures[id-1].height%tile_px!=0){copy_error(error,error_size,"tile pixel size must divide both atlas dimensions exactly");return 0;}
        tile_cols=g_textures[id-1].width/tile_px;tile_rows=g_textures[id-1].height/tile_px;
        for(i=0;i<cells;i++){
            unsigned char ch=(unsigned char)map[i];int tile=-1;
            if(ch=='.'||ch==' ')continue;
            if(ch>='0'&&ch<='9')tile=(int)(ch-'0');
            else {ch=(unsigned char)toupper(ch);if(ch>='A'&&ch<='F')tile=10+(int)(ch-'A');}
            if(tile<0||tile>=tile_cols*tile_rows){copy_error(error,error_size,"tilemap contains an invalid tile id for the atlas; use 0-9/A-F or '.'");return 0;}
        }
        for(i=0;i<cells;i++){
            unsigned char ch=(unsigned char)map[i];int tile=-1,row=i/cols,col=i%cols;float sx,sy,sw,sh,u0,v0,uv_w,uv_h;double wx,wy;
            if(ch=='.'||ch==' ')continue;
            if(ch>='0'&&ch<='9')tile=(int)(ch-'0');else {ch=(unsigned char)toupper(ch);tile=10+(int)(ch-'A');}
            wx=origin_x+(double)col*(double)tile_px*scale;wy=origin_y+(double)row*(double)tile_px*scale;camera_point(wx,wy,&sx,&sy);
            sw=(float)((double)tile_px*scale*g_camera_zoom);sh=sw;
            u0=(float)((tile%tile_cols*tile_px)/(double)g_textures[id-1].width);v0=(float)((tile/tile_cols*tile_px)/(double)g_textures[id-1].height);
            uv_w=(float)((double)tile_px/(double)g_textures[id-1].width);uv_h=(float)((double)tile_px/(double)g_textures[id-1].height);
            if(!draw_textured(g_textures[id-1].texture,sx,sy,sw,sh,u0,v0,u0+uv_w,v0+uv_h)){copy_error(error,error_size,"Direct3D failed while rendering tilemap");return 0;}drawn++;
        }
        set_num(result,drawn);return 1;
    }
    if(strcmp(name,"freeTexture")==0){int id;if(argc!=1||!all_numbers(argc,argv)){copy_error(error,error_size,"freeTexture(id) expects one number");return 0;}id=(int)argv[0].number;if(!texture_id_valid(id)){copy_error(error,error_size,"invalid or freed PDX texture id");return 0;}g_textures[id-1].texture->Release();g_textures[id-1].texture=NULL;g_textures[id-1].width=0;g_textures[id-1].height=0;set_bool(result,1);return 1;}
    if(strcmp(name,"textureWidth")==0||strcmp(name,"textureHeight")==0){int id;if(argc!=1||!all_numbers(argc,argv)){copy_error(error,error_size,"textureWidth/textureHeight expect one numeric texture id");return 0;}id=(int)argv[0].number;if(!texture_id_valid(id)){copy_error(error,error_size,"invalid or freed PDX texture id");return 0;}set_num(result,strcmp(name,"textureWidth")==0?g_textures[id-1].width:g_textures[id-1].height);return 1;}
    if(strcmp(name,"playSound")==0){int loop;if(argc!=2||argv[0].type!=PIPI_VALUE_STRING||argv[1].type!=PIPI_VALUE_BOOLEAN){copy_error(error,error_size,"playSound(path, loop) expects a string and boolean; WAV files are supported");return 0;}loop=argv[1].boolean;set_bool(result,PlaySoundA(argv[0].string,NULL,SND_FILENAME|SND_ASYNC|(loop?SND_LOOP:0))?1:0);return 1;}
    if(strcmp(name,"stopSound")==0){if(argc!=0){copy_error(error,error_size,"stopSound() expects no arguments");return 0;}set_bool(result,PlaySoundA(NULL,NULL,0)?1:0);return 1;}
    if(strcmp(name,"rectsOverlap")==0){double ax,ay,aw,ah,bx,by,bw,bh;if(argc!=8||!all_numbers(argc,argv)){copy_error(error,error_size,"rectsOverlap(ax,ay,aw,ah,bx,by,bw,bh) expects eight numbers");return 0;}ax=argv[0].number;ay=argv[1].number;aw=argv[2].number;ah=argv[3].number;bx=argv[4].number;by=argv[5].number;bw=argv[6].number;bh=argv[7].number;set_bool(result,(aw>0&&ah>0&&bw>0&&bh>0&&ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by));return 1;}
    if(strcmp(name,"clamp")==0){double x,low,high;if(argc!=3||!all_numbers(argc,argv)){copy_error(error,error_size,"clamp(value,min,max) expects three numbers");return 0;}x=argv[0].number;low=argv[1].number;high=argv[2].number;if(low>high){double t=low;low=high;high=t;}if(x<low)x=low;if(x>high)x=high;set_num(result,x);return 1;}
    if(strcmp(name,"randomInt")==0){int low,high,t;if(argc!=2||!all_numbers(argc,argv)){copy_error(error,error_size,"randomInt(min,max) expects two numbers");return 0;}low=(int)argv[0].number;high=(int)argv[1].number;if(low>high){t=low;low=high;high=t;}if((long)high-(long)low>2000000000L){copy_error(error,error_size,"randomInt range is too wide");return 0;}set_num(result,low+(int)((unsigned long)rand()%(unsigned long)((long)high-(long)low+1L)));return 1;}
    if(strcmp(name,"distance")==0){double dx,dy;if(argc!=4||!all_numbers(argc,argv)){copy_error(error,error_size,"distance(x1,y1,x2,y2) expects four numbers");return 0;}dx=argv[2].number-argv[0].number;dy=argv[3].number-argv[1].number;set_num(result,sqrt(dx*dx+dy*dy));return 1;}
    copy_error(error,error_size,"unknown PDX function");return 0;
}
static int initialize(const PipiHostAPI *host,char *error,size_t error_size) {
    (void)error;(void)error_size;
    if(!host||host->abi_version!=PIPI_NATIVE_ABI_VERSION)return 0;
    memset(g_textures,0,sizeof(g_textures));memset(g_text_cache,0,sizeof(g_text_cache));
    memset(g_keys_now,0,sizeof(g_keys_now));memset(g_keys_before,0,sizeof(g_keys_before));
    memset(g_mouse_now,0,sizeof(g_mouse_now));memset(g_mouse_before,0,sizeof(g_mouse_before));
    g_text_stamp=1UL;return 1;
}
static void shutdown_module(const PipiHostAPI *host) {
    (void)host;
    if(g_window)DestroyWindow(g_window);g_window=NULL;g_running=0;release_graphics();PlaySoundA(NULL,NULL,0);
}
static const PipiNativeModuleV1 module_api = {
    PIPI_NATIVE_ABI_VERSION,"pdx","0.8.0",initialize,has_function,call_function,shutdown_module
};
extern "C" PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void) { return &module_api; }
