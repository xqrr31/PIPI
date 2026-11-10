/* PIPI standard networking module - portable C89 + POSIX sockets / WinSock. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "../include/pipi_native.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET PipiSocket;
#define PIPI_BAD_SOCKET INVALID_SOCKET
#define PIPI_CLOSESOCKET closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int PipiSocket;
#define PIPI_BAD_SOCKET (-1)
#define PIPI_CLOSESOCKET close
#endif

#define NET_SOCKETS 64
#define NET_BUFFER 8192
static PipiSocket sockets[NET_SOCKETS];
static int socket_used[NET_SOCKETS];
static char net_buffer[NET_BUFFER + 1];
static char net_error[256];

static void set_error_text(char *out, size_t cap, const char *s) {
    if (out && cap) { strncpy(out, s ? s : "network error", cap - 1U); out[cap - 1U] = '\0'; }
}
static void result_number(PipiValue *r, double n) { r->type=PIPI_VALUE_NUMBER; r->number=n; r->string=""; r->boolean=0; }
static void result_bool(PipiValue *r, int b) { r->type=PIPI_VALUE_BOOLEAN; r->number=0.0; r->string=""; r->boolean=b?1:0; }
static void result_string(PipiValue *r, const char *s) { r->type=PIPI_VALUE_STRING; r->number=0.0; r->string=s?s:""; r->boolean=0; }
static void record_socket_error(const char *where) {
#ifdef _WIN32
    sprintf(net_error,"%s: WinSock error %d",where,WSAGetLastError());
#else
    sprintf(net_error,"%s: %s",where,strerror(errno));
#endif
}
static double number_at(const PipiValue *a, int i) { return a[i].type==PIPI_VALUE_NUMBER?a[i].number:(double)a[i].boolean; }
static const char *string_at(const PipiValue *a, int i) { return a[i].string?a[i].string:""; }
static int valid_handle(double value) { int h=(int)value; return value==(double)h && h>0 && h<=NET_SOCKETS && socket_used[h-1]; }
static int handle_index(double value) { return (int)value-1; }
static int allocate_socket(PipiSocket s) { int i; for(i=0;i<NET_SOCKETS;i++) if(!socket_used[i]) { sockets[i]=s; socket_used[i]=1; return i+1; } return 0; }
static int resolve_ipv4(const char *host, struct in_addr *address) {
    struct hostent *entry;
    unsigned long raw;
    if (!host || !*host || strcmp(host,"*")==0) { address->s_addr=htonl(INADDR_ANY); return 1; }
    raw=inet_addr(host);
    if(raw!=INADDR_NONE) { address->s_addr=raw; return 1; }
    entry=gethostbyname(host);
    if(!entry || !entry->h_addr_list || !entry->h_addr_list[0]) return 0;
    memcpy(address,entry->h_addr_list[0],sizeof(*address));
    return 1;
}
static int has_function(const char *n) {
    static const char *names[]={"socketCreate","socketConnect","socketBind","socketListen","socketAccept","socketSend","socketRecv","socketClose","socketSetReuse","socketShutdownWrite","socketLastError",NULL};
    int i; for(i=0;names[i];i++) if(strcmp(n,names[i])==0) return 1; return 0;
}
static int call_function(const PipiHostAPI *host,const char *name,int argc,const PipiValue *a,PipiValue *r,char *error,size_t error_size) {
    int idx,port,rc; PipiSocket s; struct sockaddr_in addr;
    (void)host; if(strcmp(name,"socketLastError")!=0)net_error[0]='\0'; result_number(r,0.0);
    if(strcmp(name,"socketCreate")==0) {
        int type;
        if(argc!=1) { set_error_text(error,error_size,"socketCreate('tcp'|'udp') expects 1 argument"); return 0; }
        if(strcmp(string_at(a,0),"tcp")==0) type=SOCK_STREAM;
        else if(strcmp(string_at(a,0),"udp")==0) type=SOCK_DGRAM;
        else { set_error_text(error,error_size,"socketCreate protocol must be 'tcp' or 'udp'"); return 0; }
        s=socket(AF_INET,type,0);
        if(s==PIPI_BAD_SOCKET) { record_socket_error("socket"); result_bool(r,0); return 1; }
        idx=allocate_socket(s);
        if(!idx) { PIPI_CLOSESOCKET(s); set_error_text(error,error_size,"socket table is full"); return 0; }
        result_number(r,(double)idx); return 1;
    }
    if(strcmp(name,"socketLastError")==0) { result_string(r,net_error[0]?net_error:"no socket error recorded"); return 1; }
    if(argc<1 || !valid_handle(number_at(a,0))) { set_error_text(error,error_size,"invalid socket handle"); return 0; }
    idx=handle_index(number_at(a,0)); s=sockets[idx];
    if(strcmp(name,"socketConnect")==0 || strcmp(name,"socketBind")==0) {
        if(argc!=3) { set_error_text(error,error_size,"socketConnect/Bind(socket, host, port) expects 3 arguments"); return 0; }
        port=(int)number_at(a,2); if(port<1||port>65535) { set_error_text(error,error_size,"port must be between 1 and 65535"); return 0; }
        memset(&addr,0,sizeof(addr)); addr.sin_family=AF_INET; addr.sin_port=htons((unsigned short)port);
        if(!resolve_ipv4(string_at(a,1),&addr.sin_addr)) { set_error_text(error,error_size,"cannot resolve IPv4 host"); return 0; }
        if(strcmp(name,"socketConnect")==0) rc=connect(s,(struct sockaddr *)&addr,(int)sizeof(addr));
        else rc=bind(s,(struct sockaddr *)&addr,(int)sizeof(addr));
        if(rc!=0) { record_socket_error(strcmp(name,"socketConnect")==0?"connect":"bind"); result_bool(r,0); return 1; }
        result_bool(r,1); return 1;
    }
    if(strcmp(name,"socketListen")==0) {
        int backlog; if(argc!=2) { set_error_text(error,error_size,"socketListen(socket, backlog) expects 2 arguments"); return 0; }
        backlog=(int)number_at(a,1); if(backlog<1) backlog=1; if(backlog>128) backlog=128;
        rc=listen(s,backlog); if(rc!=0) { record_socket_error("listen"); result_bool(r,0); return 1; } result_bool(r,1); return 1;
    }
    if(strcmp(name,"socketAccept")==0) {
        PipiSocket accepted; accepted=accept(s,NULL,NULL);
        if(accepted==PIPI_BAD_SOCKET) { record_socket_error("accept"); result_number(r,0.0); return 1; }
        idx=allocate_socket(accepted); if(!idx) { PIPI_CLOSESOCKET(accepted); set_error_text(error,error_size,"socket table is full"); return 0; }
        result_number(r,(double)idx); return 1;
    }
    if(strcmp(name,"socketSend")==0) {
        const char *data; int length,sent;
        if(argc!=2) { set_error_text(error,error_size,"socketSend(socket, text) expects 2 arguments"); return 0; }
        data=string_at(a,1); length=(int)strlen(data);
        sent=send(s,data,length,0); if(sent<0) { record_socket_error("send"); result_number(r,-1.0); return 1; }
        result_number(r,(double)sent); return 1;
    }
    if(strcmp(name,"socketRecv")==0) {
        int wanted,received;
        if(argc!=2) { set_error_text(error,error_size,"socketRecv(socket, maxBytes) expects 2 arguments"); return 0; }
        wanted=(int)number_at(a,1); if(wanted<1) wanted=1; if(wanted>NET_BUFFER) wanted=NET_BUFFER;
        received=recv(s,net_buffer,wanted,0); if(received<0) { record_socket_error("recv"); result_string(r,""); return 1; }
        net_buffer[received]='\0'; result_string(r,net_buffer); return 1;
    }
    if(strcmp(name,"socketClose")==0) {
        PIPI_CLOSESOCKET(s); socket_used[idx]=0; result_bool(r,1); return 1;
    }
    if(strcmp(name,"socketSetReuse")==0) {
        int yes; if(argc!=2) { set_error_text(error,error_size,"socketSetReuse(socket, enabled) expects 2 arguments"); return 0; }
        yes=number_at(a,1)!=0.0; rc=setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&yes,(int)sizeof(yes));
        if(rc!=0) { record_socket_error("setsockopt"); result_bool(r,0); return 1; } result_bool(r,1); return 1;
    }
    if(strcmp(name,"socketShutdownWrite")==0) {
        if(argc!=1) { set_error_text(error,error_size,"socketShutdownWrite(socket) expects 1 argument"); return 0; }
#ifdef _WIN32
        rc=shutdown(s,SD_SEND);
#else
        rc=shutdown(s,SHUT_WR);
#endif
        if(rc!=0) { record_socket_error("shutdown"); result_bool(r,0); return 1; } result_bool(r,1); return 1;
    }
    set_error_text(error,error_size,"unknown pipi/net function"); return 0;
}
static int initialize(const PipiHostAPI *host,char *error,size_t cap) {
    int i; (void)host;
    for(i=0;i<NET_SOCKETS;i++) socket_used[i]=0;
#ifdef _WIN32
    { WSADATA data; if(WSAStartup(MAKEWORD(2,2),&data)!=0) { set_error_text(error,cap,"WSAStartup failed"); return 0; } }
#else
    (void)error; (void)cap;
#endif
    return 1;
}
static void shutdown_module(const PipiHostAPI *host) {
    int i; (void)host;
    for(i=0;i<NET_SOCKETS;i++) if(socket_used[i]) { PIPI_CLOSESOCKET(sockets[i]); socket_used[i]=0; }
#ifdef _WIN32
    WSACleanup();
#endif
}
static const PipiNativeModuleV1 api={PIPI_NATIVE_ABI_VERSION,"pipi/net","1.0.0",initialize,has_function,call_function,shutdown_module};
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void) { return &api; }
