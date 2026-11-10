/* PIPI pseudo-random utilities. Not suitable for cryptographic secrets. C89. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../include/pipi_native.h"
static unsigned long state=0UL;
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void result_number(PipiValue *r,double n){r->type=PIPI_VALUE_NUMBER;r->number=n;r->string="";r->boolean=0;}
static void result_bool(PipiValue *r,int b){r->type=PIPI_VALUE_BOOLEAN;r->number=0.0;r->string="";r->boolean=b?1:0;}
static unsigned long next_value(void){if(state==0UL)state=(unsigned long)time(NULL)^0x9e3779b9UL;state^=(state<<13);state&=0xffffffffUL;state^=(state>>17);state&=0xffffffffUL;state^=(state<<5);state&=0xffffffffUL;if(state==0UL)state=0x6d2b79f5UL;return state;}
static int has_function(const char *n){return strcmp(n,"randomSeed")==0||strcmp(n,"randomInt")==0||strcmp(n,"randomFloat")==0||strcmp(n,"randomBool")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){double lo,hi;unsigned long span;(void)h;
 if(strcmp(n,"randomSeed")==0){if(c!=1||a[0].type!=PIPI_VALUE_NUMBER){errcopy(e,es,"randomSeed(number) expects one number");return 0;}state=((unsigned long)a[0].number)&0xffffffffUL;if(state==0UL)state=1UL;result_bool(r,1);return 1;}
 if(strcmp(n,"randomInt")==0){if(c!=2||a[0].type!=PIPI_VALUE_NUMBER||a[1].type!=PIPI_VALUE_NUMBER){errcopy(e,es,"randomInt(min, max) expects two numbers");return 0;}lo=a[0].number;hi=a[1].number;if(lo>hi){double t=lo;lo=hi;hi=t;}if(lo<-1000000000.0||hi>1000000000.0){errcopy(e,es,"randomInt bounds must fit signed 32-bit integers");return 0;}span=(unsigned long)((long)hi-(long)lo)+1UL;result_number(r,(double)((long)lo+(long)(next_value()%span)));return 1;}
 if(strcmp(n,"randomFloat")==0){if(c!=0){errcopy(e,es,"randomFloat() expects no arguments");return 0;}result_number(r,(double)(next_value()&0xffffffUL)/16777216.0);return 1;}
 if(strcmp(n,"randomBool")==0){if(c!=0){errcopy(e,es,"randomBool() expects no arguments");return 0;}result_bool(r,(int)(next_value()&1UL));return 1;}
 errcopy(e,es,"unknown pipi/random function");return 0;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;state=0UL;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/random","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
