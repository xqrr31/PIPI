/* PIPI JSON helpers. C89, flat-object getters with safe nested-value skipping. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/pipi_native.h"

static char json_output[8192];
static char json_error[256];
static void result_string(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0.0;r->string=s?s:"";r->boolean=0;}
static void result_number(PipiValue *r,double n){r->type=PIPI_VALUE_NUMBER;r->number=n;r->string="";r->boolean=0;}
static void result_bool(PipiValue *r,int b){r->type=PIPI_VALUE_BOOLEAN;r->number=0.0;r->string="";r->boolean=b?1:0;}
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void ws(const char **p){while(**p&&isspace((unsigned char)**p))(*p)++;}
static int append_char(char *out,size_t cap,size_t *n,char c){if(*n+1U>=cap)return 0;out[(*n)++]=c;out[*n]='\0';return 1;}
static int parse_string(const char **p,char *out,size_t cap){size_t n=0;int v;unsigned long code;if(**p!='"')return 0;(*p)++;if(cap)out[0]='\0';while(**p&&**p!='"'){
 char c=*(*p)++;
 if(c=='\\'){
  c=*(*p)++;if(!c)return 0;
  if(c=='"'||c=='\\'||c=='/'){}
  else if(c=='b')c='\b';else if(c=='f')c='\f';else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
  else if(c=='u'){
   code=0UL;for(v=0;v<4;v++){char h=*(*p)++;if(!h)return 0;code<<=4;if(h>='0'&&h<='9')code+=(unsigned long)(h-'0');else if(h>='a'&&h<='f')code+=(unsigned long)(h-'a'+10);else if(h>='A'&&h<='F')code+=(unsigned long)(h-'A'+10);else return 0;}
   if(code<0x80UL)c=(char)code;
   else if(code<0x800UL){if(!append_char(out,cap,&n,(char)(0xc0U|(code>>6)))||!append_char(out,cap,&n,(char)(0x80U|(code&0x3fU))))return 0;continue;}
   else {if(!append_char(out,cap,&n,(char)(0xe0U|(code>>12)))||!append_char(out,cap,&n,(char)(0x80U|((code>>6)&0x3fU)))||!append_char(out,cap,&n,(char)(0x80U|(code&0x3fU))))return 0;continue;}
  } else return 0;
 }
 if(!append_char(out,cap,&n,c))return 0;
 }
 if(**p!='"') return 0;
 (*p)++;
 if(cap) out[n]='\0';
 return 1;
}
static int skip_long_string(const char **p);
static int skip_value(const char **p,int depth){if(depth>32)return 0;ws(p);
 if(**p=='"')return skip_long_string(p);
 if(**p=='{'){
  (*p)++;ws(p);if(**p=='}'){(*p)++;return 1;}
  for(;;){char key[2048];if(!parse_string(p,key,sizeof(key)))return 0;ws(p);if(*(*p)++!=':')return 0;if(!skip_value(p,depth+1))return 0;ws(p);if(**p=='}'){(*p)++;return 1;}if(*(*p)++!=',')return 0;ws(p);}
 }
 if(**p=='['){(*p)++;ws(p);if(**p==']'){(*p)++;return 1;}for(;;){if(!skip_value(p,depth+1))return 0;ws(p);if(**p==']'){(*p)++;return 1;}if(*(*p)++!=',')return 0;}}
 if(strncmp(*p,"true",4)==0){*p+=4;return 1;}if(strncmp(*p,"false",5)==0){*p+=5;return 1;}if(strncmp(*p,"null",4)==0){*p+=4;return 1;}
 {const char *s=*p;while(**p&&!strchr(",}] \t\r\n",**p))(*p)++;return *p>s;}
}
/* Long skipped strings use a separate bounded scanner; no allocation required. */
static int skip_long_string(const char **p){if(**p!='"')return 0;(*p)++;while(**p){if(**p=='\\'){(*p)++;if(!**p)return 0;(*p)++;}else if(*(*p)++=='"')return 1;}return 0;}
static int locate(const char *json,const char *key,const char **value,int *type){const char *p=json;char k[2048];
 ws(&p);if(*p++!='{')return 0;ws(&p);if(*p=='}')return 0;
 for(;;){if(!parse_string(&p,k,sizeof(k)))return 0;ws(&p);if(*p++!=':')return 0;ws(&p);
  if(strcmp(k,key)==0){*value=p;if(*p=='"')*type=1;else if(*p=='-'||isdigit((unsigned char)*p))*type=2;else if(strncmp(p,"true",4)==0||strncmp(p,"false",5)==0)*type=3;else if(strncmp(p,"null",4)==0)*type=4;else if(*p=='{')*type=5;else if(*p=='[')*type=6;else *type=0;return 1;}
  if(!skip_value(&p,0)) return 0;
  ws(&p);
  if(*p=='}') return 0;
  if(*p++!=',') return 0;
  ws(&p);
 }
}
static int has_function(const char *n){static const char *names[]={"jsonHasKey","jsonGetString","jsonGetNumber","jsonGetBoolean","jsonType","jsonEscape",NULL};int i;for(i=0;names[i];i++)if(strcmp(n,names[i])==0)return 1;return 0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const char *value;int type;const char *json,*key;(void)h;(void)json_error;
 if(strcmp(n,"jsonEscape")==0){const unsigned char *p;size_t z=0;if(c!=1||a[0].type!=PIPI_VALUE_STRING){errcopy(e,es,"jsonEscape(text) expects one string");return 0;}json_output[0]='\0';p=(const unsigned char *)(a[0].string?a[0].string:"");while(*p){char b[7];const char *s=NULL;switch(*p){case '"':s="\\\"";break;case '\\':s="\\\\";break;case '\b':s="\\b";break;case '\f':s="\\f";break;case '\n':s="\\n";break;case '\r':s="\\r";break;case '\t':s="\\t";break;default:break;}if(s){size_t m=strlen(s);if(z+m>=sizeof(json_output)){errcopy(e,es,"jsonEscape result too long");return 0;}memcpy(json_output+z,s,m);z+=m;json_output[z]='\0';}else if(*p<32U){sprintf(b,"\\u%04x",(unsigned int)*p);if(z+6U>=sizeof(json_output)){errcopy(e,es,"jsonEscape result too long");return 0;}memcpy(json_output+z,b,6U);z+=6U;json_output[z]='\0';}else{if(z+1U>=sizeof(json_output)){errcopy(e,es,"jsonEscape result too long");return 0;}json_output[z++]=(char)*p;json_output[z]='\0';}p++;}result_string(r,json_output);return 1;}
 if(c!=2||a[0].type!=PIPI_VALUE_STRING||a[1].type!=PIPI_VALUE_STRING){errcopy(e,es,"JSON getters expect (jsonText, key) strings");return 0;}json=a[0].string?a[0].string:"";key=a[1].string?a[1].string:"";
 if(!locate(json,key,&value,&type)){if(strcmp(n,"jsonHasKey")==0){result_bool(r,0);return 1;}if(strcmp(n,"jsonType")==0){result_string(r,"missing");return 1;}errcopy(e,es,"JSON key was not found or JSON object is invalid");return 0;}
 if(strcmp(n,"jsonHasKey")==0){result_bool(r,1);return 1;}
 if(strcmp(n,"jsonType")==0){static const char *types[]={"invalid","string","number","boolean","null","object","array"};result_string(r,types[type>=0&&type<=6?type:0]);return 1;}
 if(strcmp(n,"jsonGetString")==0){const char *p=value;if(type!=1||!parse_string(&p,json_output,sizeof(json_output))){errcopy(e,es,"JSON value is not a valid string or is too long");return 0;}result_string(r,json_output);return 1;}
 if(strcmp(n,"jsonGetNumber")==0){char num[128];size_t z=0;char *end;double d;const char *p=value;if(type!=2){errcopy(e,es,"JSON value is not a number");return 0;}while(*p&&!strchr(",}] \t\r\n",*p)&&z+1U<sizeof(num))num[z++]=*p++;num[z]='\0';d=strtod(num,&end);if(end==num||*end){errcopy(e,es,"invalid JSON number");return 0;}result_number(r,d);return 1;}
 if(strcmp(n,"jsonGetBoolean")==0){if(type!=3){errcopy(e,es,"JSON value is not a boolean");return 0;}result_bool(r,strncmp(value,"true",4)==0);return 1;}
 errcopy(e,es,"unknown pipi/json function");return 0;
}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/json","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
