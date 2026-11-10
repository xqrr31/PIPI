/* PIPI string helpers. Results use module-owned storage copied by the VM. */
#include <string.h>
#include <ctype.h>
#include "../include/pipi_native.h"
#define STR_CAP 65536UL
static char output[STR_CAP];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setnum(PipiValue *r,double n){r->type=PIPI_VALUE_NUMBER;r->number=n;r->string="";r->boolean=0;}
static void setbool(PipiValue *r,int b){r->type=PIPI_VALUE_BOOLEAN;r->number=0;r->string="";r->boolean=b?1:0;}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static const char *arg(const PipiValue *a,int i){return a[i].string?a[i].string:"";}
static int has_function(const char *n){static const char *names[]={"strContains","strIndexOf","strUpper","strLower","strTrim","strReplace","strStartsWith","strEndsWith",NULL};int i;for(i=0;names[i];i++)if(strcmp(n,names[i])==0)return 1;return 0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const char *s,*needle,*replacement,*found;size_t i,j,k,len,oldlen,newlen,pos;(void)h;setnum(r,0);s=c>0?arg(a,0):"";
 if(strcmp(n,"strContains")==0||strcmp(n,"strIndexOf")==0){if(c!=2){errcopy(e,es,"strContains/strIndexOf(text, needle) expects 2 arguments");return 0;}needle=arg(a,1);found=strstr(s,needle);if(strcmp(n,"strContains")==0)setbool(r,found!=NULL);else setnum(r,found?(double)(found-s):-1.0);return 1;}
 if(strcmp(n,"strUpper")==0||strcmp(n,"strLower")==0||strcmp(n,"strTrim")==0){if(c!=1){errcopy(e,es,"string transform expects 1 argument");return 0;}len=strlen(s);if(len>=STR_CAP){errcopy(e,es,"string exceeds 65535 bytes");return 0;}if(strcmp(n,"strTrim")==0){i=0;while(i<len&&isspace((unsigned char)s[i]))i++;j=len;while(j>i&&isspace((unsigned char)s[j-1]))j--;memcpy(output,s+i,j-i);output[j-i]='\0';}else{for(i=0;i<len;i++)output[i]=(char)(strcmp(n,"strUpper")==0?toupper((unsigned char)s[i]):tolower((unsigned char)s[i]));output[len]='\0';}setstr(r,output);return 1;}
 if(strcmp(n,"strReplace")==0){if(c!=3){errcopy(e,es,"strReplace(text, search, replacement) expects 3 arguments");return 0;}needle=arg(a,1);replacement=arg(a,2);oldlen=strlen(needle);newlen=strlen(replacement);len=strlen(s);if(oldlen==0U){errcopy(e,es,"strReplace search text cannot be empty");return 0;}i=0;pos=0;while(i<len){if(strncmp(s+i,needle,oldlen)==0){if(pos+newlen>=STR_CAP){errcopy(e,es,"strReplace output too large");return 0;}memcpy(output+pos,replacement,newlen);pos+=newlen;i+=oldlen;}else{if(pos+1U>=STR_CAP){errcopy(e,es,"strReplace output too large");return 0;}output[pos++]=s[i++];}}output[pos]='\0';setstr(r,output);return 1;}
 if(strcmp(n,"strStartsWith")==0||strcmp(n,"strEndsWith")==0){if(c!=2){errcopy(e,es,"strStartsWith/strEndsWith(text, part) expects 2 arguments");return 0;}needle=arg(a,1);len=strlen(s);oldlen=strlen(needle);if(oldlen>len){setbool(r,0);return 1;}if(strcmp(n,"strStartsWith")==0)setbool(r,strncmp(s,needle,oldlen)==0);else setbool(r,strcmp(s+len-oldlen,needle)==0);return 1;}
 (void)k;errcopy(e,es,"unknown pipi/string function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/string","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
