#pragma once
#include <RE/Skyrim.h>
#include <cstring>
#include <deque>
#include <functional>
#include <string_view>
namespace REL {
struct Version {
    unsigned value;
    constexpr Version(unsigned a, unsigned b, unsigned c, unsigned d = 0) : value(a << 24 | b << 16 | c << 4 | d) {}
    constexpr auto operator<=>(const Version&) const = default;
};
}
namespace SKSE {
namespace log {
inline std::vector<std::string> errors;
template<class... Args> void info(const char*, Args&&...) {}
template<class... Args> void warn(const char*, Args&&...) {}
template<class... Args> void error(const char* msg, Args&&...) { errors.emplace_back(msg); }
}
struct PluginVersionData {
    unsigned pluginVersion{};
    constexpr void PluginName(const char*) {}
    constexpr void AuthorName(const char*) {}
    constexpr void PluginVersion(REL::Version version) { pluginVersion = version.value; }
    constexpr void UsesAddressLibrary() {}
    constexpr void UsesNoStructs() {}
};
constexpr REL::Version RUNTIME_VR_1_4_15{1, 4, 15}, RUNTIME_SSE_1_5_97{1, 5, 97};
struct QueryInterface { bool IsEditor() const { return false; } REL::Version RuntimeVersion() const { return {1, 6, 1170}; } };
struct LoadInterface : QueryInterface {};
struct PluginInfo { static constexpr unsigned kVersion = 1; unsigned infoVersion, version; const char* name; };
inline void Init(const LoadInterface*) {}
struct TaskInterface {
    std::deque<std::function<void()>> tasks;
    void AddTask(std::function<void()> task) { tasks.push_back(std::move(task)); }
    void drain() { while (!tasks.empty()) { auto task = tasks.front(); tasks.pop_front(); task(); } }
};
inline TaskInterface* GetTaskInterface() { static TaskInterface result; return &result; }
struct ModCallbackEvent { RE::BSFixedString eventName, strArg; float numArg; RE::TESForm* sender; };
struct ModCallbacks { unsigned count = 0; void SendEvent(ModCallbackEvent*) { ++count; } };
inline ModCallbacks* GetModCallbackEventSource() { static ModCallbacks result; return &result; }
struct MessagingInterface {
    enum { kPostLoad, kDataLoaded, kPreLoadGame, kPostLoadGame, kNewGame };
    struct Message { std::uint32_t type, dataLen; void* data; };
    using Callback = void(*)(Message*);
    bool RegisterListener(Callback) { return true; }
    bool RegisterListener(const char*, Callback) { return true; }
};
inline MessagingInterface* GetMessagingInterface() { static MessagingInterface result; return &result; }
struct PapyrusInterface {
    RE::BSScript::IVirtualMachine vm;
    void Register(bool(*callback)(RE::BSScript::IVirtualMachine*)) { callback(&vm); }
};
inline PapyrusInterface* GetPapyrusInterface() { static PapyrusInterface result; return &result; }
struct SerializationInterface {
    struct Record { std::uint32_t type, version; std::string data; };
    std::vector<Record> records;
    unsigned readIndex = 0;
    void SetUniqueID(unsigned) {}
    using Callback = void(*)(SerializationInterface*);
    void SetSaveCallback(Callback) {} void SetLoadCallback(Callback) {} void SetRevertCallback(Callback) {}
    bool OpenRecord(unsigned type, unsigned version) { records.push_back({type, version, {}}); return true; }
    bool WriteRecordData(const void* data, unsigned length) { records.back().data.assign(static_cast<const char*>(data), length); return true; }
    bool GetNextRecordInfo(unsigned& type, unsigned& version, unsigned& length) {
        if (readIndex == records.size()) return false;
        auto& record = records[readIndex++]; type = record.type; version = record.version; length = record.data.size(); return true;
    }
    unsigned ReadRecordData(void* target, unsigned length) { std::memcpy(target, records[readIndex - 1].data.data(), length); return length; }
};
inline SerializationInterface* GetSerializationInterface() { static SerializationInterface result; return &result; }
}
