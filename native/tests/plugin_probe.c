/* Load the real preview DLL and check the SKSE query ABI, without game calls. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint32_t skseVersion,runtimeVersion,editorVersion,isEditor; } Query;
typedef struct { uint32_t infoVersion; const char* name; uint32_t version; } Info;
typedef unsigned char (*QueryFn)(const Query*,Info*);
#define VERSION(a,b,c) ((a<<24)|(b<<16)|(c<<4))
int main(int argc,char** argv) {
    if(argc!=2&&argc!=3)return 2;
    const int backend=argc==3&&!strcmp(argv[2],"TTCG");
    HMODULE dll=LoadLibraryA(argv[1]);
    if(!dll) { fprintf(stderr,"LoadLibrary failed: %lu\n",GetLastError());return 3; }
    QueryFn query=(QueryFn)(void*)GetProcAddress(dll,"SKSEPlugin_Query");
    const uint32_t* metadata=(const uint32_t*)(void*)GetProcAddress(dll,"SKSEPlugin_Version");
    if(!query||!metadata||!GetProcAddress(dll,"SKSEPlugin_Load"))return 4;
    const uint32_t versions[]={VERSION(1,5,97),VERSION(1,6,640),VERSION(1,6,1170),VERSION(1,6,1179)};
    for(unsigned i=0;i<sizeof(versions)/sizeof(versions[0]);++i) {
        Query input={0,versions[i],0,0};Info output={0};
        if(!query(&input,&output)||output.infoVersion!=1||strcmp(output.name,backend?"TTCG":"TesseraNativePreview")||output.version!=metadata[1])return 5;
    }
    Query vr={0,VERSION(1,4,15),0,0};Info output={0};
    if(!backend&&query(&vr,&output))return 6;
    Query editor={0,VERSION(1,6,1170),0,1};if(query(&editor,&output))return 7;
    Query old={0,VERSION(1,5,80),0,0};if(query(&old,&output))return 8;
    if(backend){
        typedef void (*Reply)(void*,uint64_t,const char*,uint32_t);
        typedef struct {uint32_t size,version;uint64_t (*open)(Reply,void*);unsigned char (*submit)(uint64_t,const char*,uint32_t);void (*close)(uint64_t);} API;
        typedef const API* (*Acquire)(uint32_t);
        Acquire acquire=(Acquire)(void*)GetProcAddress(dll,"TesseraNativeUIAPI");
        if(!acquire||acquire(0)||acquire(2))return 9;
        const API* api=acquire(1);
        if(!api||api->size!=sizeof(API)||api->version!=1||!api->open||!api->submit||!api->close)return 10;
        if(api->open(NULL,NULL)||api->submit(0,"{}",2)||api->submit(999,"{}",2))return 11;
        api->close(999);
        puts("PASS: backend bridge ABI; incompatible versions and invalid sessions rejected. Game callbacks not executed.");
    }
    FreeLibrary(dll);
    puts("PASS: DLL loads; SE/AE query metadata accepted; editor/old runtime rejected. Engine callbacks not executed.");
    return 0;
}
