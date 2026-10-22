/* Exercise the real Windows DLL and SKSE query ABI without starting Skyrim. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/Tessera/CollectionAPI.h"

typedef struct { uint32_t skseVersion, runtimeVersion, editorVersion, isEditor; } Query;
typedef struct { uint32_t infoVersion; const char* name; uint32_t version; } Info;
typedef unsigned char (*QueryFunction)(const Query*, Info*);
#define VERSION(major, minor, patch) ((major << 24) | (minor << 16) | (patch << 4))

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) { fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 3; }
    QueryFunction query = (QueryFunction)(void*)GetProcAddress(module, "SKSEPlugin_Query");
    if (!query || !GetProcAddress(module, "SKSEPlugin_Load")) return 4;
    const uint32_t* metadata = (const uint32_t*)(void*)GetProcAddress(module, "SKSEPlugin_Version");
    if (!metadata) return 4;
    TesseraGetCollectionAPI collection = (TesseraGetCollectionAPI)(void*)GetProcAddress(module, "Tessera_GetCollectionAPI");
    if (!collection || collection(0) || collection(2)) return 8;
    const TesseraCollectionAPI* api = collection(1);
    if (!api || api->size != sizeof(*api) || api->version != 1 || api->GetRevision() || api->AcquireSnapshot()) return 9;
    if (api->GetSummary(NULL) || api->GetCard(NULL, 0) || api->FindCard(NULL, "tessera:murkwater-goblin")) return 10;
    api->ReleaseSnapshot(NULL);
    const uint32_t versions[] = {
        VERSION(1,5,97), VERSION(1,6,353), VERSION(1,6,640), VERSION(1,6,659),
        VERSION(1,6,1170), VERSION(1,6,1179), VERSION(1,7,99), VERSION(1,7,104)
    };
    for (unsigned i=0; i<sizeof(versions)/sizeof(versions[0]); ++i) {
        Query input = {0,versions[i],0,0}; Info info = {0};
        if (!query(&input,&info) || info.infoVersion != 1 || strcmp(info.name,"TTCG") || info.version != metadata[1]) return 5;
    }
    Query editor = {0,VERSION(1,6,1170),0,1}; Info info = {0};
    if (query(&editor,&info)) return 6;
    Query vr = {0,VERSION(1,4,15),0,0};
    if (!query(&vr,&info)) return 7;
    puts("Windows loader: DLL loaded; collection API v1 C ABI and unavailable state passed; eight desktop query versions accepted; VR accepted; editor rejected. Game engine calls were not exercised.");
    FreeLibrary(module);
    return 0;
}
