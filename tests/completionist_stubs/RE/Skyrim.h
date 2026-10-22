#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#define __declspec(...)
namespace RE {
using FormID = std::uint32_t;
struct TESForm { FormID id{}; FormID GetFormID() const { return id; } };
struct TESObjectMISC : TESForm {};
struct StaticFunctionTag {};
struct BSFixedString { std::string value; BSFixedString(const char* v) : value(v) {} };
struct TESDataHandler {
    std::map<std::string, unsigned> mods;
    std::map<std::pair<std::string, FormID>, TESObjectMISC> forms;
    static TESDataHandler* GetSingleton() { static TESDataHandler data; return &data; }
    bool LookupModByName(const std::string& name) { return mods.contains(name); }
    template<class T> T* LookupForm(FormID local, const std::string& plugin) {
        auto it = forms.find({plugin, local});
        return it == forms.end() ? nullptr : &it->second;
    }
    void add(const std::string& plugin, FormID local) { forms[{plugin, local}].id = (mods.at(plugin) << 24) | local; }
};
namespace BSScript {
struct IVirtualMachine {
    std::vector<std::string> registered;
    template<class Function> void RegisterFunction(const char* name, const char*, Function) { registered.push_back(name); }
};
}
}
inline void* testCollectionExport = nullptr;
inline void* GetModuleHandleW(const wchar_t*) { return reinterpret_cast<void*>(1); }
inline void* GetProcAddress(void*, const char*) { return testCollectionExport; }
