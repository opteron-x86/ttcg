#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint32_t skseVersion, runtimeVersion, editorVersion, isEditor; } Query;
typedef struct { uint32_t infoVersion; const char* name; uint32_t version; } Info;
typedef unsigned char (*QueryFunction)(const Query*, Info*);
#define VERSION(a,b,c) ((a << 24) | (b << 16) | (c << 4))
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) { fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 3; }
    QueryFunction query = (QueryFunction)(void*)GetProcAddress(module, "SKSEPlugin_Query");
    const uint32_t* version = (const uint32_t*)(void*)GetProcAddress(module, "SKSEPlugin_Version");
    if (!query || !version || !GetProcAddress(module, "SKSEPlugin_Load")) return 4;
    const uint32_t runtimes[] = {VERSION(1,5,97), VERSION(1,6,640), VERSION(1,6,659), VERSION(1,6,1170), VERSION(1,6,1179), VERSION(1,4,15)};
    for (unsigned i = 0; i < sizeof(runtimes)/sizeof(runtimes[0]); ++i) {
        Query input = {0,runtimes[i],0,0}; Info info = {0};
        if (!query(&input, &info) || info.infoVersion != 1 || strcmp(info.name, "TesseraCompletionist") || info.version != version[1]) return 5;
    }
    Query editor = {0,VERSION(1,6,1170),0,1}; Info info = {0};
    if (query(&editor,&info)) return 6;
    puts("Completionist DLL: Windows loader and SE/AE/VR query checks passed. Engine calls require Skyrim.");
    FreeLibrary(module);
    return 0;
}
