/* PIPI C89 compiler v1.0.0: source -> PIPI bytecode (PIPIBC06). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define MAX_SOURCE (5UL * 1024UL * 1024UL)
#define MAX_TOKENS 50000
#define MAX_CONSTS 12000
#define MAX_FUNCS 256
#define MAX_PARAMS 64
#define MAX_IMPORTS 512

typedef struct { char *data; unsigned long len; unsigned long cap; } Str;
typedef struct { int kind; char *text; int line; } Token;
typedef struct { char *text; } Constant;
typedef struct { char name[128]; char target[512]; } ImportAlias;
typedef struct { char *name; int name_const; int params[MAX_PARAMS]; int param_count; int body_start; int body_end; Str code; } Function;

enum { T_EOF=0, T_ID=1, T_NUM=2, T_STR=3, T_SYM=4 };
enum { OP_HALT=0, OP_PUSH_NUM=1, OP_PUSH_STR=2, OP_PUSH_BOOL=3, OP_PUSH_NULL=4,
 OP_LOAD=5, OP_STORE=6, OP_DECLARE=7, OP_POP=8, OP_ADD=9, OP_SUB=10, OP_MUL=11,
 OP_DIV=12, OP_MOD=13, OP_NEG=14, OP_NOT=15, OP_EQ=16, OP_NE=17, OP_LT=18,
 OP_LE=19, OP_GT=20, OP_GE=21, OP_AND=22, OP_OR=23, OP_JUMP=24,
 OP_JFALSE=25, OP_CALL=26, OP_PUSH_FUNC=27, OP_RETURN=28 };

static Token *tok = NULL;
static int ntok = 0, tokcap = 0;
static Constant constants[MAX_CONSTS];
static int nconst = 0;
static Function funcs[MAX_FUNCS];
static int nfunc = 0;
static ImportAlias aliases[MAX_IMPORTS];
static int nalias = 0;
static char modules[128][256];
static int nmodules = 0;
static char err[1024];
static int pos = 0;
static int end_pos = 0;
static Str *outcode = NULL;
static int current_function = -1;

static int eq(const char *a, const char *b) { return a && b && strcmp(a,b)==0; }
static void fail(const char *fmt, ...) { va_list ap; if (err[0]) return; va_start(ap,fmt); vsprintf(err,fmt,ap); va_end(ap); }
static char *dupn(const char *s, unsigned long n) { char *p=(char *)malloc((size_t)n+1U); if(!p) return NULL; memcpy(p,s,(size_t)n); p[n]='\0'; return p; }
static void sb_init(Str *s) { s->data=NULL; s->len=0; s->cap=0; }
static int sb_reserve(Str *s, unsigned long n) { unsigned long need=s->len+n+1UL, cap; char *p; if(need<=s->cap)return 1; cap=s->cap?s->cap:256UL; while(cap<need){if(cap>0x7fffffffUL/2UL){cap=need;break;}cap*=2UL;}p=(char *)realloc(s->data,(size_t)cap);if(!p){fail("out of memory");return 0;}s->data=p;s->cap=cap;return 1; }
static int sb_putn(Str *s,const char *p,unsigned long n) { if(!sb_reserve(s,n))return 0;if(n)memcpy(s->data+s->len,p,(size_t)n);s->len+=n;s->data[s->len]='\0';return 1; }
static int sb_put1(Str *s,unsigned long v) { unsigned char c=(unsigned char)(v&255UL);return sb_putn(s,(const char *)&c,1UL); }
static int sb_put4(Str *s,unsigned long v) { return sb_put1(s,(v>>24)&255UL)&&sb_put1(s,(v>>16)&255UL)&&sb_put1(s,(v>>8)&255UL)&&sb_put1(s,v&255UL); }
static void patch4(Str *s, unsigned long at, unsigned long v) { if(at+4UL>s->len){fail("internal compiler patch overflow");return;}s->data[at]=(char)((v>>24)&255UL);s->data[at+1]=(char)((v>>16)&255UL);s->data[at+2]=(char)((v>>8)&255UL);s->data[at+3]=(char)(v&255UL); }
static int emit1(unsigned long v) { return sb_put1(outcode,v); }
static int emit4(unsigned long v) { return sb_put4(outcode,v); }
static unsigned long jump_placeholder(int op) { emit1((unsigned long)op); { unsigned long at=outcode->len; emit4(0); return at; } }
static void write_u4_file(FILE *f,unsigned long v) { fputc((int)((v>>24)&255UL),f);fputc((int)((v>>16)&255UL),f);fputc((int)((v>>8)&255UL),f);fputc((int)(v&255UL),f); }
static char *read_source(const char *path, unsigned long *len) { FILE *f;long sz;char *p;f=fopen(path,"rb");if(!f){fail("cannot open '%s'",path);return NULL;}if(fseek(f,0L,SEEK_END)!=0){fclose(f);fail("cannot seek '%s'",path);return NULL;}sz=ftell(f);if(sz<0||(unsigned long)sz>MAX_SOURCE){fclose(f);fail("source file too large or unreadable");return NULL;}if(fseek(f,0L,SEEK_SET)!=0){fclose(f);fail("cannot rewind source");return NULL;}p=(char *)malloc((size_t)sz+1U);if(!p){fclose(f);fail("out of memory reading source");return NULL;}if(fread(p,1U,(size_t)sz,f)!=(size_t)sz){free(p);fclose(f);fail("cannot read source");return NULL;}fclose(f);p[sz]='\0';*len=(unsigned long)sz;return p; }
static int id_start(unsigned char c) { return isalpha(c)||c=='_'||c=='$'; }
static int id_part(unsigned char c) { return isalnum(c)||c=='_'||c=='$'; }
static int add_token(int kind,const char *p,unsigned long n,int line) { Token *q;if(ntok>=MAX_TOKENS){fail("too many tokens");return 0;}if(ntok>=tokcap){int cap=tokcap?tokcap*2:512;if(cap>MAX_TOKENS)cap=MAX_TOKENS;q=(Token *)realloc(tok,(size_t)cap*sizeof(Token));if(!q){fail("out of memory tokenizing");return 0;}tok=q;tokcap=cap;}tok[ntok].kind=kind;tok[ntok].text=dupn(p,n);tok[ntok].line=line;if(!tok[ntok].text){fail("out of memory storing token");return 0;}ntok++;return 1; }
static int lex(const char *s,unsigned long len) { unsigned long i=0,st;int line=1;unsigned char c;char two[3];ntok=0;while(i<len){c=(unsigned char)s[i];if(isspace(c)){if(c=='\n')line++;i++;continue;}if(c=='/'&&i+1<len&&s[i+1]=='/'){while(i<len&&s[i]!='\n')i++;continue;}if(c=='/'&&i+1<len&&s[i+1]=='*'){int closed=0;i+=2;while(i<len){if(s[i]=='\n')line++;if(s[i]=='*'&&i+1<len&&s[i+1]=='/'){i+=2;closed=1;break;}i++;}if(!closed){fail("line %d: unterminated block comment",line);return 0;}continue;}if(id_start(c)){st=i++;while(i<len&&id_part((unsigned char)s[i]))i++;if(!add_token(T_ID,s+st,i-st,line))return 0;continue;}if(isdigit(c)){st=i++;while(i<len&&(isdigit((unsigned char)s[i])||s[i]=='.'))i++;if(!add_token(T_NUM,s+st,i-st,line))return 0;continue;}if(c=='"'||c=='\''){unsigned char quote=c;int ln=line;Str b;sb_init(&b);i++;while(i<len&&(unsigned char)s[i]!=quote){char ch=s[i++];if(ch=='\n')line++;if(ch=='\\'&&i<len){char e=s[i++];if(e=='n')ch='\n';else if(e=='r')ch='\r';else if(e=='t')ch='\t';else ch=e;}if(!sb_putn(&b,&ch,1UL)){free(b.data);return 0;}}if(i>=len){free(b.data);fail("line %d: unterminated string",ln);return 0;}i++;if(!add_token(T_STR,b.data?b.data:"",b.len,ln)){free(b.data);return 0;}free(b.data);continue;}two[0]=(char)c;two[1]=(i+1<len)?s[i+1]:'\0';two[2]='\0';if(eq(two,"==")||eq(two,"!=")||eq(two,"<=")||eq(two,">=")||eq(two,"&&")||eq(two,"||")||eq(two,"+=")||eq(two,"-=")){if(!add_token(T_SYM,two,2UL,line))return 0;i+=2;continue;}if(strchr("{}()[],:;. +-*/%!<>=|",c)!=NULL){if(!add_token(T_SYM,s+i,1UL,line))return 0;i++;continue;}fail("line %d: unexpected character '%c'",line,c);return 0;}return add_token(T_EOF,"<eof>",5UL,line); }
static int constant(const char *s) { int i;for(i=0;i<nconst;i++)if(eq(constants[i].text,s))return i;if(nconst>=MAX_CONSTS){fail("too many constants");return 0;}constants[nconst].text=dupn(s,(unsigned long)strlen(s));if(!constants[nconst].text){fail("out of memory storing constant");return 0;}return nconst++; }
static int is_safe_module(const char *s) { const unsigned char *p=(const unsigned char *)s;if(!*s||strstr(s,"..")||*s=='/'||*s=='\\')return 0;for(;*p;p++)if(!(isalnum(*p)||*p=='/'||*p=='_'||*p=='-'||*p=='.'))return 0;return 1; }
static int file_exists(const char *path) { FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1; }
static const char *module_leaf(const char *module) { const char *p=strrchr(module,'/');return p?p+1:module; }
static int save_path(char *out,size_t cap,const char *path) { if(strlen(path)>=cap)return 0;strcpy(out,path);return 1; }
static int try_manifest(char *out,size_t cap,const char *path) { if(file_exists(path))return save_path(out,cap,path);return 0; }
static int find_manifest(const char *module,char *out,size_t cap) {
 char candidate[2048],copy[4096],*p,*end;const char *env;size_t need;
 if(!is_safe_module(module)){fail("unsafe import path '%s'",module);return 0;}
 need=strlen(module)+16U;
 if(need<sizeof(candidate)){sprintf(candidate,"lib/%s.pmod",module);if(try_manifest(out,cap,candidate))return 1;}
 if(strlen(module)+32U<sizeof(candidate)){sprintf(candidate,"modules/%s/module.pmod",module);if(try_manifest(out,cap,candidate))return 1;sprintf(candidate,"modules/%s/%s.pmod",module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;sprintf(candidate,"packages/%s/module.pmod",module);if(try_manifest(out,cap,candidate))return 1;sprintf(candidate,"packages/%s/%s.pmod",module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;}
 env=getenv("PIPI_LIB_PATH");
 if(env&&*env&&strlen(env)<sizeof(copy)){
  strcpy(copy,env);p=copy;
  while(*p){
#ifdef _WIN32
   end=strchr(p,';');
#else
   end=strchr(p,':');
#endif
   if(end)*end='\0';
   if(*p){if(strlen(p)+strlen(module)+8U<sizeof(candidate)){sprintf(candidate,"%s/%s.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;sprintf(candidate,"%s/%s/module.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;sprintf(candidate,"%s/%s/%s.pmod",p,module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;}}
   if(!end)break;
   p=end+1;
  }
 }
 env=getenv("PIPI_MODULE_PATH");
 if(env&&*env&&strlen(env)<sizeof(copy)){
  strcpy(copy,env);p=copy;
  while(*p){
#ifdef _WIN32
   end=strchr(p,';');
#else
   end=strchr(p,':');
#endif
   if(end)*end='\0';
   if(*p&&strlen(p)+strlen(module)+64U<sizeof(candidate)){
    sprintf(candidate,"%s/lib/%s.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;
    sprintf(candidate,"%s/modules/%s/module.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;
    sprintf(candidate,"%s/modules/%s/%s.pmod",p,module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;
    sprintf(candidate,"%s/%s/module.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;
    sprintf(candidate,"%s/%s/%s.pmod",p,module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;
    sprintf(candidate,"%s/%s.pmod",p,module);if(try_manifest(out,cap,candidate))return 1;
   }
   if(!end)break;
   p=end+1;
  }
 }
 /* pipiwood default install root: PIPI_HOME or HOME/.pipi. */
 env=getenv("PIPI_HOME");
 if(!env||!*env) {
#ifdef _WIN32
  env=getenv("USERPROFILE");
#else
  env=getenv("HOME");
#endif
  if(env&&*env&&strlen(env)+7U<sizeof(copy)) { strcpy(copy,env); strcat(copy,"/.pipi"); env=copy; }
 }
 if(env&&*env&&strlen(env)+strlen(module)+64U<sizeof(candidate)) {
  sprintf(candidate,"%s/lib/%s.pmod",env,module);if(try_manifest(out,cap,candidate))return 1;
  sprintf(candidate,"%s/modules/%s/module.pmod",env,module);if(try_manifest(out,cap,candidate))return 1;
  sprintf(candidate,"%s/modules/%s/%s.pmod",env,module,module_leaf(module));if(try_manifest(out,cap,candidate))return 1;
 }
 return 0;
}
static int module_is_native_plugin(const char *module) { char path[2048],line[1024],*p,*q;FILE *f;int result=0;if(!find_manifest(module,path,sizeof(path)))return 0;f=fopen(path,"r");if(!f)return 0;while(fgets(line,sizeof(line),f)){p=line;while(isspace((unsigned char)*p))p++;if(strncmp(p,"kind",4)==0&&isspace((unsigned char)p[4])){p+=4;while(isspace((unsigned char)*p))p++;q=p;while(*q&&!isspace((unsigned char)*q))q++;if((size_t)(q-p)==13U&&strncmp(p,"native-plugin",13U)==0)result=1;break;}}fclose(f);return result; }
static int exported(const char *module,const char *symbol) { char path[2048],line[2048],*p,*q;FILE *f;int found=0;if(!find_manifest(module,path,sizeof(path))){fail("module '%s' not found (searched lib/, PIPI_LIB_PATH and PIPI_MODULE_PATH)",module);return 0;}f=fopen(path,"r");if(!f){fail("cannot read module manifest '%s'",path);return 0;}while(fgets(line,sizeof(line),f)){p=line;while(isspace((unsigned char)*p))p++;if(strncmp(p,"exports",7)==0&&isspace((unsigned char)p[7])){p+=7;while(*p){while(isspace((unsigned char)*p))p++;if(!*p)break;q=p;while(*q&&!isspace((unsigned char)*q))q++;if((size_t)(q-p)==strlen(symbol)&&strncmp(p,symbol,(size_t)(q-p))==0)found=1;p=q;}}}fclose(f);if(!found)fail("module '%s' does not export '%s'",module,symbol);return found; }
static int alias_add(const char *local,const char *target) { int i;if(nalias>=MAX_IMPORTS){fail("too many imported symbols");return 0;}for(i=0;i<nalias;i++)if(eq(aliases[i].name,local)){fail("duplicate imported name '%s'",local);return 0;}if(strlen(local)>=sizeof(aliases[0].name)||strlen(target)>=sizeof(aliases[0].target)){fail("imported name too long");return 0;}strcpy(aliases[nalias].name,local);strcpy(aliases[nalias].target,target);nalias++;return 1; }
static const char *resolve_name(const char *name) { int i;for(i=0;i<nalias;i++)if(eq(aliases[i].name,name))return aliases[i].target;return name; }
static int module_add(const char *name) { int i;if(strlen(name)>=sizeof(modules[0])){fail("module path too long");return 0;}for(i=0;i<nmodules;i++)if(eq(modules[i],name))return 1;if(nmodules>=128){fail("too many imported modules");return 0;}strcpy(modules[nmodules++],name);return 1; }
static int matching(int start,const char *open,const char *close,int limit) { int i,depth=0;for(i=start;i<limit;i++){if(eq(tok[i].text,open))depth++;else if(eq(tok[i].text,close)){depth--;if(depth==0)return i;}}return -1; }
static int parse_import_header(int at,int *after) { int i=at+1,j,native_plugin;char names[128][128],locals[128][128],module[256],target[512];int count=0;while(i<ntok&&eq(tok[i].text,"\n"))i++;if(!eq(tok[i].text,"{")){fail("line %d: expected '{' after import",tok[at].line);return 0;}i++;while(i<ntok&&!eq(tok[i].text,"}")){if(tok[i].kind!=T_ID){fail("line %d: expected imported symbol",tok[i].line);return 0;}if(count>=128){fail("too many symbols in import");return 0;}if(strlen(tok[i].text)>=sizeof(names[0])){fail("imported symbol too long");return 0;}strcpy(names[count],tok[i].text);strcpy(locals[count],tok[i].text);i++;if(eq(tok[i].text,"as")){i++;if(tok[i].kind!=T_ID){fail("line %d: expected alias name",tok[i].line);return 0;}strcpy(locals[count],tok[i].text);i++;}count++;if(eq(tok[i].text,","))i++;else if(!eq(tok[i].text,"}")){fail("line %d: expected ',' or '}' in import",tok[i].line);return 0;}}if(!eq(tok[i].text,"}")){fail("line %d: unclosed import",tok[at].line);return 0;}i++;if(!eq(tok[i].text,"from")){fail("line %d: expected 'from' after import list",tok[at].line);return 0;}i++;if(tok[i].kind!=T_STR){fail("line %d: expected module string",tok[i].line);return 0;}if(strlen(tok[i].text)>=sizeof(module)){fail("module name too long");return 0;}strcpy(module,tok[i].text);i++;if(eq(tok[i].text,";"))i++;if(!module_add(module))return 0;native_plugin=module_is_native_plugin(module);for(j=0;j<count;j++){if(!exported(module,names[j]))return 0;if(native_plugin){if(strlen(module)+strlen(names[j])+10U>=sizeof(target)){fail("native import target too long");return 0;}memcpy(target,"@native:",8U);memcpy(target+8U,module,strlen(module));target[8U+strlen(module)]=':';memcpy(target+9U+strlen(module),names[j],strlen(names[j])+1U);}else strcpy(target,names[j]);if(!alias_add(locals[j],target))return 0;}*after=i;return 1; }
static int add_function(const char *name,int start,int finish,int *params,int nparams) { int i;Function *f;if(nfunc>=MAX_FUNCS){fail("too many functions (limit %d)",MAX_FUNCS);return -1;}for(i=0;i<nfunc;i++)if(eq(funcs[i].name,name)){fail("duplicate function '%s'",name);return -1;}if(nparams>MAX_PARAMS){fail("function '%s' has too many parameters",name);return -1;}f=&funcs[nfunc];memset(f,0,sizeof(*f));f->name=dupn(name,(unsigned long)strlen(name));f->name_const=constant(name);f->body_start=start;f->body_end=finish;f->param_count=nparams;for(i=0;i<nparams;i++)f->params[i]=params[i];sb_init(&f->code);return nfunc++; }
static int scan_function_header(int at,int *after) { int i=at+1,j,parclose,brace,braceclose;int params[MAX_PARAMS],np=0;char name[128];if(tok[i].kind!=T_ID){fail("line %d: expected function name",tok[at].line);return 0;}if(strlen(tok[i].text)>=sizeof(name)){fail("function name too long");return 0;}strcpy(name,tok[i].text);i++;if(!eq(tok[i].text,"(")){fail("line %d: expected '(' after function name",tok[i].line);return 0;}parclose=matching(i,"(",")",ntok);if(parclose<0){fail("line %d: unclosed parameter list",tok[at].line);return 0;}j=i+1;while(j<parclose){if(tok[j].kind!=T_ID){fail("line %d: expected parameter name",tok[j].line);return 0;}if(np>=MAX_PARAMS){fail("too many parameters");return 0;}params[np++]=constant(tok[j].text);j++;if(eq(tok[j].text,":")){j++;while(j<parclose&&!eq(tok[j].text,",")&&!eq(tok[j].text,")"))j++;}if(eq(tok[j].text,","))j++;else if(j<parclose&&!eq(tok[j].text,")")){fail("line %d: expected ',' in parameters",tok[j].line);return 0;}}i=parclose+1;if(eq(tok[i].text,":")){i++;while(i<ntok&&!eq(tok[i].text,"{"))i++;}if(!eq(tok[i].text,"{")){fail("line %d: expected function body",tok[at].line);return 0;}brace=i;braceclose=matching(brace,"{","}",ntok);if(braceclose<0){fail("line %d: unclosed function body",tok[at].line);return 0;}if(eq(name,"main")){fail("function name 'main' is reserved");return 0;}if(add_function(name,brace+1,braceclose,params,np)<0)return 0;*after=braceclose+1;return 1; }
static int scan_headers(void) { int i=0,after,depth=0;int main_params[1];main_params[0]=0;add_function("$entry",0,0,main_params,0);while(i<ntok-1){if(depth==0&&eq(tok[i].text,"import")){if(!parse_import_header(i,&after))return 0;i=after;continue;}if(depth==0&&eq(tok[i].text,"fn")){if(!scan_function_header(i,&after))return 0;i=after;continue;}if(eq(tok[i].text,"{"))depth++;else if(eq(tok[i].text,"}")){if(depth>0)depth--;}i++;}return !err[0]; }
static int is_function(const char *name) { int i;for(i=1;i<nfunc;i++)if(eq(funcs[i].name,name))return 1;return 0; }
/* Keep the compiler and VM's built-in call table in sync. Imported names are
 * checked separately so native-plugin functions cannot be called by accident
 * without an import. */
static int is_builtin_callable(const char *name) {
 static const char *names[]={"print","readLine","sleep","len","str","num","abs","sqrt","floor","ceil","round","random","min","max","window","label","button","show","run","message","close",NULL};
 int i;for(i=0;names[i];i++)if(eq(name,names[i]))return 1;return 0;
}
static int is_imported_name(const char *name) { int i;for(i=0;i<nalias;i++)if(eq(aliases[i].name,name))return 1;return 0; }
static int is_known_callable(const char *name) { return is_function(name)||is_imported_name(name)||is_builtin_callable(name); }
static void skip_semi(void) { if(eq(tok[pos].text,";"))pos++; }
static void skip_import_or_function(void) { int i,brace,close;if(eq(tok[pos].text,"import")){i=pos+1;if(eq(tok[i].text,"{")){close=matching(i,"{","}",ntok);if(close>=0){i=close+1;if(eq(tok[i].text,"from"))i++;if(tok[i].kind==T_STR)i++;if(eq(tok[i].text,";"))i++;pos=i;return;}}pos++;}else if(eq(tok[pos].text,"fn")){i=pos+1;while(i<ntok&&!eq(tok[i].text,"("))i++;close=matching(i,"(",")",ntok);if(close<0){pos++;return;}brace=close+1;while(brace<ntok&&!eq(tok[brace].text,"{"))brace++;if(brace>=ntok){pos++;return;}close=matching(brace,"{","}",ntok);pos=close>=0?close+1:brace+1;} }
static int precedence(const char *op) { if(eq(op,"||"))return 1;if(eq(op,"&&"))return 2;if(eq(op,"==")||eq(op,"!="))return 3;if(eq(op,"<")||eq(op,"<=")||eq(op,">")||eq(op,">="))return 4;if(eq(op,"+")||eq(op,"-"))return 5;if(eq(op,"*")||eq(op,"/")||eq(op,"%"))return 6;return 0; }
static int opcode_for(const char *op) { if(eq(op,"+"))return OP_ADD;if(eq(op,"-"))return OP_SUB;if(eq(op,"*"))return OP_MUL;if(eq(op,"/"))return OP_DIV;if(eq(op,"%"))return OP_MOD;if(eq(op,"=="))return OP_EQ;if(eq(op,"!="))return OP_NE;if(eq(op,"<"))return OP_LT;if(eq(op,"<="))return OP_LE;if(eq(op,">"))return OP_GT;if(eq(op,">="))return OP_GE;if(eq(op,"&&"))return OP_AND;if(eq(op,"||"))return OP_OR;return -1; }
static int parse_expr(int minp);
static int parse_primary(void) { Token t=tok[pos];int ci,nargs=0;const char *name;if(t.kind==T_NUM){ci=constant(t.text);emit1(OP_PUSH_NUM);emit4((unsigned long)ci);pos++;return !err[0];}if(t.kind==T_STR){ci=constant(t.text);emit1(OP_PUSH_STR);emit4((unsigned long)ci);pos++;return !err[0];}if(eq(t.text,"true")||eq(t.text,"false")){emit1(OP_PUSH_BOOL);emit1(eq(t.text,"true")?1UL:0UL);pos++;return 1;}if(eq(t.text,"null")){emit1(OP_PUSH_NULL);pos++;return 1;}if(eq(t.text,"(") ){pos++;if(!parse_expr(1))return 0;if(!eq(tok[pos].text,")")){fail("line %d: expected ')'",tok[pos].line);return 0;}pos++;return 1;}if(t.kind==T_ID){name=t.text;pos++;if(eq(tok[pos].text,"(")){if(!is_known_callable(name)){fail("line %d: unknown function '%s'; import it from a module or define it with fn",t.line,name);return 0;}pos++;if(!eq(tok[pos].text,")")){for(;;){if(!parse_expr(1))return 0;nargs++;if(nargs>255){fail("line %d: maximum 255 call arguments",t.line);return 0;}if(eq(tok[pos].text,",")){pos++;continue;}break;}}if(!eq(tok[pos].text,")")){fail("line %d: expected ')' after call arguments",tok[pos].line);return 0;}pos++;ci=constant(resolve_name(name));emit1(OP_CALL);emit4((unsigned long)ci);emit1((unsigned long)nargs);return !err[0];}if(is_function(name)){ci=constant(name);emit1(OP_PUSH_FUNC);emit4((unsigned long)ci);}else{ci=constant(name);emit1(OP_LOAD);emit4((unsigned long)ci);}return !err[0];}fail("line %d: expected expression, got '%s'",t.line,t.text);return 0; }
static int parse_unary(void) { if(eq(tok[pos].text,"-")||eq(tok[pos].text,"!")){int op=eq(tok[pos].text,"-")?OP_NEG:OP_NOT;int ln=tok[pos].line;pos++;if(!parse_unary()){return 0;}emit1((unsigned long)op);(void)ln;return 1;}return parse_primary(); }
static int parse_expr(int minp) { int p,op;char oper[3];if(!parse_unary())return 0;while((p=precedence(tok[pos].text))>=minp&&p>0){if(strlen(tok[pos].text)>=sizeof(oper)){fail("line %d: invalid operator",tok[pos].line);return 0;}strcpy(oper,tok[pos].text);pos++;if(!parse_expr(p+1))return 0;op=opcode_for(oper);emit1((unsigned long)op);}return !err[0]; }
static int type_annotation_until(const char *stop) { if(eq(tok[pos].text,":")){pos++;while(tok[pos].kind!=T_EOF&&!eq(tok[pos].text,stop)&&!eq(tok[pos].text,";")){if(eq(tok[pos].text,"{"))break;pos++;}return 1;}return 1; }
static int parse_statement(void);
static int parse_block(void) { if(!eq(tok[pos].text,"{")){fail("line %d: expected '{'",tok[pos].line);return 0;}pos++;while(pos<end_pos&&!eq(tok[pos].text,"}")){if(tok[pos].kind==T_EOF){fail("unexpected end of function");return 0;}if(!parse_statement())return 0;}if(!eq(tok[pos].text,"}")){fail("line %d: expected '}'",tok[pos].line);return 0;}pos++;return 1; }
static int parse_statement(void) { Token t=tok[pos];int idx;unsigned long j1,j2;char name[128];if(eq(t.text,";")){pos++;return 1;}if(eq(t.text,"{"))return parse_block();if(eq(t.text,"import")||eq(t.text,"fn")){skip_import_or_function();return 1;}if(eq(t.text,"let")||eq(t.text,"const")||eq(t.text,"var")){pos++;if(tok[pos].kind!=T_ID){fail("line %d: expected variable name",tok[pos].line);return 0;}idx=constant(tok[pos].text);pos++;type_annotation_until("=");if(eq(tok[pos].text,"=")){pos++;if(!parse_expr(1))return 0;}else emit1(OP_PUSH_NULL);emit1(OP_DECLARE);emit4((unsigned long)idx);skip_semi();return !err[0];}if(eq(t.text,"if")){pos++;if(eq(tok[pos].text,"(")){pos++;if(!parse_expr(1))return 0;if(!eq(tok[pos].text,")")){fail("line %d: expected ')' after if condition",tok[pos].line);return 0;}pos++;}else if(!parse_expr(1))return 0;j1=jump_placeholder(OP_JFALSE);if(!parse_statement())return 0;if(eq(tok[pos].text,"else")){pos++;j2=jump_placeholder(OP_JUMP);patch4(outcode,j1,outcode->len);if(eq(tok[pos].text,"if")){if(!parse_statement())return 0;}else if(!parse_statement())return 0;patch4(outcode,j2,outcode->len);}else patch4(outcode,j1,outcode->len);return !err[0];}if(eq(t.text,"while")){unsigned long start;pos++;start=outcode->len;if(eq(tok[pos].text,"(")){pos++;if(!parse_expr(1))return 0;if(!eq(tok[pos].text,")")){fail("line %d: expected ')' after while condition",tok[pos].line);return 0;}pos++;}else if(!parse_expr(1))return 0;j1=jump_placeholder(OP_JFALSE);if(!parse_statement())return 0;emit1(OP_JUMP);emit4(start);patch4(outcode,j1,outcode->len);return !err[0];}if(eq(t.text,"return")){pos++;if(eq(tok[pos].text,";")||eq(tok[pos].text,"}"))emit1(OP_PUSH_NULL);else if(!parse_expr(1))return 0;emit1(OP_RETURN);skip_semi();return !err[0];}if(eq(t.text,"break")||eq(t.text,"continue")){fail("line %d: break/continue are not supported in v0.5 yet",t.line);return 0;}if(t.kind==T_ID&&(eq(tok[pos+1].text,"=")||eq(tok[pos+1].text,"+=")||eq(tok[pos+1].text,"-="))){strcpy(name,t.text);pos++;if(eq(tok[pos].text,"=")){pos++;if(!parse_expr(1))return 0;}else{int add=eq(tok[pos].text,"+=");pos++;idx=constant(name);emit1(OP_LOAD);emit4((unsigned long)idx);if(!parse_expr(1))return 0;emit1(add?OP_ADD:OP_SUB);}idx=constant(name);emit1(OP_STORE);emit4((unsigned long)idx);skip_semi();return !err[0];}if(!parse_expr(1))return 0;emit1(OP_POP);skip_semi();return !err[0]; }
static int compile_function(int fi) { int savedpos=pos;int savedend=end_pos;Str *savedcode=outcode;int savedi=current_function;Function *f=&funcs[fi];outcode=&f->code;sb_init(outcode);current_function=fi;pos=f->body_start;end_pos=f->body_end;while(pos<end_pos&&!err[0])if(!parse_statement())break;if(!err[0]){emit1(OP_PUSH_NULL);emit1(OP_RETURN);}pos=savedpos;end_pos=savedend;outcode=savedcode;current_function=savedi;return !err[0]; }
static int compile_main(void) { int savedpos=pos;int savedend=end_pos;Str *savedcode=outcode;int savedi=current_function;Function *f=&funcs[0];outcode=&f->code;sb_init(outcode);current_function=0;pos=0;end_pos=ntok-1;while(pos<end_pos&&!err[0]){if(eq(tok[pos].text,"import")||eq(tok[pos].text,"fn")){skip_import_or_function();continue;}if(!parse_statement())break;}if(!err[0])emit1(OP_HALT);pos=savedpos;end_pos=savedend;outcode=savedcode;current_function=savedi;return !err[0]; }
static int write_bytecode(const char *path,unsigned long *written) { FILE *f;int i,j;unsigned char header[8]={'P','I','P','I','B','C','0','6'};f=fopen(path,"wb");if(!f){fail("cannot create '%s'",path);return 0;}if(fwrite(header,1U,8U,f)!=8U){fclose(f);fail("failed writing bytecode header");return 0;}write_u4_file(f,(unsigned long)nconst);for(i=0;i<nconst;i++){unsigned long n=(unsigned long)strlen(constants[i].text);write_u4_file(f,n);if(n&&fwrite(constants[i].text,1U,(size_t)n,f)!=(size_t)n){fclose(f);fail("failed writing constant pool");return 0;}}write_u4_file(f,(unsigned long)nfunc);for(i=0;i<nfunc;i++){write_u4_file(f,(unsigned long)funcs[i].name_const);write_u4_file(f,(unsigned long)funcs[i].param_count);for(j=0;j<funcs[i].param_count;j++)write_u4_file(f,(unsigned long)funcs[i].params[j]);write_u4_file(f,funcs[i].code.len);if(funcs[i].code.len&&fwrite(funcs[i].code.data,1U,(size_t)funcs[i].code.len,f)!=(size_t)funcs[i].code.len){fclose(f);fail("failed writing function bytecode");return 0;}}if(written)*written=(unsigned long)ftell(f);if(fclose(f)!=0){fail("failed closing bytecode output");return 0;}return 1; }
static void usage(void) { printf("PIPI C89 compiler v1.0.0 — extensible native modules\n\nUsage:\n  bin/pipi-c check <file.pipi>\n  bin/pipi-c compile <file.pipi> -o <file.pbc>\n  bin/pipi-c build <file.pipi> -o <file.pbc>\n\nPIPI_LIB_PATH / PIPI_MODULE_PATH add external package roots; pipiwood installs to PIPI_HOME or ~/.pipi.\nRun with: bin/pipi-vm <file.pbc>\n"); }
int main(int argc,char **argv) { const char *cmd,*in,*out=NULL;char *source;unsigned long len,written=0;int i,ok;if(argc==2&&eq(argv[1],"--version")){puts("PIPI compiler 1.0.0 (PIPIBC06 bytecode; native module ABI v1)");return 0;}if(argc<3){usage();return 2;}cmd=argv[1];in=argv[2];for(i=3;i<argc;i++){if(eq(argv[i],"-o")&&i+1<argc)out=argv[++i];else{fprintf(stderr,"unknown option: %s\n",argv[i]);return 2;}}if(!eq(cmd,"check")&&!eq(cmd,"compile")&&!eq(cmd,"build")){usage();return 2;}if(!eq(cmd,"check")&&!out){fprintf(stderr,"error: -o output path required\n");return 2;}source=read_source(in,&len);if(!source){fprintf(stderr,"PIPI: %s\n",err);return 1;}if(!lex(source,len)){fprintf(stderr,"PIPI lexer: %s\n",err);free(source);return 1;}if(!scan_headers()){fprintf(stderr,"PIPI imports/parser: %s\n",err);free(source);return 1;}for(i=1;i<nfunc;i++)if(!compile_function(i))break;if(!err[0])compile_main();if(err[0]){fprintf(stderr,"PIPI compile error: %s\n",err);free(source);return 1;}if(eq(cmd,"check")){printf("PIPI v1.0.0 OK: %s; %d module(s), %d user function(s); all calls resolved.\n",in,nmodules,nfunc-1);free(source);return 0;}ok=write_bytecode(out,&written);if(!ok){fprintf(stderr,"PIPI output error: %s\n",err);free(source);return 1;}printf("PIPI v1.0.0 compiled %s -> %s (%d user function(s), %d constant(s), %lu bytes; PIPIBC06)\n",in,out,nfunc-1,nconst,written);free(source);for(i=0;i<ntok;i++)free(tok[i].text);free(tok);for(i=0;i<nconst;i++)free(constants[i].text);for(i=0;i<nfunc;i++){free(funcs[i].name);free(funcs[i].code.data);}return 0; }
