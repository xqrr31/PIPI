/* Compact SHA-256 and CRC32 helpers, implemented in portable C89. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/pipi_native.h"
#define U32 0xffffffffUL
#define ROR32(x,n) ((((x)>>(n))|((x)<<(32-(n))))&U32)
static char digest_hex[65];
static unsigned long crc32_text(const unsigned char *p,size_t n){unsigned long c=U32;size_t i;int j;for(i=0;i<n;i++){c^=(unsigned long)p[i];for(j=0;j<8;j++)c=(c&1UL)?((c>>1)^0xedb88320UL):(c>>1);}return c^U32;}
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setnum(PipiValue *r,double v){r->type=PIPI_VALUE_NUMBER;r->number=v;r->string="";r->boolean=0;}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static unsigned long choose32(unsigned long x,unsigned long y,unsigned long z){return (x&y)^((~x)&z);}
static unsigned long majority32(unsigned long x,unsigned long y,unsigned long z){return (x&y)^(x&z)^(y&z);}
static void sha_transform(unsigned long state[8],const unsigned char block[64]){
 static const unsigned long k[64]={
 0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
 0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
 0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
 0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
 0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
 0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
 0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
 0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL};
 unsigned long w[64],a,b,c,d,e,f,g,h,s0,s1,ch,maj,t1,t2;int i;
 for(i=0;i<16;i++)w[i]=(((unsigned long)block[i*4]<<24)|((unsigned long)block[i*4+1]<<16)|((unsigned long)block[i*4+2]<<8)|(unsigned long)block[i*4+3])&U32;
 for(i=16;i<64;i++){s0=ROR32(w[i-15],7)^ROR32(w[i-15],18)^(w[i-15]>>3);s1=ROR32(w[i-2],17)^ROR32(w[i-2],19)^(w[i-2]>>10);w[i]=(w[i-16]+s0+w[i-7]+s1)&U32;}
 a=state[0];b=state[1];c=state[2];d=state[3];e=state[4];f=state[5];g=state[6];h=state[7];
 for(i=0;i<64;i++){s1=ROR32(e,6)^ROR32(e,11)^ROR32(e,25);ch=choose32(e,f,g);t1=(h+s1+ch+k[i]+w[i])&U32;s0=ROR32(a,2)^ROR32(a,13)^ROR32(a,22);maj=majority32(a,b,c);t2=(s0+maj)&U32;h=g;g=f;f=e;e=(d+t1)&U32;d=c;c=b;b=a;a=(t1+t2)&U32;}
 state[0]=(state[0]+a)&U32;state[1]=(state[1]+b)&U32;state[2]=(state[2]+c)&U32;state[3]=(state[3]+d)&U32;state[4]=(state[4]+e)&U32;state[5]=(state[5]+f)&U32;state[6]=(state[6]+g)&U32;state[7]=(state[7]+h)&U32;
}
static const char *sha256(const char *text){unsigned long state[8];size_t len=strlen(text),padded,i;unsigned char *data;static const char hex[]="0123456789abcdef";unsigned long bit_low,bit_high;int j;
 state[0]=0x6a09e667UL;state[1]=0xbb67ae85UL;state[2]=0x3c6ef372UL;state[3]=0xa54ff53aUL;state[4]=0x510e527fUL;state[5]=0x9b05688cUL;state[6]=0x1f83d9abUL;state[7]=0x5be0cd19UL;
 padded=((len+9U+63U)/64U)*64U;data=(unsigned char *)calloc(padded,1U);if(!data)return NULL;memcpy(data,text,len);data[len]=0x80U;bit_low=(unsigned long)len*8UL;bit_high=(unsigned long)(((double)len*8.0)/4294967296.0);for(i=0;i<4;i++){data[padded-8U+i]=(unsigned char)((bit_high>>(24U-(unsigned long)i*8U))&255UL);data[padded-4U+i]=(unsigned char)((bit_low>>(24U-(unsigned long)i*8U))&255UL);}for(i=0;i<padded;i+=64U)sha_transform(state,data+i);free(data);
 for(i=0;i<8;i++) { for(j=0;j<4;j++){unsigned int v=(unsigned int)((state[i]>>(24-j*8))&255UL);digest_hex[i*8U+j*2U]=hex[v>>4];digest_hex[i*8U+j*2U+1U]=hex[v&15U];} }
 digest_hex[64]='\0';return digest_hex;}
static int has_function(const char *n){return strcmp(n,"sha256")==0||strcmp(n,"crc32")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){const char *s;(void)h;if(c!=1){errcopy(e,es,"crypto function expects one string argument");return 0;}s=a[0].string?a[0].string:"";if(strcmp(n,"sha256")==0){s=sha256(s);if(!s){errcopy(e,es,"sha256 out of memory");return 0;}setstr(r,s);return 1;}setnum(r,(double)crc32_text((const unsigned char *)s,strlen(s)));return 1;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/crypto","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
