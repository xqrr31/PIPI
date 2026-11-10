/* PIPI C89 virtual machine v0.8.0. Executes PIPIBC06 bytecode and native plugins. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include "../include/pipi_native.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define MAX_CONSTS 12000
#define MAX_FUNCS 256
#define MAX_PARAMS 64
#define MAX_STACK 8192
#define MAX_VARS 4096
#define MAX_CALLBACKS 512
#define MAX_HEAP_STRINGS 32768
#define MAX_BYTECODE (32UL * 1024UL * 1024UL)

enum { OP_HALT=0, OP_PUSH_NUM=1, OP_PUSH_STR=2, OP_PUSH_BOOL=3, OP_PUSH_NULL=4,
 OP_LOAD=5, OP_STORE=6, OP_DECLARE=7, OP_POP=8, OP_ADD=9, OP_SUB=10, OP_MUL=11,
 OP_DIV=12, OP_MOD=13, OP_NEG=14, OP_NOT=15, OP_EQ=16, OP_NE=17, OP_LT=18,
 OP_LE=19, OP_GT=20, OP_GE=21, OP_AND=22, OP_OR=23, OP_JUMP=24,
 OP_JFALSE=25, OP_CALL=26, OP_PUSH_FUNC=27, OP_RETURN=28 };
enum { V_NULL=0, V_NUM=1, V_STR=2, V_BOOL=3, V_FUNC=4 };
typedef PipiValue Value;
typedef struct { char *text; } Constant;
typedef struct { int name_const; int param_count; int params[MAX_PARAMS]; unsigned char *code; unsigned long code_len; } Function;
typedef struct { char name[128]; Value value; } Variable;
typedef struct { long window; int button; char function[128]; } Callback;
#ifdef _WIN32
typedef HMODULE LibHandle;
#else
typedef void *LibHandle;
#endif
#define MAX_NATIVE_MODULES 128
typedef struct { char name[256]; LibHandle handle; const PipiNativeModuleV1 *api; } NativeModule;
typedef struct { int depth; } NativeCallContext;
static NativeModule native_modules[MAX_NATIVE_MODULES];
static int native_module_count=0;
static Value execute_function(int fi,Value *args,int argc,int depth);
static void host_log(void *userdata,int level,const char *message);
static void host_set_error(void *userdata,const char *message);
static int host_call_function(void *userdata,const char *name,int argc,const PipiValue *argv,PipiValue *result);
static const char *host_get_env(void *userdata,const char *name);
static const PipiHostAPI host_api={PIPI_NATIVE_ABI_VERSION,NULL,host_log,host_set_error,host_call_function,host_get_env};

typedef long (*FnWindowCreate)(const char *, int, int);
typedef int (*FnLabelAdd)(long, const char *, int, int);
typedef int (*FnButtonAdd)(long, const char *, int, int);
typedef int (*FnWindowShow)(long);
typedef int (*FnNextEvent)(long);
typedef int (*FnMessage)(long, const char *, const char *);
typedef void (*FnWindowDestroy)(long);
typedef void (*FnShutdown)(void);

static Constant constants[MAX_CONSTS];
static Function functions[MAX_FUNCS];
static int constant_count=0, function_count=0;
static Value stack_values[MAX_STACK];
static int stack_top=0;
static Variable variables[MAX_VARS];
static int variable_count=0, global_variable_count=0;
static int frame_bases[256];
static int callback_count=0;
static Callback callbacks[MAX_CALLBACKS];
static char *heap_strings[MAX_HEAP_STRINGS];
static int heap_string_count=0;
static char vm_error[1024];
static LibHandle gui_library=0;
static int gui_attempted=0;
static FnWindowCreate gui_window_create=0;
static FnLabelAdd gui_label_add=0;
static FnButtonAdd gui_button_add=0;
static FnWindowShow gui_window_show=0;
static FnNextEvent gui_next_event=0;
static FnMessage gui_message=0;
static FnWindowDestroy gui_window_destroy=0;
static FnShutdown gui_shutdown=0;

static int eq(const char *a,const char *b) { return a&&b&&strcmp(a,b)==0; }
static void set_error(const char *fmt, ...) { va_list ap; if(vm_error[0])return; va_start(ap,fmt); vsprintf(vm_error,fmt,ap); va_end(ap); vm_error[sizeof(vm_error)-1]='\0'; }
static Value null_value(void) { Value v;v.type=V_NULL;v.number=0;v.string="";v.boolean=0;return v; }
static Value num_value(double n) { Value v;v.type=V_NUM;v.number=n;v.string="";v.boolean=0;return v; }
static Value bool_value(int b) { Value v;v.type=V_BOOL;v.number=0;v.string="";v.boolean=b?1:0;return v; }
static Value str_value(const char *s) { Value v;v.type=V_STR;v.number=0;v.string=s?s:"";v.boolean=0;return v; }
static Value func_value(const char *s) { Value v;v.type=V_FUNC;v.number=0;v.string=s?s:"";v.boolean=0;return v; }
static const char *const_text(unsigned long i) { if(i>= (unsigned long)constant_count){set_error("bytecode references invalid constant index");return "";}return constants[i].text; }
static int push(Value v) { if(stack_top>=MAX_STACK){set_error("PIPI stack overflow");return 0;}stack_values[stack_top++]=v;return 1; }
static Value pop(void) { if(stack_top<=0){set_error("PIPI stack underflow");return null_value();}return stack_values[--stack_top]; }
static char *own_string(const char *s) { size_t n;char *p;if(!s)s="";n=strlen(s);p=(char *)malloc(n+1U);if(!p){set_error("PIPI runtime out of memory");return NULL;}memcpy(p,s,n+1U);if(heap_string_count>=MAX_HEAP_STRINGS){free(p);set_error("too many allocated strings");return NULL;}heap_strings[heap_string_count++]=p;return p; }
static const char *value_string(Value v,char *buffer,size_t cap) { if(v.type==V_STR||v.type==V_FUNC)return v.string?v.string:"";if(v.type==V_NULL)return "null";if(v.type==V_BOOL)return v.boolean?"true":"false";if(v.type==V_NUM){sprintf(buffer,"%.15g",v.number);return buffer;}if(cap)buffer[0]='\0';return ""; }
static double value_number(Value v) { if(v.type==V_NUM)return v.number;if(v.type==V_BOOL)return v.boolean?1.0:0.0;if(v.type==V_NULL)return 0.0;if(v.type==V_STR)return atof(v.string?v.string:"");return 0.0; }
static int truthy(Value v) { if(v.type==V_NULL)return 0;if(v.type==V_BOOL)return v.boolean;if(v.type==V_NUM)return v.number!=0.0;if(v.type==V_STR)return v.string&&v.string[0]!='\0';if(v.type==V_FUNC)return 1;return 0; }
static int value_equal(Value a,Value b) { if(a.type==V_STR&&b.type==V_STR)return eq(a.string,b.string);if(a.type==V_FUNC&&b.type==V_FUNC)return eq(a.string,b.string);if(a.type==V_NULL&&b.type==V_NULL)return 1;if(a.type==V_BOOL&&b.type==V_BOOL)return a.boolean==b.boolean;return value_number(a)==value_number(b); }
static int read_all(const char *path,unsigned char **out,unsigned long *outlen) { FILE *f;long size;unsigned char *p;f=fopen(path,"rb");if(!f){fprintf(stderr,"PIPI VM: cannot open '%s'\n",path);return 0;}if(fseek(f,0L,SEEK_END)!=0){fclose(f);return 0;}size=ftell(f);if(size<0||(unsigned long)size>MAX_BYTECODE){fclose(f);fprintf(stderr,"PIPI VM: invalid bytecode size\n");return 0;}if(fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}p=(unsigned char *)malloc((size_t)size+1U);if(!p){fclose(f);return 0;}if(fread(p,1U,(size_t)size,f)!=(size_t)size){free(p);fclose(f);return 0;}fclose(f);p[size]=0;*out=p;*outlen=(unsigned long)size;return 1; }
static unsigned long get_u4(const unsigned char *data,unsigned long len,unsigned long *offset) { unsigned long v;if(*offset+4UL>len){set_error("truncated PIPIBC06 bytecode");return 0;}v=((unsigned long)data[*offset]<<24)|((unsigned long)data[*offset+1]<<16)|((unsigned long)data[*offset+2]<<8)|(unsigned long)data[*offset+3];*offset+=4UL;return v; }
static int parse_bytecode(const char *path) { unsigned char *data;unsigned long len,off=0,n,i,j,k;char magic[8];if(!read_all(path,&data,&len))return 0;if(len<8UL){free(data);fprintf(stderr,"PIPI VM: not a PIPI bytecode file\n");return 0;}memcpy(magic,data,8U);off=8UL;if(memcmp(magic,"PIPIBC05",8U)!=0&&memcmp(magic,"PIPIBC06",8U)!=0){free(data);fprintf(stderr,"PIPI VM: bytecode version mismatch; expected PIPIBC05 or PIPIBC06\n");return 0;}n=get_u4(data,len,&off);if(n>MAX_CONSTS){free(data);fprintf(stderr,"PIPI VM: too many constants\n");return 0;}constant_count=(int)n;for(i=0;i<n;i++){unsigned long sz=get_u4(data,len,&off);if(vm_error[0]||off+sz>len){free(data);fprintf(stderr,"PIPI VM: corrupt constant pool\n");return 0;}constants[i].text=(char *)malloc((size_t)sz+1U);if(!constants[i].text){free(data);fprintf(stderr,"PIPI VM: out of memory\n");return 0;}memcpy(constants[i].text,data+off,(size_t)sz);constants[i].text[sz]='\0';off+=sz;}n=get_u4(data,len,&off);if(n==0||n>MAX_FUNCS){free(data);fprintf(stderr,"PIPI VM: invalid function count\n");return 0;}function_count=(int)n;for(i=0;i<n;i++){functions[i].name_const=(int)get_u4(data,len,&off);functions[i].param_count=(int)get_u4(data,len,&off);if(functions[i].param_count<0||functions[i].param_count>MAX_PARAMS){free(data);fprintf(stderr,"PIPI VM: invalid parameter count\n");return 0;}for(j=0;j<(unsigned long)functions[i].param_count;j++)functions[i].params[j]=(int)get_u4(data,len,&off);k=get_u4(data,len,&off);if(vm_error[0]||off+k>len){free(data);fprintf(stderr,"PIPI VM: corrupt function body\n");return 0;}functions[i].code=(unsigned char *)malloc((size_t)k+1U);if(!functions[i].code){free(data);return 0;}memcpy(functions[i].code,data+off,(size_t)k);functions[i].code[k]=OP_HALT;functions[i].code_len=k;off+=k;if(functions[i].name_const<0||functions[i].name_const>=constant_count){free(data);fprintf(stderr,"PIPI VM: invalid function name constant\n");return 0;}for(j=0;j<(unsigned long)functions[i].param_count;j++)if(functions[i].params[j]<0||functions[i].params[j]>=constant_count){free(data);fprintf(stderr,"PIPI VM: invalid parameter constant\n");return 0;}}free(data);if(off!=len){fprintf(stderr,"PIPI VM: unexpected trailing bytes in bytecode\n");return 0;}return 1; }
static int function_index(const char *name) { int i;for(i=0;i<function_count;i++)if(eq(const_text((unsigned long)functions[i].name_const),name))return i;return -1; }
static int variable_find(const char *name,int base) { int i;for(i=variable_count-1;i>=base;i--)if(eq(variables[i].name,name))return i;for(i=global_variable_count-1;i>=0;i--)if(eq(variables[i].name,name))return i;return -1; }
static int variable_declare(const char *name,Value v,int depth) { int i;if(variable_count>=MAX_VARS){set_error("too many PIPI variables");return 0;}for(i=depth==0?0:frame_bases[depth];i<variable_count;i++)if(eq(variables[i].name,name)){set_error("variable redeclared in same frame");return 0;}if(strlen(name)>=sizeof(variables[0].name)){set_error("variable name too long");return 0;}strcpy(variables[variable_count].name,name);variables[variable_count].value=v;variable_count++;if(depth==0)global_variable_count=variable_count;return 1; }
static int symbol_load(const char *name,int depth,Value *v) { int idx=variable_find(name,frame_bases[depth]);if(idx<0){set_error("undefined variable '%s'",name);return 0;}*v=variables[idx].value;return 1; }
static int symbol_store(const char *name,int depth,Value v) { int idx=variable_find(name,frame_bases[depth]);if(idx<0){set_error("assignment to undefined variable '%s'",name);return 0;}variables[idx].value=v;return 1; }
static int add_callback(long win,int button,const char *func) { if(callback_count>=MAX_CALLBACKS){set_error("too many GUI callbacks");return 0;}callbacks[callback_count].window=win;callbacks[callback_count].button=button;if(strlen(func)>=sizeof(callbacks[0].function)){set_error("callback name too long");return 0;}strcpy(callbacks[callback_count].function,func);callback_count++;return 1; }
static const char *native_platform(void);
static const char *gui_default_path(void) {
 static char path[160];
#ifdef _WIN32
 sprintf(path,"native/%s/pipi_gui.dll",native_platform());
#else
 sprintf(path,"native/%s/libpipi_gui.so",native_platform());
#endif
 return path;
}
static int bind_gui_symbols(void) {
#ifdef _WIN32
#define PIPI_SYM(type,var,name) do { FARPROC p=GetProcAddress(gui_library,name); if(!p){set_error("native GUI DLL missing symbol " name);return 0;} memcpy(&(var),&p,sizeof(var)); } while(0)
#else
#define PIPI_SYM(type,var,name) do { void *p=dlsym(gui_library,name); if(!p){set_error("native GUI library missing symbol " name);return 0;} memcpy(&(var),&p,sizeof(var)); } while(0)
#endif
 PIPI_SYM(FnWindowCreate,gui_window_create,"pipi_gui_window_create");
 PIPI_SYM(FnLabelAdd,gui_label_add,"pipi_gui_label_add");
 PIPI_SYM(FnButtonAdd,gui_button_add,"pipi_gui_button_add");
 PIPI_SYM(FnWindowShow,gui_window_show,"pipi_gui_window_show");
 PIPI_SYM(FnNextEvent,gui_next_event,"pipi_gui_next_event");
 PIPI_SYM(FnMessage,gui_message,"pipi_gui_message");
 PIPI_SYM(FnWindowDestroy,gui_window_destroy,"pipi_gui_window_destroy");
 PIPI_SYM(FnShutdown,gui_shutdown,"pipi_gui_shutdown");
#undef PIPI_SYM
 return 1;
}
static int load_gui(void) { const char *path;const char *env;if(gui_attempted)return gui_library!=0;gui_attempted=1;env=getenv("PIPI_GUI_LIBRARY");path=(env&&*env)?env:gui_default_path();
#ifdef _WIN32
 gui_library=LoadLibraryA(path);
 if(!gui_library&&!env){char fallback[160];sprintf(fallback,"..\\native\\%s\\pipi_gui.dll",native_platform());gui_library=LoadLibraryA(fallback);}
 if(!gui_library&&!env)gui_library=LoadLibraryA("pipi_gui.dll");
#else
 gui_library=dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!gui_library&&!env){char fallback[160];sprintf(fallback,"../native/%s/libpipi_gui.so",native_platform());gui_library=dlopen(fallback,RTLD_NOW|RTLD_LOCAL);}
 if(!gui_library&&!env)gui_library=dlopen("./libpipi_gui.so",RTLD_NOW|RTLD_LOCAL);
 if(!gui_library&&!env)gui_library=dlopen("../libpipi_gui.so",RTLD_NOW|RTLD_LOCAL);
#endif
 if(!gui_library){char msg[512];sprintf(msg,"cannot load native GUI library '%s'; build native/ for this OS or set PIPI_GUI_LIBRARY",path);set_error(msg);return 0;}return bind_gui_symbols(); }
static int starts_with(const char *s,const char *prefix) { return s&&prefix&&strncmp(s,prefix,strlen(prefix))==0; }
static int safe_native_module(const char *name) { const unsigned char *p=(const unsigned char *)name;if(!name||!*name||strstr(name,".."))return 0;for(;*p;p++)if(!(isalnum(*p)||*p=='/'||*p=='_'||*p=='-'||*p=='.'))return 0;return 1; }
static const char *native_leaf(const char *module) { const char *p=strrchr(module,'/');return p?p+1:module; }
static int module_file_exists(const char *path) { FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1; }
static int build_module_path(char *out,size_t cap,const char *root,const char *prefix,const char *module,const char *platform,const char *filename) {
 char modpath[320];size_t i,n=0;const char *p=module;
 while(*p&&n+1U<sizeof(modpath)){char c=*p++;if(c=='/'){
#ifdef _WIN32
 c='\\';
#endif
 }modpath[n++]=c;}modpath[n]='\0';
 if(strlen(root)+strlen(prefix)+strlen(modpath)+strlen(platform)+strlen(filename)+16U>=cap)return 0;
#ifdef _WIN32
 sprintf(out,"%s%s%s%s%s%s%s",root,*root?"\\":"",prefix,*prefix?"\\":"",modpath,"\\native\\",platform);
 if(strlen(out)+strlen(filename)+2U>=cap)return 0;
 strcat(out,"\\");strcat(out,filename);
#else
 sprintf(out,"%s%s%s%s%s%s%s",root,*root?"/":"",prefix,*prefix?"/":"",modpath,"/native/",platform);
 if(strlen(out)+strlen(filename)+2U>=cap)return 0;
 strcat(out,"/");strcat(out,filename);
#endif
 (void)i;return 1;
}
static int try_plugin_path(char *out,size_t cap,const char *root,const char *prefix,const char *module,const char *platform,const char *filename) { if(!build_module_path(out,cap,root,prefix,module,platform,filename))return 0;return module_file_exists(out); }
static const char *native_platform(void) {
#ifdef _WIN32
# if defined(_WIN64)
    return "windows-x86_64";
# else
    return "windows-x86";
# endif
#elif defined(__aarch64__)
    return "linux-aarch64";
#elif defined(__arm__)
    return "linux-armv7";
#elif defined(__x86_64__)
    return "linux-x86_64";
#elif defined(__i386__)
    return "linux-x86";
#else
    return "linux-unknown";
#endif
}
static int find_native_path(const char *module,char *out,size_t cap) {
 char filename[512],candidate[2048],copy[4096],*p,*end;const char *env;const char *prefixes[3];int i;
 if(!safe_native_module(module)){set_error("unsafe native module name '%s'",module);return 0;}
#ifdef _WIN32
 sprintf(filename,"pipi_%s.dll",native_leaf(module));
#else
 sprintf(filename,"libpipi_%s.so",native_leaf(module));
#endif
 prefixes[0]="modules";prefixes[1]="packages";prefixes[2]="";
 env=getenv("PIPI_MODULE_PATH");
 if(env&&*env&&strlen(env)<sizeof(copy)) {
  strcpy(copy,env);p=copy;
  while(*p) {
#ifdef _WIN32
   end=strchr(p,';');
#else
   end=strchr(p,':');
#endif
   if(end)*end='\0';
   if(*p)for(i=0;i<3;i++)if(try_plugin_path(candidate,sizeof(candidate),p,prefixes[i],module,native_platform(),filename)) {
    if(strlen(candidate)<cap){strcpy(out,candidate);return 1;}
   }
   if(!end)break;
   p=end+1;
  }
 }
 for(i=0;i<3;i++)if(try_plugin_path(candidate,sizeof(candidate),".",prefixes[i],module,native_platform(),filename)) {
  if(strlen(candidate)<cap){strcpy(out,candidate);return 1;}
 }
 if(try_plugin_path(candidate,sizeof(candidate),"..","modules",module,native_platform(),filename)) {
  if(strlen(candidate)<cap){strcpy(out,candidate);return 1;}
 }
 /* pipiwood default install root: PIPI_HOME or HOME/.pipi. */
 env=getenv("PIPI_HOME");
 if(!env||!*env) {
#ifdef _WIN32
  env=getenv("USERPROFILE");
#else
  env=getenv("HOME");
#endif
  if(env&&*env&&strlen(env)+7U<sizeof(copy)){strcpy(copy,env);strcat(copy,"/.pipi");env=copy;}
 }
 if(env&&*env)for(i=0;i<2;i++)if(try_plugin_path(candidate,sizeof(candidate),env,prefixes[i],module,native_platform(),filename)) {
  if(strlen(candidate)<cap){strcpy(out,candidate);return 1;}
 }
 set_error("native module '%s' not found; expected %s under PIPI_MODULE_PATH, PIPI_HOME or ~/.pipi/modules/%s/native/%s/",module,filename,module,native_platform());return 0;
}
static NativeModule *find_loaded_native(const char *name) { int i;for(i=0;i<native_module_count;i++)if(eq(native_modules[i].name,name))return &native_modules[i];return NULL; }
static void close_native_handle(LibHandle handle) {
 if(!handle)return;
#ifdef _WIN32
 FreeLibrary(handle);
#else
 dlclose(handle);
#endif
}
static NativeModule *load_native_module(const char *name) {
 NativeModule *loaded;char path[2048],error[1024];PipiNativeModuleEntryV1 entry;
#ifdef _WIN32
 FARPROC symbol;
#else
 void *symbol;
#endif
 loaded=find_loaded_native(name);if(loaded)return loaded;
 if(native_module_count>=MAX_NATIVE_MODULES){set_error("too many native modules loaded");return NULL;}
 if(!find_native_path(name,path,sizeof(path)))return NULL;
#ifdef _WIN32
 { HMODULE handle=LoadLibraryA(path);if(!handle){set_error("LoadLibrary failed for native module '%s' at '%s' (Windows error %lu)",name,path,(unsigned long)GetLastError());return NULL;}symbol=GetProcAddress(handle,"pipi_native_module_v1");if(!symbol){FreeLibrary(handle);set_error("native module '%s' lacks pipi_native_module_v1",name);return NULL;}memcpy(&entry,&symbol,sizeof(entry));loaded=&native_modules[native_module_count];loaded->handle=handle; }
#else
 { void *handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!handle){set_error("dlopen failed for native module '%s' at '%s': %s",name,path,dlerror());return NULL;}symbol=dlsym(handle,"pipi_native_module_v1");if(!symbol){dlclose(handle);set_error("native module '%s' lacks pipi_native_module_v1",name);return NULL;}memcpy(&entry,&symbol,sizeof(entry));loaded=&native_modules[native_module_count];loaded->handle=handle; }
#endif
 loaded->api=entry();if(!loaded->api){close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module '%s' returned a NULL descriptor",name);return NULL;}
 if(loaded->api->abi_version!=PIPI_NATIVE_ABI_VERSION){unsigned long found_abi=loaded->api->abi_version;close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module '%s' uses ABI %lu; VM supports ABI %lu",name,found_abi,PIPI_NATIVE_ABI_VERSION);return NULL;}
 if(!eq(loaded->api->module_name,name)){const char *found_name=loaded->api->module_name;char name_copy[256];if(found_name){strncpy(name_copy,found_name,sizeof(name_copy)-1U);name_copy[sizeof(name_copy)-1U]='\0';}else strcpy(name_copy,"(null)");close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module identity mismatch: requested '%s', library identifies as '%s'",name,name_copy);return NULL;}
 if(!loaded->api->has_function||!loaded->api->call){close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module '%s' has an incomplete descriptor",name);return NULL;}
 error[0]='\0';if(loaded->api->initialize&&!loaded->api->initialize(&host_api,error,sizeof(error))){char error_copy[1024];strncpy(error_copy,error,sizeof(error_copy)-1U);error_copy[sizeof(error_copy)-1U]='\0';close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module '%s' initialization failed: %s",name,error_copy);return NULL;}
 if(strlen(name)>=sizeof(loaded->name)){close_native_handle(loaded->handle);memset(loaded,0,sizeof(*loaded));set_error("native module name too long");return NULL;}strcpy(loaded->name,name);native_module_count++;return loaded;
}
static Value call_native_target(const char *target,Value *args,int argc,int depth) {
 Value result=null_value();char module[256],symbol[160],error[1024];const char *body,*sep,*returned_string;size_t mlen,slen;NativeModule *loaded;int ok,returned_type;NativeCallContext context;PipiHostAPI call_host;
 if(!starts_with(target,"@native:")){set_error("invalid native target");return result;}body=target+8;sep=strrchr(body,':');if(!sep||sep==body||!sep[1]){set_error("malformed native call target '%s'",target);return result;}mlen=(size_t)(sep-body);slen=strlen(sep+1);if(mlen>=sizeof(module)||slen>=sizeof(symbol)){set_error("native module target too long");return result;}memcpy(module,body,mlen);module[mlen]='\0';strcpy(symbol,sep+1);loaded=load_native_module(module);if(!loaded)return result;if(!loaded->api->has_function(symbol)){set_error("native module '%s' does not export function '%s'",module,symbol);return result;}context.depth=depth;call_host=host_api;call_host.userdata=&context;error[0]='\0';ok=loaded->api->call(&call_host,symbol,argc,(const PipiValue *)args,&result,error,sizeof(error));if(!ok){set_error("native module '%s', function '%s': %s",module,symbol,error[0]?error:"call failed");return null_value();}if(result.type<PIPI_VALUE_NULL||result.type>PIPI_VALUE_FUNCTION){set_error("native module '%s' returned an invalid PIPI value type",module);return null_value();}if(result.type==PIPI_VALUE_STRING||result.type==PIPI_VALUE_FUNCTION){returned_type=result.type;returned_string=own_string(result.string?result.string:"");result=str_value(returned_string);result.type=returned_type;}return result;
}
static void host_log(void *userdata,int level,const char *message) { (void)userdata;if(level<=1)fprintf(stderr,"[PIPI native] %s\n",message?message:"");else fprintf(stdout,"[PIPI native] %s\n",message?message:""); }
static void host_set_error(void *userdata,const char *message) { (void)userdata;set_error("%s",message?message:"native module error"); }
static const char *host_get_env(void *userdata,const char *name) { (void)userdata;return getenv(name?name:""); }
static int is_builtin(const char *s) { static const char *names[]={"print","readLine","sleep","len","str","num","abs","sqrt","floor","ceil","round","random","min","max","window","label","button","show","run","message","close",NULL};int i;for(i=0;names[i];i++)if(eq(s,names[i]))return 1;return 0; }
static Value call_by_name(const char *name,Value *args,int argc,int depth) {
 Value r=null_value();int i,idx;double a,b;char buf[256];const char *s;
 if(starts_with(name,"@native:"))return call_native_target(name,args,argc,depth);
 if(eq(name,"print")){for(i=0;i<argc;i++){if(i)fputc(' ',stdout);s=value_string(args[i],buf,sizeof(buf));fputs(s,stdout);}fputc('\n',stdout);fflush(stdout);return r;}
 if(eq(name,"readLine")){char line[4096];if(fgets(line,sizeof(line),stdin)==NULL)return str_value("");line[strcspn(line,"\r\n")]='\0';return str_value(own_string(line));}
 if(eq(name,"sleep")){if(argc<1){set_error("sleep(seconds) expects 1 argument");return r;}a=value_number(args[0]);if(a<0)a=0;
#ifdef _WIN32
 Sleep((DWORD)(a*1000.0));
#else
 sleep((unsigned int)a);
#endif
 return r;}
 if(eq(name,"len")){if(argc!=1){set_error("len(value) expects 1 argument");return r;}if(args[0].type==V_STR) return num_value((double)strlen(args[0].string));return num_value(0);}
 if(eq(name,"str")){if(argc!=1){set_error("str(value) expects 1 argument");return r;}s=value_string(args[0],buf,sizeof(buf));return str_value(own_string(s));}
 if(eq(name,"num")){if(argc!=1){set_error("num(value) expects 1 argument");return r;}return num_value(value_number(args[0]));}
 if(eq(name,"abs")||eq(name,"sqrt")||eq(name,"floor")||eq(name,"ceil")||eq(name,"round")){if(argc!=1){set_error("numeric builtin expects 1 argument");return r;}a=value_number(args[0]);if(eq(name,"abs"))a=fabs(a);else if(eq(name,"sqrt")){if(a<0){set_error("sqrt of negative number");return r;}a=sqrt(a);}else if(eq(name,"floor"))a=floor(a);else if(eq(name,"ceil"))a=ceil(a);else a=floor(a+0.5);return num_value(a);}
 if(eq(name,"random")){if(argc>0){a=value_number(args[0]);if(a>0)return num_value((double)rand()/(double)RAND_MAX*a);}return num_value((double)rand()/(double)RAND_MAX);}
 if(eq(name,"min")||eq(name,"max")){if(argc<1){set_error("min/max expects at least one argument");return r;}a=value_number(args[0]);for(i=1;i<argc;i++){b=value_number(args[i]);if(eq(name,"min")&&b<a)a=b;if(eq(name,"max")&&b>a)a=b;}return num_value(a);}
 if(eq(name,"window")){long win;if(argc!=3){set_error("window(title, width, height) expects 3 arguments");return r;}if(!load_gui())return r;s=value_string(args[0],buf,sizeof(buf));win=gui_window_create(s,(int)value_number(args[1]),(int)value_number(args[2]));if(win<=0){set_error("native window creation failed (check DISPLAY or desktop session)");return r;}return num_value((double)win);}
 if(eq(name,"label")){if(argc!=4){set_error("label(window, text, x, y) expects 4 arguments");return r;}if(!load_gui())return r;s=value_string(args[1],buf,sizeof(buf));return num_value((double)gui_label_add((long)value_number(args[0]),s,(int)value_number(args[2]),(int)value_number(args[3])));}
 if(eq(name,"button")){int id;const char *cb;char buf2[256];if(argc!=5){set_error("button(window, text, x, y, callback) expects 5 arguments");return r;}if(!load_gui())return r;if(args[4].type!=V_FUNC){set_error("button callback must be a function name, e.g. button(..., onClick)");return r;}if(function_index(args[4].string)<0){set_error("unknown button callback function");return r;}s=value_string(args[1],buf,sizeof(buf));id=gui_button_add((long)value_number(args[0]),s,(int)value_number(args[2]),(int)value_number(args[3]));if(id<=0){set_error("native button creation failed");return r;}cb=args[4].string;strncpy(buf2,cb,sizeof(buf2)-1);buf2[sizeof(buf2)-1]='\0';add_callback((long)value_number(args[0]),id,buf2);return num_value((double)id);}
 if(eq(name,"show")){if(argc!=1){set_error("show(window) expects 1 argument");return r;}if(!load_gui())return r;return bool_value(gui_window_show((long)value_number(args[0]))!=0);}
 if(eq(name,"run")){long win;int event,found,j;if(argc!=1){set_error("run(window) expects 1 argument");return r;}if(!load_gui())return r;win=(long)value_number(args[0]);while(!vm_error[0]){event=gui_next_event(win);if(event<=0)break;found=0;for(j=0;j<callback_count;j++)if(callbacks[j].window==win&&callbacks[j].button==event){idx=function_index(callbacks[j].function);if(idx>=0){execute_function(idx,NULL,0,depth+1);found=1;}break;}(void)found;}return r;}
 if(eq(name,"message")){if(argc!=3){set_error("message(window, title, text) expects 3 arguments");return r;}if(!load_gui())return r;s=value_string(args[1],buf,sizeof(buf));{char textbuf[1024];const char *msg=value_string(args[2],textbuf,sizeof(textbuf));return bool_value(gui_message((long)value_number(args[0]),s,msg)!=0);}}
 if(eq(name,"close")){if(argc!=1){set_error("close(window) expects 1 argument");return r;}if(!load_gui())return r;gui_window_destroy((long)value_number(args[0]));return r;}
 idx=function_index(name);if(idx>=0)return execute_function(idx,args,argc,depth+1);
 {char msg[512];sprintf(msg,"unknown function or builtin '%s'",name);set_error(msg);}return r;
}
static int get_code_u4(Function *fn,unsigned long *pc,unsigned long *value) { if(*pc+4UL>fn->code_len){set_error("truncated instruction operand");return 0;}*value=((unsigned long)fn->code[*pc]<<24)|((unsigned long)fn->code[*pc+1]<<16)|((unsigned long)fn->code[*pc+2]<<8)|(unsigned long)fn->code[*pc+3];*pc+=4UL;return 1; }
static Value execute_function(int fi,Value *args,int argc,int depth) {
 Function *fn;unsigned long pc=0,target,idx;Value a,b,v,result=null_value();int op,i,old_vars,base_sp;char leftbuf[256],rightbuf[256];const char *ls,*rs;char *joined;size_t ln,rn;
 if(vm_error[0]) return result;
 if(depth>=255){set_error("maximum PIPI call depth exceeded");return result;}
 if(fi<0||fi>=function_count){set_error("invalid function index");return result;}
 fn=&functions[fi];
 if(argc!=fn->param_count){set_error("function '%s' expects %d argument(s), got %d",const_text((unsigned long)fn->name_const),fn->param_count,argc);return result;}
 old_vars=variable_count;base_sp=stack_top;frame_bases[depth]=old_vars;
 for(i=0;i<argc;i++){const char *param=const_text((unsigned long)fn->params[i]);if(!variable_declare(param,args[i],depth)){variable_count=old_vars;return result;}}
 while(pc<fn->code_len&&!vm_error[0]){op=(int)fn->code[pc++];switch(op){case OP_HALT:pc=fn->code_len;result=null_value();goto done;
 case OP_PUSH_NUM:if(!get_code_u4(fn,&pc,&idx))break;push(num_value(atof(const_text(idx))));break;
 case OP_PUSH_STR:if(!get_code_u4(fn,&pc,&idx))break;push(str_value(const_text(idx)));break;
 case OP_PUSH_BOOL:if(pc>=fn->code_len){set_error("truncated boolean instruction");break;}push(bool_value(fn->code[pc++]!=0));break;
 case OP_PUSH_NULL:push(null_value());break;
 case OP_LOAD:if(!get_code_u4(fn,&pc,&idx))break;if(symbol_load(const_text(idx),depth,&v))push(v);break;
 case OP_STORE:if(!get_code_u4(fn,&pc,&idx))break;v=pop();symbol_store(const_text(idx),depth,v);break;
 case OP_DECLARE:if(!get_code_u4(fn,&pc,&idx))break;v=pop();variable_declare(const_text(idx),v,depth);break;
 case OP_POP:pop();break;
 case OP_NEG:v=pop();push(num_value(-value_number(v)));break;
 case OP_NOT:v=pop();push(bool_value(!truthy(v)));break;
 case OP_ADD:b=pop();a=pop();if(a.type==V_STR||b.type==V_STR||a.type==V_FUNC||b.type==V_FUNC){ls=value_string(a,leftbuf,sizeof(leftbuf));rs=value_string(b,rightbuf,sizeof(rightbuf));ln=strlen(ls);rn=strlen(rs);joined=(char *)malloc(ln+rn+1U);if(!joined){set_error("out of memory concatenating string");break;}memcpy(joined,ls,ln);memcpy(joined+ln,rs,rn);joined[ln+rn]='\0';if(heap_string_count>=MAX_HEAP_STRINGS){free(joined);set_error("too many allocated strings");break;}heap_strings[heap_string_count++]=joined;push(str_value(joined));}else push(num_value(value_number(a)+value_number(b)));break;
 case OP_SUB:b=pop();a=pop();push(num_value(value_number(a)-value_number(b)));break;
 case OP_MUL:b=pop();a=pop();push(num_value(value_number(a)*value_number(b)));break;
 case OP_DIV:b=pop();a=pop();if(value_number(b)==0.0){set_error("division by zero");break;}push(num_value(value_number(a)/value_number(b)));break;
 case OP_MOD:b=pop();a=pop();if(value_number(b)==0.0){set_error("modulo by zero");break;}push(num_value(fmod(value_number(a),value_number(b))));break;
 case OP_EQ:b=pop();a=pop();push(bool_value(value_equal(a,b)));break;
 case OP_NE:b=pop();a=pop();push(bool_value(!value_equal(a,b)));break;
 case OP_LT:b=pop();a=pop();push(bool_value(value_number(a)<value_number(b)));break;
 case OP_LE:b=pop();a=pop();push(bool_value(value_number(a)<=value_number(b)));break;
 case OP_GT:b=pop();a=pop();push(bool_value(value_number(a)>value_number(b)));break;
 case OP_GE:b=pop();a=pop();push(bool_value(value_number(a)>=value_number(b)));break;
 case OP_AND:b=pop();a=pop();push(bool_value(truthy(a)&&truthy(b)));break;
 case OP_OR:b=pop();a=pop();push(bool_value(truthy(a)||truthy(b)));break;
 case OP_JUMP:if(!get_code_u4(fn,&pc,&target))break;if(target>fn->code_len){set_error("jump target outside function");break;}pc=target;break;
 case OP_JFALSE:if(!get_code_u4(fn,&pc,&target))break;v=pop();if(!truthy(v)){if(target>fn->code_len){set_error("conditional jump outside function");break;}pc=target;}break;
 case OP_CALL:if(!get_code_u4(fn,&pc,&idx))break;if(pc>=fn->code_len){set_error("truncated call operand");break;}i=(int)fn->code[pc++];if(i>stack_top){set_error("not enough arguments on stack");break;} {Value callargs[256];int start=stack_top-i;int j;for(j=0;j<i;j++)callargs[j]=stack_values[start+j];stack_top=start;if(is_builtin(const_text(idx))||starts_with(const_text(idx),"@native:"))v=call_by_name(const_text(idx),callargs,i,depth);else {int fidx=function_index(const_text(idx));if(fidx<0){char msg[512];sprintf(msg,"unknown function '%s'",const_text(idx));set_error(msg);v=null_value();}else v=execute_function(fidx,callargs,i,depth+1);}push(v);}break;
 case OP_PUSH_FUNC:if(!get_code_u4(fn,&pc,&idx))break;push(func_value(const_text(idx)));break;
 case OP_RETURN:result=pop();goto done;
 default:set_error("unknown opcode in PIPIBC06");break;
 }}
done: variable_count=old_vars;stack_top=base_sp;return result;
}
static int host_call_function(void *userdata,const char *name,int argc,const PipiValue *argv,PipiValue *result) {
 NativeCallContext *context=(NativeCallContext *)userdata;int idx,depth;Value value;
 if(!name||!result){set_error("native callback received invalid arguments");return 0;}
 idx=function_index(name);if(idx<0){set_error("native module requested unknown PIPI callback '%s'",name);return 0;}
 depth=context?context->depth+1:1;if(depth>=255){set_error("maximum PIPI callback depth exceeded");return 0;}
 value=execute_function(idx,(Value *)argv,argc,depth);*result=value;return vm_error[0]?0:1;
}
static void cleanup(void) { int i;for(i=native_module_count-1;i>=0;i--){if(native_modules[i].api&&native_modules[i].api->shutdown)native_modules[i].api->shutdown(&host_api);if(native_modules[i].handle){
#ifdef _WIN32
 FreeLibrary(native_modules[i].handle);
#else
 dlclose(native_modules[i].handle);
#endif
 }}for(i=0;i<constant_count;i++)free(constants[i].text);for(i=0;i<function_count;i++)free(functions[i].code);for(i=0;i<heap_string_count;i++)free(heap_strings[i]);if(gui_library){if(gui_shutdown)gui_shutdown();
#ifdef _WIN32
 FreeLibrary(gui_library);
#else
 dlclose(gui_library);
#endif
 }}
int main(int argc,char **argv) { Value result;if(argc==2&&eq(argv[1],"--version")){puts("PIPI VM 1.0.0 (PIPIBC06; native module ABI v1; no Java runtime)");return 0;}if(argc!=2){fprintf(stderr,"PIPI VM 1.0.0\nUsage: bin/pipi-vm <program.pbc>\n");return 2;}srand((unsigned int)time(NULL));if(!parse_bytecode(argv[1])){cleanup();return 1;}result=execute_function(0,NULL,0,0);(void)result;if(vm_error[0]){fprintf(stderr,"PIPI runtime error: %s\n",vm_error);cleanup();return 1;}cleanup();return 0;}
