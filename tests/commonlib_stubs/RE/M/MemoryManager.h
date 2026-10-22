#pragma once
// Offline engine boundary for the real CommonLib container regression.
#define TES_HEAP_REDEFINE_NEW()
namespace RE {
using std::malloc;
using std::free;
struct ScrapHeap {
    void* Allocate(std::size_t size,std::size_t) { return std::malloc(size); }
    void Deallocate(void* memory) { std::free(memory); }
};
struct MemoryManager {
    static MemoryManager* GetSingleton() { static MemoryManager manager;return &manager; }
    ScrapHeap* GetThreadScrapHeap() { static ScrapHeap heap;return &heap; }
};
}
