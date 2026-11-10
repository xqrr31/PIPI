/* Small portable operating-system introspection module. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/pipi_native.h"
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PIPI_GETCWD(b,n) _getcwd((b),(int)(n))
#define PIPI_PID() ((double)GetCurrentProcessId())
#else
#include <unistd.h>
#define PIPI_GETCWD(b,n) getcwd((b),(n))
#define PIPI_PID() ((double)getpid())
#endif
static char buffer[4096];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static void setnum(PipiValue *r,double v){r->type=PIPI_VALUE_NUMBER;r->number=v;r->string="";r->boolean=0;}
static int has_function(const char *n){return strcmp(n,"osName")==0||strcmp(n,"architecture")==0||strcmp(n,"envGet")==0||strcmp(n,"currentDirectory")==0||strcmp(n,"processId")==0||strcmp(n,"pathSeparator")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const char *v;(void)h;
 if(strcmp(n,"osName")==0){if(c){errcopy(e,es,"osName() expects no arguments");return 0;}
#ifdef _WIN32
 setstr(r,"windows");
#elif defined(__APPLE__)
 setstr(r,"macos");
#elif defined(__linux__)
 setstr(r,"linux");
#else
 setstr(r,"unix");
#endif
 return 1;}
 if(strcmp(n,"architecture")==0){if(c){errcopy(e,es,"architecture() expects no arguments");return 0;}
#if defined(__x86_64__) || defined(_M_X64)
 setstr(r,"x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
 setstr(r,"aarch64");
#elif defined(__i386__) || defined(_M_IX86)
 setstr(r,"x86");
#else
 setstr(r,"unknown");
#endif
 return 1;}
 if(strcmp(n,"pathSeparator")==0){if(c){errcopy(e,es,"pathSeparator() expects no arguments");return 0;}
#ifdef _WIN32
 setstr(r,"\\");
#else
 setstr(r,"/");
#endif
 return 1;}
 if(strcmp(n,"processId")==0){if(c){errcopy(e,es,"processId() expects no arguments");return 0;}setnum(r,PIPI_PID());return 1;}
 if(strcmp(n,"envGet")==0){if(c!=1){errcopy(e,es,"envGet(name) expects 1 argument");return 0;}v=getenv(a[0].string?a[0].string:"");setstr(r,v?v:"");return 1;}
 if(strcmp(n,"currentDirectory")==0){if(c){errcopy(e,es,"currentDirectory() expects no arguments");return 0;}if(!PIPI_GETCWD(buffer,sizeof(buffer))){errcopy(e,es,"could not read current directory");return 0;}setstr(r,buffer);return 1;}
 errcopy(e,es,"unknown pipi/system function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/system","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
