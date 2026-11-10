/* Plain HTTP/1.0 GET helper. HTTPS/TLS is intentionally not implied. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/pipi_native.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET HttpSocket;
#define HTTP_BAD_SOCKET INVALID_SOCKET
#define HTTP_CLOSE closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int HttpSocket;
#define HTTP_BAD_SOCKET (-1)
#define HTTP_CLOSE close
#endif
#define HTTP_CAP 65536UL
static char response[HTTP_CAP+1UL];
static char request[4096];
static void errcopy(char *e,size_t n,const char *s){if(e&&n){strncpy(e,s,n-1U);e[n-1U]='\0';}}
static void setstr(PipiValue *r,const char *s){r->type=PIPI_VALUE_STRING;r->number=0;r->string=s?s:"";r->boolean=0;}
static int has_function(const char *n){return strcmp(n,"httpGet")==0;}
static int call_function(const PipiHostAPI *h,const char *n,int c,const PipiValue *a,PipiValue *r,char *e,size_t es){HttpSocket s;struct sockaddr_in addr;struct hostent *entry;const char *host,*path;int port,rc,sent,total,got;size_t i;(void)h;(void)n;
 if(c!=3){errcopy(e,es,"httpGet(host, path, port) expects 3 arguments; only plain HTTP is supported");return 0;}
 host=a[0].string?a[0].string:"";path=a[1].string?a[1].string:"/";port=(int)a[2].number;
 if(!*host||port<1||port>65535){errcopy(e,es,"httpGet host or port is invalid");return 0;}
 if(path[0]!='/' ){errcopy(e,es,"httpGet path must begin with '/'");return 0;}
#ifdef _WIN32
 {WSADATA wd;if(WSAStartup(MAKEWORD(2,2),&wd)!=0){errcopy(e,es,"WSAStartup failed");return 0;}}
#endif
 s=socket(AF_INET,SOCK_STREAM,0);if(s==HTTP_BAD_SOCKET){errcopy(e,es,"HTTP socket() failed");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;}
 entry=gethostbyname(host);if(!entry||!entry->h_addr_list||!entry->h_addr_list[0]){HTTP_CLOSE(s);errcopy(e,es,"HTTP host DNS lookup failed");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;}
 memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)port);memcpy(&addr.sin_addr,entry->h_addr_list[0],sizeof(addr.sin_addr));
 rc=connect(s,(struct sockaddr *)&addr,(int)sizeof(addr));if(rc!=0){HTTP_CLOSE(s);errcopy(e,es,"HTTP connect failed");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;}
 if(strlen(path)+strlen(host)+128U>=sizeof(request)){HTTP_CLOSE(s);errcopy(e,es,"HTTP request URL too long");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;}
 request[0]='\0';strcat(request,"GET ");strcat(request,path);strcat(request," HTTP/1.0\r\nHost: ");strcat(request,host);strcat(request,"\r\nConnection: close\r\nUser-Agent: PIPI/1.0.0\r\n\r\n");
 total=(int)strlen(request);sent=0;while(sent<total){rc=send(s,request+sent,total-sent,0);if(rc<=0){HTTP_CLOSE(s);errcopy(e,es,"HTTP request send failed");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;}sent+=rc;}
 i=0;while(i<HTTP_CAP){got=recv(s,response+i,(int)(HTTP_CAP-i),0);if(got<=0)break;i+=(size_t)got;}
 HTTP_CLOSE(s);
#ifdef _WIN32
 WSACleanup();
#endif
 response[i]='\0';setstr(r,response);return 1;}
static int initialize(const PipiHostAPI *h,char *e,size_t n){(void)h;(void)e;(void)n;return 1;}
static void shutdown_module(const PipiHostAPI *h){(void)h;}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/http","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void){return &api;}
