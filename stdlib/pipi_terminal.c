/* PIPI terminal I/O helpers. C89, ANSI colors where supported. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#define PIPI_ISATTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define PIPI_ISATTY(fd) isatty(fd)
#endif
#include "../include/pipi_native.h"
static char line_buffer[4096];
static char color_buffer[32];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void result_string(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0.0;r->string=s?s:"";r->boolean=0;}
static void result_bool(PipiValue *r,int b){r->type=PIPI_VALUE_BOOLEAN;r->number=0.0;r->string="";r->boolean=b?1:0;}
static int has_function(const char *n){static const char *names[]={"terminalWrite","terminalWriteLine","terminalReadLine","terminalClear","terminalColor","terminalResetColor","terminalIsTTY",NULL};int i;for(i=0;names[i];i++)if(strcmp(n,names[i])==0)return 1;return 0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){(void)h;
 if(strcmp(n,"terminalWrite")==0||strcmp(n,"terminalWriteLine")==0){if(c!=1||a[0].type!=PIPI_VALUE_STRING){errcopy(e,es,"terminalWrite(text) expects one string");return 0;}fputs(a[0].string?a[0].string:"",stdout);if(strcmp(n,"terminalWriteLine")==0)fputc('\n',stdout);fflush(stdout);result_bool(r,1);return 1;}
 if(strcmp(n,"terminalReadLine")==0){if(c>0){errcopy(e,es,"terminalReadLine() expects no arguments");return 0;}if(!fgets(line_buffer,sizeof(line_buffer),stdin)){line_buffer[0]='\0';}else{size_t z=strlen(line_buffer);if(z&&line_buffer[z-1]=='\n')line_buffer[z-1]='\0';}result_string(r,line_buffer);return 1;}
 if(strcmp(n,"terminalClear")==0){if(c!=0){errcopy(e,es,"terminalClear() expects no arguments");return 0;}fputs("\033[2J\033[H",stdout);fflush(stdout);result_bool(r,1);return 1;}
 if(strcmp(n,"terminalColor")==0){int code;if(c!=1||a[0].type!=PIPI_VALUE_NUMBER){errcopy(e,es,"terminalColor(code) expects one number");return 0;}code=(int)a[0].number;if(code<0||code>255){errcopy(e,es,"ANSI color code must be from 0 to 255");return 0;}sprintf(color_buffer,"\033[38;5;%dm",code);fputs(color_buffer,stdout);fflush(stdout);result_bool(r,1);return 1;}
 if(strcmp(n,"terminalResetColor")==0){if(c!=0){errcopy(e,es,"terminalResetColor() expects no arguments");return 0;}fputs("\033[0m",stdout);fflush(stdout);result_bool(r,1);return 1;}
 if(strcmp(n,"terminalIsTTY")==0){if(c!=0){errcopy(e,es,"terminalIsTTY() expects no arguments");return 0;}result_bool(r,PIPI_ISATTY(1)!=0);return 1;}
 errcopy(e,es,"unknown pipi/terminal function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;line_buffer[0]='\0';return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/terminal","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
