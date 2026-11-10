/* PIPI URL/hex text encoding helpers. */
#include <string.h>
#include <ctype.h>
#include "../include/pipi_native.h"
#define ENC_CAP 65536UL
static char output[ENC_CAP];
static const char hexchars[]="0123456789ABCDEF";
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static const char *arg(const PipiValue *a,int i){return a[i].string?a[i].string:"";}
static int unhex(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int has_function(const char *n){return strcmp(n,"urlEncode")==0||strcmp(n,"urlDecode")==0||strcmp(n,"hexEncode")==0||strcmp(n,"hexDecode")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const unsigned char *s;size_t i,p;int hi,lo;unsigned char ch;(void)h;if(c!=1){errcopy(e,es,"encoding function expects one text argument");return 0;}s=(const unsigned char *)arg(a,0);p=0;
 if(strcmp(n,"urlEncode")==0||strcmp(n,"hexEncode")==0){for(i=0;s[i];i++){ch=s[i];if(strcmp(n,"urlEncode")==0&&(isalnum(ch)||ch=='-'||ch=='_'||ch=='.'||ch=='~')){if(p+1U>=ENC_CAP)break;output[p++]=(char)ch;}else if(strcmp(n,"urlEncode")==0){if(p+3U>=ENC_CAP)break;output[p++]='%';output[p++]=hexchars[ch>>4];output[p++]=hexchars[ch&15];}else{if(p+2U>=ENC_CAP)break;output[p++]=hexchars[ch>>4];output[p++]=hexchars[ch&15];}}output[p]='\0';setstr(r,output);return 1;}
 if(strcmp(n,"urlDecode")==0){for(i=0;s[i];i++){if(s[i]=='%'&&s[i+1]&&s[i+2]){hi=unhex(s[i+1]);lo=unhex(s[i+2]);if(hi<0||lo<0){errcopy(e,es,"urlDecode found an invalid percent escape");return 0;}ch=(unsigned char)((hi<<4)|lo);i+=2;}else if(s[i]=='+')ch=' ';else ch=s[i];if(p+1U>=ENC_CAP){errcopy(e,es,"decoded text too large");return 0;}output[p++]=(char)ch;}output[p]='\0';setstr(r,output);return 1;}
 if(strcmp(n,"hexDecode")==0){for(i=0;s[i];i+=2){if(!s[i+1]){errcopy(e,es,"hexDecode expects an even number of digits");return 0;}hi=unhex(s[i]);lo=unhex(s[i+1]);if(hi<0||lo<0){errcopy(e,es,"hexDecode found a non-hexadecimal digit");return 0;}ch=(unsigned char)((hi<<4)|lo);if(ch==0){errcopy(e,es,"hexDecode cannot return embedded NUL bytes as a PIPI string");return 0;}if(p+1U>=ENC_CAP){errcopy(e,es,"decoded text too large");return 0;}output[p++]=(char)ch;}output[p]='\0';setstr(r,output);return 1;}
 errcopy(e,es,"unknown pipi/encoding function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/encoding","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
