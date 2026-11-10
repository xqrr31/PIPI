/* PIPI filesystem module - portable C89 text/file operations. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/pipi_native.h"
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define PIPI_MKDIR(p) _mkdir(p)
#define PIPI_ACCESS(p) _access(p,0)
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define PIPI_MKDIR(p) mkdir(p,0755)
#define PIPI_ACCESS(p) access(p,F_OK)
#endif
#define FS_MAX_READ (1024UL*1024UL)
static char fs_buffer[FS_MAX_READ+1UL];
static char fs_error[256];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void set_num(PipiValue *r,double n){r->type=PIPI_VALUE_NUMBER;r->number=n;r->string="";r->boolean=0;}
static void set_bool(PipiValue *r,int b){r->type=PIPI_VALUE_BOOLEAN;r->number=0;r->string="";r->boolean=b?1:0;}
static void set_str(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static const char *argstr(const PipiValue *a,int i){return a[i].string?a[i].string:"";}
static int has_function(const char *n){static const char *names[]={"fileExists","fileRead","fileWrite","fileAppend","fileDelete","fileSize","fileCopy","fileMove","directoryCreate",NULL};int i;for(i=0;names[i];i++)if(strcmp(n,names[i])==0)return 1;return 0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){FILE *f;long size;size_t got;char chunk[8192];(void)h;set_num(r,0);
 if(strcmp(n,"fileExists")==0){if(c!=1){errcopy(e,es,"fileExists(path) expects 1 argument");return 0;}set_bool(r,PIPI_ACCESS(argstr(a,0))==0);return 1;}
 if(strcmp(n,"fileRead")==0){if(c!=1){errcopy(e,es,"fileRead(path) expects 1 argument");return 0;}f=fopen(argstr(a,0),"rb");if(!f){errcopy(e,es,"fileRead could not open file");return 0;}got=fread(fs_buffer,1U,(size_t)FS_MAX_READ,f);if(ferror(f)){fclose(f);errcopy(e,es,"fileRead failed");return 0;}fclose(f);fs_buffer[got]='\0';set_str(r,fs_buffer);return 1;}
 if(strcmp(n,"fileWrite")==0||strcmp(n,"fileAppend")==0){if(c!=2){errcopy(e,es,"fileWrite/fileAppend(path, text) expects 2 arguments");return 0;}f=fopen(argstr(a,0),strcmp(n,"fileWrite")==0?"wb":"ab");if(!f){errcopy(e,es,"cannot open file for writing");return 0;}size=strlen(argstr(a,1));got=fwrite(argstr(a,1),1U,(size_t)size,f);if(fclose(f)!=0||got!=(size_t)size){errcopy(e,es,"file write failed");return 0;}set_bool(r,1);return 1;}
 if(strcmp(n,"fileDelete")==0){if(c!=1){errcopy(e,es,"fileDelete(path) expects 1 argument");return 0;}set_bool(r,remove(argstr(a,0))==0);return 1;}
 if(strcmp(n,"fileSize")==0){if(c!=1){errcopy(e,es,"fileSize(path) expects 1 argument");return 0;}f=fopen(argstr(a,0),"rb");if(!f){errcopy(e,es,"fileSize could not open file");return 0;}if(fseek(f,0L,SEEK_END)!=0){fclose(f);errcopy(e,es,"fileSize seek failed");return 0;}size=ftell(f);fclose(f);if(size<0){errcopy(e,es,"fileSize failed");return 0;}set_num(r,(double)size);return 1;}
 if(strcmp(n,"fileCopy")==0){FILE *src,*dst;size_t nread;if(c!=2){errcopy(e,es,"fileCopy(source, destination) expects 2 arguments");return 0;}src=fopen(argstr(a,0),"rb");if(!src){errcopy(e,es,"fileCopy cannot open source");return 0;}dst=fopen(argstr(a,1),"wb");if(!dst){fclose(src);errcopy(e,es,"fileCopy cannot open destination");return 0;}do{nread=fread(chunk,1U,sizeof(chunk),src);if(nread&&fwrite(chunk,1U,nread,dst)!=nread){fclose(src);fclose(dst);errcopy(e,es,"fileCopy write failed");return 0;}}while(nread==sizeof(chunk));if(ferror(src)){fclose(src);fclose(dst);errcopy(e,es,"fileCopy read failed");return 0;}fclose(src);if(fclose(dst)!=0){errcopy(e,es,"fileCopy close failed");return 0;}set_bool(r,1);return 1;}
 if(strcmp(n,"fileMove")==0){if(c!=2){errcopy(e,es,"fileMove(source, destination) expects 2 arguments");return 0;}set_bool(r,rename(argstr(a,0),argstr(a,1))==0);return 1;}
 if(strcmp(n,"directoryCreate")==0){if(c!=1){errcopy(e,es,"directoryCreate(path) expects 1 argument");return 0;}if(PIPI_MKDIR(argstr(a,0))==0||PIPI_ACCESS(argstr(a,0))==0){set_bool(r,1);return 1;}errcopy(e,es,"directoryCreate failed");return 0;}
 errcopy(e,es,"unknown pipi/fs function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;fs_error[0]='\0';return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/fs","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
