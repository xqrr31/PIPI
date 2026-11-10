/*
 * PIPIwood v1.0.0 - C89 package packer and installer.
 * WOODLIB1 format: magic[8], entry_count[u32 BE], then
 * path_len[u16 BE], data_len[u32 BE], crc32[u32 BE], path, file bytes.
 * Uncompressed and intentionally simple; all paths are validated before use.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(p) _mkdir(p)
#define UNLINK(p) _unlink(p)
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir((p),0755)
#define UNLINK(p) unlink(p)
#define PATH_SEP '/'
#endif

#define WOOD_MAGIC "WOODLIB1"
#define MAX_ENTRIES 4096UL
#define MAX_PATH_LEN 511U
#define MAX_FILE_SIZE (64UL*1024UL*1024UL)
#define MAX_TOTAL_SIZE (256UL*1024UL*1024UL)
#define MAX_LINE 2048
#define MAX_FILES 4096

static char error_text[512];
static void fail(const char *s) { if(!error_text[0]) { strncpy(error_text,s,sizeof(error_text)-1U); error_text[sizeof(error_text)-1U]='\0'; } }
static int eq(const char *a,const char *b) { return a&&b&&strcmp(a,b)==0; }
static unsigned long crc32_update(unsigned long crc,const unsigned char *p,unsigned long n) {
    unsigned long i,j; crc=crc^0xffffffffUL;
    for(i=0;i<n;i++){crc^=(unsigned long)p[i];for(j=0;j<8UL;j++)crc=(crc&1UL)?((crc>>1)^0xedb88320UL):(crc>>1);}
    return crc^0xffffffffUL;
}
static void put_u2(FILE *f,unsigned long v){fputc((int)((v>>8)&255UL),f);fputc((int)(v&255UL),f);}
static void put_u4(FILE *f,unsigned long v){fputc((int)((v>>24)&255UL),f);fputc((int)((v>>16)&255UL),f);fputc((int)((v>>8)&255UL),f);fputc((int)(v&255UL),f);}
static int get_u2(FILE *f,unsigned long *v){int a=fgetc(f),b=fgetc(f);if(a==EOF||b==EOF)return 0;*v=((unsigned long)a<<8)|(unsigned long)b;return 1;}
static int get_u4(FILE *f,unsigned long *v){int a=fgetc(f),b=fgetc(f),c=fgetc(f),d=fgetc(f);if(a==EOF||b==EOF||c==EOF||d==EOF)return 0;*v=((unsigned long)a<<24)|((unsigned long)b<<16)|((unsigned long)c<<8)|(unsigned long)d;return 1;}
static int safe_relpath(const char *p) {
    const unsigned char *q=(const unsigned char *)p; const char *part=p;
    if(!p||!*p||*p=='/'||*p=='\\'||strchr(p,':')||strlen(p)>MAX_PATH_LEN)return 0;
    for(;*q;q++) if(!(isalnum(*q)||*q=='_'||*q=='-'||*q=='.'||*q=='/'))return 0;
    while(*part){const char *slash=strchr(part,'/');size_t n=slash?(size_t)(slash-part):strlen(part);if(n==0U||(n==1U&&part[0]=='.')||(n==2U&&part[0]=='.'&&part[1]=='.'))return 0;if(!slash)break;part=slash+1;}
    return 1;
}
static int safe_archive_path(const char *p) {
    if(!safe_relpath(p)) return 0;
    if(strcmp(p,".installed")==0 || strncmp(p,".installed/",11U)==0) return 0;
    if(strcmp(p,".woodlib")==0 || (strncmp(p,".woodlib/",9U)==0 && strcmp(p,".woodlib/manifest")!=0)) return 0;
    return 1;
}
static int safe_package_name(const char *p) {
    if(!safe_relpath(p)) return 0;
    if(strcmp(p,".installed")==0 || strncmp(p,".installed/",11U)==0 || strcmp(p,".woodlib")==0 || strncmp(p,".woodlib/",9U)==0) return 0;
    return 1;
}
static int read_file(const char *path,unsigned char **data,unsigned long *size) {
    FILE *f; long n; unsigned char *p;
    f=fopen(path,"rb");if(!f){fprintf(stderr,"pipiwood: cannot open '%s': %s\n",path,strerror(errno));return 0;}
    if(fseek(f,0L,SEEK_END)!=0){fclose(f);return 0;}n=ftell(f);if(n<0||(unsigned long)n>MAX_FILE_SIZE){fclose(f);fprintf(stderr,"pipiwood: file too large: %s\n",path);return 0;}
    if(fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}p=(unsigned char *)malloc((size_t)n+1U);if(!p){fclose(f);return 0;}
    if(n>0&&fread(p,1U,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return 0;}fclose(f);*data=p;*size=(unsigned long)n;return 1;
}
static int write_entry(FILE *out,const char *archive_path,const char *source_path) {
    unsigned char *data;unsigned long size,crc;size_t plen;
    if(!safe_archive_path(archive_path)){fprintf(stderr,"pipiwood: unsafe archive path '%s'\n",archive_path);return 0;}
    if(!read_file(source_path,&data,&size))return 0;
    plen=strlen(archive_path);crc=crc32_update(0UL,data,size);
    put_u2(out,(unsigned long)plen);put_u4(out,size);put_u4(out,crc);
    if(fwrite(archive_path,1U,plen,out)!=plen||(size&&fwrite(data,1U,(size_t)size,out)!=(size_t)size)){free(data);return 0;}
    free(data);return !ferror(out);
}
static int command_pack(const char *list_path,const char *out_path) {
    FILE *list,*out;char line[MAX_LINE],archive_path[1024],source_path[1024],*p,*q;unsigned long count=0;long count_pos;unsigned long total=0;
    list=fopen(list_path,"r");if(!list){fprintf(stderr,"pipiwood: cannot open file list '%s'\n",list_path);return 0;}
    out=fopen(out_path,"wb+");if(!out){fclose(list);fprintf(stderr,"pipiwood: cannot create '%s'\n",out_path);return 0;}
    fwrite(WOOD_MAGIC,1U,8U,out);put_u4(out,0UL);count_pos=ftell(out)-4L;
    while(fgets(line,sizeof(line),list)) {
        p=line;while(isspace((unsigned char)*p))p++;if(!*p||*p=='#')continue;
        q=p;while(*q&&!isspace((unsigned char)*q))q++;if(*q){*q++='\0';while(isspace((unsigned char)*q))q++;}
        if(strlen(p)>=sizeof(archive_path)){fail("archive path too long");break;}strcpy(archive_path,p);
        if(*q){size_t z=strlen(q);while(z&&isspace((unsigned char)q[z-1]))q[--z]='\0';if(strlen(q)>=sizeof(source_path)){fail("source path too long");break;}strcpy(source_path,q);}else strcpy(source_path,p);
        if(!write_entry(out,archive_path,source_path)){fail("failed writing archive entry");break;}
        count++;if(count>MAX_ENTRIES){fail("too many files in package");break;}
        {unsigned char *d;unsigned long sz;if(!read_file(source_path,&d,&sz)){fail("failed to validate source file");break;}total+=sz;free(d);}
        if(total>MAX_TOTAL_SIZE){fail("package exceeds 256 MiB limit");break;}
    }
    fclose(list);
    if(error_text[0]){fclose(out);UNLINK(out_path);fprintf(stderr,"pipiwood: %s\n",error_text);return 0;}
    if(fseek(out,count_pos,SEEK_SET)!=0){fclose(out);return 0;}put_u4(out,count);if(fclose(out)!=0)return 0;
    printf("pipiwood: packed %lu file(s) into %s (%lu bytes)\n",count,out_path,total);return 1;
}
static int read_header(FILE *f,unsigned long *count) { char magic[8];if(fread(magic,1U,8U,f)!=8U||memcmp(magic,WOOD_MAGIC,8U)!=0){fprintf(stderr,"pipiwood: not a WOODLIB1 package\n");return 0;}if(!get_u4(f,count)||*count>MAX_ENTRIES){fprintf(stderr,"pipiwood: invalid package entry count\n");return 0;}return 1; }
static int read_entry_header(FILE *f,char *path,size_t cap,unsigned long *size,unsigned long *crc) { unsigned long plen;if(!get_u2(f,&plen)||!get_u4(f,size)||!get_u4(f,crc)||plen==0UL||plen>=cap||*size>MAX_FILE_SIZE)return 0;if(fread(path,1U,(size_t)plen,f)!=(size_t)plen)return 0;path[plen]='\0';return safe_archive_path(path); }
static int command_verify_or_list(const char *archive,int list_mode) {
    FILE *f;unsigned long count,i,size,crc,got,total=0;char path[MAX_PATH_LEN+1U];unsigned char *data;
    f=fopen(archive,"rb");if(!f){fprintf(stderr,"pipiwood: cannot open '%s'\n",archive);return 0;}if(!read_header(f,&count)){fclose(f);return 0;}
    for(i=0;i<count;i++){if(!read_entry_header(f,path,sizeof(path),&size,&crc)){fclose(f);fprintf(stderr,"pipiwood: invalid or unsafe entry %lu\n",i);return 0;}
        if(size>MAX_TOTAL_SIZE-total){fclose(f);fprintf(stderr,"pipiwood: package exceeds 256 MiB limit\n");return 0;}
        data=(unsigned char *)malloc((size_t)size+1U);if(!data){fclose(f);return 0;}if(size&&fread(data,1U,(size_t)size,f)!=(size_t)size){free(data);fclose(f);fprintf(stderr,"pipiwood: truncated entry '%s'\n",path);return 0;}got=crc32_update(0UL,data,size);free(data);if(got!=crc){fclose(f);fprintf(stderr,"pipiwood: checksum mismatch for '%s'\n",path);return 0;}total+=size;if(list_mode)printf("%s (%lu bytes)\n",path,size);
    }
    if(fgetc(f)!=EOF){fclose(f);fprintf(stderr,"pipiwood: trailing data after last entry\n");return 0;}fclose(f);
    if(list_mode)printf("%lu file(s), %lu bytes\n",count,total);else printf("pipiwood: verified %s (%lu file(s), %lu bytes, CRC32 OK)\n",archive,count,total);return 1;
}
static int make_dirs(const char *path) {
    char copy[2048];size_t i,n;if(strlen(path)>=sizeof(copy))return 0;strcpy(copy,path);n=strlen(copy);
    for(i=1;i<n;i++)if(copy[i]=='/'||copy[i]=='\\'){char saved=copy[i];copy[i]='\0';
#ifdef _WIN32
        if(!(i==2U&&copy[1]==':')&&*copy&&MKDIR(copy)!=0&&errno!=EEXIST){copy[i]=saved;return 0;}
#else
        if(*copy&&MKDIR(copy)!=0&&errno!=EEXIST){copy[i]=saved;return 0;}
#endif
        copy[i]=saved;}
    if(n&&MKDIR(copy)!=0&&errno!=EEXIST)return 0;
    return 1;
}
static int join_path(char *out,size_t cap,const char *root,const char *rel) { size_t n=strlen(root),m=strlen(rel);if(n+m+2U>=cap)return 0;strcpy(out,root);if(n&&out[n-1]!='/'&&out[n-1]!='\\'){out[n]=PATH_SEP;out[n+1]='\0';}strcat(out,rel);
#ifdef _WIN32
 {char *p;for(p=out;*p;p++)if(*p=='/')*p='\\';}
#endif
 return 1; }
static const char *default_root(void) { const char *p=getenv("PIPI_HOME");static char fallback[1024];if(p&&*p)return p;
#ifdef _WIN32
 p=getenv("USERPROFILE");if(!p||!*p)p=getenv("APPDATA");if(p&&*p){sprintf(fallback,"%s\\.pipi",p);return fallback;}
#else
 p=getenv("HOME");if(p&&*p){sprintf(fallback,"%s/.pipi",p);return fallback;}
#endif
 return ".pipi"; }
static int parse_package_manifest(const char *data,char *name,size_t ncap,char *version,size_t vcap) { char copy[2048],*p,*line,*end;if(strlen(data)>=sizeof(copy))return 0;strcpy(copy,data);p=copy;line=p;end=strchr(line,'\n');if(end)*end='\0';if(!eq(line,"WOODLIB 1"))return 0;p=end?end+1:p+strlen(p);name[0]='\0';version[0]='\0';while(*p){line=p;end=strchr(line,'\n');if(end){*end='\0';p=end+1;}else p+=strlen(p);if(strncmp(line,"name ",5)==0&&strlen(line+5)<ncap)strcpy(name,line+5);if(strncmp(line,"version ",8)==0&&strlen(line+8)<vcap)strcpy(version,line+8);}return safe_package_name(name)&&version[0]!='\0'; }
static void marker_name(const char *name,char *out,size_t cap) { size_t i;for(i=0;name[i]&&i+1U<cap;i++)out[i]=name[i]=='/'?'_':name[i];out[i]='\0'; }
static int preflight_package(const char *archive) {
    FILE *f;unsigned long count,i,size,crc;char path[MAX_PATH_LEN+1U],meta[2048],name[256],version[64];int found=0;
    if(!command_verify_or_list(archive,0))return 0;
    f=fopen(archive,"rb");if(!f)return 0;if(!read_header(f,&count)){fclose(f);return 0;}
    for(i=0;i<count;i++) {
        if(!read_entry_header(f,path,sizeof(path),&size,&crc)){fclose(f);return 0;}
        if(eq(path,".woodlib/manifest")) {
            if(size>=sizeof(meta)){fclose(f);fprintf(stderr,"pipiwood: package manifest too large\n");return 0;}
            if(size&&fread(meta,1U,(size_t)size,f)!=(size_t)size){fclose(f);return 0;}
            meta[size]='\0';if(!parse_package_manifest(meta,name,sizeof(name),version,sizeof(version))){fclose(f);fprintf(stderr,"pipiwood: invalid package manifest\n");return 0;}found=1;
        } else if(fseek(f,(long)size,SEEK_CUR)!=0){fclose(f);return 0;}
    }
    fclose(f);if(!found){fprintf(stderr,"pipiwood: archive has no .woodlib/manifest; refusing install\n");return 0;}return 1;
}
static int command_install(const char *archive,const char *root) {
    FILE *f,*record;unsigned long count,i,size,crc,got,total=0;char path[MAX_PATH_LEN+1U],dest[4096],pkg[256],version[64],marker[4096],marker_rel[512],id[256];unsigned char *data;char *contents;char root_copy[2048];
    if(!preflight_package(archive))return 0;
    f=fopen(archive,"rb");if(!f){fprintf(stderr,"pipiwood: cannot open '%s'\n",archive);return 0;}if(!read_header(f,&count)){fclose(f);return 0;}
    if(strlen(root)>=sizeof(root_copy)){fclose(f);return 0;}strcpy(root_copy,root);if(!make_dirs(root_copy)){fclose(f);fprintf(stderr,"pipiwood: cannot create install root '%s'\n",root);return 0;}
    if(!join_path(marker,sizeof(marker),root,".installed")){fclose(f);return 0;}if(!make_dirs(marker)){fclose(f);return 0;}
    pkg[0]='\0';version[0]='\0';
    /* Read all entries, verify CRC before writing each file. */
    for(i=0;i<count;i++) {
        if(!read_entry_header(f,path,sizeof(path),&size,&crc)){fclose(f);fprintf(stderr,"pipiwood: invalid entry in archive\n");return 0;}
        data=(unsigned char *)malloc((size_t)size+1U);if(!data){fclose(f);return 0;}
        if(size&&fread(data,1U,(size_t)size,f)!=(size_t)size){free(data);fclose(f);fprintf(stderr,"pipiwood: truncated entry '%s'\n",path);return 0;}
        got=crc32_update(0UL,data,size);if(got!=crc){free(data);fclose(f);fprintf(stderr,"pipiwood: CRC mismatch for '%s'\n",path);return 0;}
        data[size]='\0';total+=size;
        if(eq(path,".woodlib/manifest")) {
            if(!parse_package_manifest((const char *)data,pkg,sizeof(pkg),version,sizeof(version))){free(data);fclose(f);fprintf(stderr,"pipiwood: invalid .woodlib/manifest\n");return 0;}
            free(data);continue;
        }
        if(!join_path(dest,sizeof(dest),root,path)){free(data);fclose(f);fprintf(stderr,"pipiwood: install path too long\n");return 0;}
        {char parent[4096];char *slash;strncpy(parent,dest,sizeof(parent)-1U);parent[sizeof(parent)-1U]='\0';slash=strrchr(parent,'/');
#ifdef _WIN32
         {char *bs=strrchr(parent,'\\');if(!slash||(bs&&bs>slash))slash=bs;}
#endif
         if(slash){*slash='\0';if(!make_dirs(parent)){free(data);fclose(f);fprintf(stderr,"pipiwood: cannot create directory '%s'\n",parent);return 0;}}}
        contents=(char *)data;{FILE *out=fopen(dest,"wb");if(!out){free(data);fclose(f);fprintf(stderr,"pipiwood: cannot write '%s'\n",dest);return 0;}if(size&&fwrite(contents,1U,(size_t)size,out)!=(size_t)size){fclose(out);free(data);fclose(f);return 0;}if(fclose(out)!=0){free(data);fclose(f);return 0;}}
        free(data);
    }
    if(fgetc(f)!=EOF){fclose(f);fprintf(stderr,"pipiwood: trailing package data\n");return 0;}fclose(f);
    if(!pkg[0]){fprintf(stderr,"pipiwood: archive has no .woodlib/manifest; refusing install\n");return 0;}
    marker_name(pkg,id,sizeof(id));sprintf(marker_rel,".installed/%s.files",id);if(!join_path(marker,sizeof(marker),root,marker_rel))return 0;
    record=fopen(marker,"w");if(!record){fprintf(stderr,"pipiwood: cannot write install record '%s'\n",marker);return 0;}
    f=fopen(archive,"rb");if(!f){fclose(record);return 0;}if(!read_header(f,&count)){fclose(f);fclose(record);return 0;}
    for(i=0;i<count;i++){if(!read_entry_header(f,path,sizeof(path),&size,&crc)){fclose(f);fclose(record);return 0;}if(size&&fseek(f,(long)size,SEEK_CUR)!=0){fclose(f);fclose(record);return 0;}if(!eq(path,".woodlib/manifest"))fprintf(record,"%s\n",path);}
    fclose(f);fclose(record);
    printf("pipiwood: installed %s %s into %s (%lu bytes)\n",pkg,version,root,total);
    printf("pipiwood: PIPI compiler and VM will search this root automatically (or set PIPI_HOME=%s)\n",root);return 1;
}
static int command_remove(const char *name,const char *root) {
    char id[256],rel[512],marker[4096],path[4096],line[MAX_PATH_LEN+16U];FILE *f;int removed=0;
    if(!safe_package_name(name)){fprintf(stderr,"pipiwood: invalid package name\n");return 0;}marker_name(name,id,sizeof(id));sprintf(rel,".installed/%s.files",id);if(!join_path(marker,sizeof(marker),root,rel)){return 0;}f=fopen(marker,"r");if(!f){fprintf(stderr,"pipiwood: package '%s' is not installed under %s\n",name,root);return 0;}
    while(fgets(line,sizeof(line),f)){size_t n=strlen(line);while(n&&isspace((unsigned char)line[n-1]))line[--n]='\0';if(!safe_relpath(line))continue;if(join_path(path,sizeof(path),root,line)&&UNLINK(path)==0)removed++;}
    fclose(f);UNLINK(marker);printf("pipiwood: removed %s (%d file(s)); empty parent directories may remain\n",name,removed);return 1;
}
static void usage(void) {
    puts("PIPIwood 1.0.0 - PIPI library packer / installer\n\n"
         "Commands:\n"
         "  pipiwood verify <file.woodlib>\n"
         "  pipiwood list <file.woodlib>\n"
         "  pipiwood install <file.woodlib> [--root DIR]\n"
         "  pipiwood remove <package/name> [--root DIR]\n"
         "  pipiwood pack <file-list.txt> -o <file.woodlib>\n\n"
         "File list: one archive path per line, or 'archive/path source/file'.\n"
         "Default install root: PIPI_HOME, otherwise %USERPROFILE%/.pipi or $HOME/.pipi.\n"
         "Package paths are relative, checksummed (CRC32), and traversal-safe.");
}
int main(int argc,char **argv) {
    const char *cmd,*root;int i;const char *a1=NULL,*out=NULL;
    if(argc==2&&eq(argv[1],"--version")){puts("PIPIwood 1.0.0 (WOODLIB1)");return 0;}
    if(argc<3){usage();return 2;}cmd=argv[1];a1=argv[2];root=default_root();
    if(eq(cmd,"pack")){for(i=3;i<argc;i++){if(eq(argv[i],"-o")&&i+1<argc)out=argv[++i];else{fprintf(stderr,"unknown option: %s\n",argv[i]);return 2;}}if(!out){fprintf(stderr,"pack requires -o output.woodlib\n");return 2;}return command_pack(a1,out)?0:1;}
    for(i=3;i<argc;i++){if(eq(argv[i],"--root")&&i+1<argc)root=argv[++i];else{fprintf(stderr,"unknown option: %s\n",argv[i]);return 2;}}
    if(eq(cmd,"verify"))return command_verify_or_list(a1,0)?0:1;
    if(eq(cmd,"list"))return command_verify_or_list(a1,1)?0:1;
    if(eq(cmd,"install"))return command_install(a1,root)?0:1;
    if(eq(cmd,"remove"))return command_remove(a1,root)?0:1;
    usage();return 2;
}
