/* Example third-party PIPIwood module: C89, ABI v1. */
#include <stdio.h>
#include <string.h>
#include "../../include/pipi_native.h"
static char greeting[1024];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static int has_function(const char *n){return strcmp(n,"greet")==0||strcmp(n,"doubleValue")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){(void)h;
 if(strcmp(n,"greet")==0){const char *name;if(c!=1){errcopy(e,es,"greet(name) expects one argument");return 0;}name=a[0].string?a[0].string:"friend";sprintf(greeting,"Hello, %.980s! Welcome to PIPIwood.",name);r->type=PIPI_VALUE_STRING;r->number=0;r->string=greeting;r->boolean=0;return 1;}
 if(strcmp(n,"doubleValue")==0){if(c!=1||a[0].type!=PIPI_VALUE_NUMBER){errcopy(e,es,"doubleValue(number) expects one number");return 0;}r->type=PIPI_VALUE_NUMBER;r->number=a[0].number*2.0;r->string="";r->boolean=0;return 1;}
 errcopy(e,es,"unknown acme/greeting export");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)e;(void)n;return h&&h->abi_version==PIPI_NATIVE_ABI_VERSION;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"acme/greeting","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
