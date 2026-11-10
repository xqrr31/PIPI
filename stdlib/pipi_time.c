/* PIPI clock/time module - C89 with platform-specific wait primitive. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../include/pipi_native.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/select.h>
#endif
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void num(PipiValue *r,double v){r->type=PIPI_VALUE_NUMBER;r->number=v;r->string="";r->boolean=0;}
static int has_function(const char *n){return strcmp(n,"timeUnix")==0||strcmp(n,"timeMillis")==0||strcmp(n,"sleepMs")==0||strcmp(n,"timeFormat") == 0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){time_t now;static char buffer[64];struct tm *parts;(void)h;num(r,0);
 if(strcmp(n,"timeUnix")==0){if(c!=0){errcopy(e,es,"timeUnix() expects no arguments");return 0;}now=time(NULL);num(r,(double)now);return 1;}
 if(strcmp(n,"timeMillis")==0){if(c!=0){errcopy(e,es,"timeMillis() expects no arguments");return 0;}
#ifdef _WIN32
  num(r,(double)GetTickCount());
#else
  {struct timeval tv;if(gettimeofday(&tv,NULL)!=0){errcopy(e,es,"timeMillis gettimeofday failed");return 0;}num(r,(double)tv.tv_sec*1000.0+(double)tv.tv_usec/1000.0);}
#endif
  return 1;}
 if(strcmp(n,"sleepMs")==0){long ms;if(c!=1){errcopy(e,es,"sleepMs(milliseconds) expects 1 argument");return 0;}ms=(long)a[0].number;if(ms<0L)ms=0L;if(ms>86400000L)ms=86400000L;
#ifdef _WIN32
 Sleep((DWORD)ms);
#else
 {struct timeval tv;tv.tv_sec=ms/1000L;tv.tv_usec=(ms%1000L)*1000L;select(0,NULL,NULL,NULL,&tv);}
#endif
 num(r,(double)ms);return 1;}
 if(strcmp(n,"timeFormat")==0){if(c!=1){errcopy(e,es,"timeFormat(unixSeconds) expects 1 argument");return 0;}now=(time_t)a[0].number;parts=localtime(&now);if(!parts){errcopy(e,es,"timeFormat cannot represent timestamp");return 0;}if(strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",parts)==0){errcopy(e,es,"timeFormat output overflow");return 0;}r->type=PIPI_VALUE_STRING;r->number=0;r->string=buffer;r->boolean=0;return 1;}
 errcopy(e,es,"unknown pipi/time function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/time","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
