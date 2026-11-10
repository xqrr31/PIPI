/* Portable path string helpers (no filesystem access). */
#include <string.h>
#include "../include/pipi_native.h"
#define PATH_CAP 4096U
static char output[PATH_CAP];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static const char *arg(const PipiValue *a,int i){return a[i].string?a[i].string:"";}
static int has_function(const char *n){return strcmp(n,"pathJoin")==0||strcmp(n,"pathBasename")==0||strcmp(n,"pathDirname")==0||strcmp(n,"pathExtension")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const char *s,*last,*p;(void)h;output[0]='\0';
 if(strcmp(n,"pathJoin")==0){size_t al,bl;if(c!=2){errcopy(e,es,"pathJoin(left, right) expects 2 arguments");return 0;}s=arg(a,0);p=arg(a,1);al=strlen(s);bl=strlen(p);if(al+bl+2U>=sizeof(output)){errcopy(e,es,"joined path too long");return 0;}strcpy(output,s);if(al&&output[al-1]!='/'&&output[al-1]!='\\'&&bl&&p[0]!='/'&&p[0]!='\\'){output[al++]='/';output[al]='\0';}strcat(output,p);setstr(r,output);return 1;}
 if(c!=1){errcopy(e,es,"path helper expects 1 argument");return 0;}s=arg(a,0);last=NULL;for(p=s;*p;p++)if(*p=='/'||*p=='\\')last=p;
 if(strcmp(n,"pathBasename")==0){setstr(r,last?last+1:s);return 1;}
 if(strcmp(n,"pathDirname")==0){if(!last){setstr(r,".");return 1;}if(last==s){output[0]=*last;output[1]='\0';}else{memcpy(output,s,(size_t)(last-s));output[last-s]='\0';}setstr(r,output);return 1;}
 if(strcmp(n,"pathExtension")==0){const char *base=last?last+1:s;const char *dot=strrchr(base,'.');setstr(r,dot?dot:"");return 1;}
 errcopy(e,es,"unknown pipi/path function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/path","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
