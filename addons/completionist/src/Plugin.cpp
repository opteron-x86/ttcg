#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Tessera/CollectionAPI.h>
#include "CollectionHistory.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>

namespace {
using namespace tessera::completionist;
namespace log = SKSE::log;
namespace fs = std::filesystem;
constexpr std::uint32_t HistoryRecord = 0x54434853; // TCHS
constexpr std::uint32_t HistoryVersion = 1;
constexpr std::size_t MaxHistoryBytes = 8 * 1024 * 1024;
struct Entry { std::string key; RE::TESForm* marker{}; bool foil{}; };
struct Page { std::string name; int number{}; std::vector<Entry> entries; };
std::vector<Page> pages;
CollectionHistory history;
std::set<RE::FormID> pending;
std::set<RE::FormID> acknowledged;
std::mutex mutex;
const TesseraCollectionAPI* api{};
bool sessionOpen = false, ready = false;
// A fresh process must not accept acknowledgements from a saved Papyrus stack.
std::uint32_t epoch = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()) & 0x3fffffff;
bool eventQueued = false;

void invalidate() {
    ready = false;
    eventQueued = false;
    epoch = (epoch + 1) & 0x7fffffff;
    if (!epoch) epoch = 1;
}
void queueNotification() {
    // Caller owns mutex. Always dispatch later, with no lock held.
    if (eventQueued) return;
    eventQueued = true;
    const auto token = epoch;
    SKSE::GetTaskInterface()->AddTask([token] {
        {
            std::lock_guard lock(mutex);
            if (token != epoch || !ready) return;
            eventQueued = false;
        }
        if (auto* events = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent event{RE::BSFixedString("Tessera_CompletionistChanged"), RE::BSFixedString("ready"), 0, nullptr};
            events->SendEvent(&event);
        }
    });
}
void collect() {
    // Tessera publishes after committed operations. Capture here, rather than
    // waiting for Papyrus: a rapid acquire -> sell still counts as collected.
    if (!api) return;
    std::lock_guard lock(mutex);
    if (!sessionOpen || pages.empty()) return;
    const void* snapshot = api->AcquireSnapshot();
    if (!snapshot) { invalidate(); return; }
    struct Release { const void* value; ~Release() { api->ReleaseSnapshot(value); } } release{snapshot};
    const auto* summary = api->GetSummary(snapshot);
    if (!summary || summary->size < sizeof(TesseraCollectionSummary)) return;
    bool changed = false;
    for (std::uint32_t i = 0; i < summary->cardCount; ++i) {
        const auto* card = api->GetCard(snapshot, i);
        if (card && card->size >= sizeof(TesseraCollectionCard)) changed |= history.observe(*card);
    }
    const bool first = !ready;
    if (first) acknowledged.clear();
    if (changed || first) {
        for (const auto& page : pages) for (const auto& entry : page.entries) {
            if (history.collected(entry.key, entry.foil) && !acknowledged.contains(entry.marker->GetFormID())) pending.insert(entry.marker->GetFormID());
        }
    }
    ready = true;
    if (first || changed) queueNotification();
}
void onCollection(SKSE::MessagingInterface::Message* message) {
    if (message->type != TESSERA_COLLECTION_CHANGED || !message->data || message->dataLen < sizeof(TesseraCollectionChanged)) return;
    const auto change = *static_cast<const TesseraCollectionChanged*>(message->data);
    if (change.size < sizeof(change) || change.apiVersion != TESSERA_COLLECTION_API_VERSION) return;
    if (change.ready) collect();
    else { std::lock_guard lock(mutex); invalidate(); }
}

std::string pageName(const std::string& file, const std::string& key) {
    // Read the same translation entry as Completionist, including translated
    // top-level page names. Do not hard-code an English MCM lookup.
    std::ifstream input(fs::path("Data/SKSE/Plugins/CompletionistData/Translations") / file);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto tab = line.find('\t'), pipe = line.find('|');
        if (tab != std::string::npos && pipe != std::string::npos && line.substr(0, tab) == key) return line.substr(pipe + 1);
    }
    return key;
}
void loadMappings() {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data || !GetModuleHandleW(L"Completionist.dll") || !data->LookupModByName("Completionist.esp") || !data->LookupModByName("Tessera Completionist.esp")) {
        log::warn("Completionist or Tessera Completionist plugin is missing; tracking is inactive");
        return;
    }
    const fs::path folder("Data/SKSE/Plugins/Tessera/Completionist");
    std::vector<fs::path> files;
    std::error_code error;
    for (const auto& item : fs::directory_iterator(folder, error)) if (item.is_regular_file() && item.path().extension() == ".json") files.push_back(item.path());
    std::sort(files.begin(), files.end());
    std::set<RE::FormID> seen;
    for (const auto& file : files) {
        try {
            if (fs::file_size(file) > 4 * 1024 * 1024) throw std::runtime_error("mapping is too large");
            std::ifstream input(file);
            const auto mapping = nlohmann::json::parse(input);
            if (mapping.at("version") != 1) throw std::runtime_error("unsupported mapping version");
            const auto plugin = mapping.at("plugin").get<std::string>();
            if (!data->LookupModByName(plugin)) continue;
            // Reject a broken file as a whole, without registering half a set.
            std::vector<Page> staged;
            auto stagedIDs = seen;
            for (const auto& row : mapping.at("pages")) {
                const auto translation = row.at("translation").get<std::string>();
                if (translation != fs::path(translation).filename().string()) throw std::runtime_error("invalid translation filename");
                Page page{pageName(translation, row.at("nameKey").get<std::string>()), row.at("number").get<int>(), {}};
                if (page.number < 1 || page.number > 127 || row.at("entries").size() > 126) throw std::runtime_error("invalid page size");
                for (const auto& item : row.at("entries")) {
                    auto* marker = data->LookupForm<RE::TESObjectMISC>(item.at("form").get<std::uint32_t>(), plugin);
                    const auto key = item.at("key").get<std::string>();
                    if (!marker || key.empty() || key.size() > 256 || !stagedIDs.insert(marker->GetFormID()).second) throw std::runtime_error("missing or duplicate marker");
                    page.entries.push_back({key, marker, item.at("foil").get<bool>()});
                }
                if (!page.entries.empty()) staged.push_back(std::move(page));
            }
            seen = std::move(stagedIDs);
            for (auto& page : staged) pages.push_back(std::move(page));
        } catch (const std::exception& e) { log::error("Cannot load {}: {}", file.filename().string(), e.what()); }
    }
    std::size_t count = 0;
    for (const auto& page : pages) count += page.entries.size();
    log::info("Loaded {} checklist pages, {} entries", pages.size(), count);
}

int getSession(RE::StaticFunctionTag*) { std::lock_guard lock(mutex); return ready ? static_cast<int>(epoch) : 0; }
int getPendingPage(RE::StaticFunctionTag*) {
    std::lock_guard lock(mutex);
    if (ready) for (std::size_t i = 0; i < pages.size(); ++i) for (const auto& entry : pages[i].entries)
        if (pending.contains(entry.marker->GetFormID())) return static_cast<int>(i);
    return -1;
}
std::string getPageName(RE::StaticFunctionTag*, int page) { return page >= 0 && page < int(pages.size()) ? pages[page].name : ""; }
int getPageNumber(RE::StaticFunctionTag*, int page) { return page >= 0 && page < int(pages.size()) ? pages[page].number : 0; }
std::vector<RE::TESForm*> getPendingForms(RE::StaticFunctionTag*, int page, int session) {
    std::lock_guard lock(mutex);
    std::vector<RE::TESForm*> result;
    if (!ready || std::uint32_t(session) != epoch || page < 0 || page >= int(pages.size())) return result;
    for (const auto& entry : pages[page].entries) if (pending.contains(entry.marker->GetFormID())) {
        result.push_back(entry.marker);
        if (result.size() == 16) break;
    }
    return result;
}
void acknowledge(RE::StaticFunctionTag*, RE::TESForm* marker, int session) {
    std::lock_guard lock(mutex);
    if (marker && ready && std::uint32_t(session) == epoch) {
        if (pending.erase(marker->GetFormID())) acknowledged.insert(marker->GetFormID());
    }
}
void save(SKSE::SerializationInterface* serial) {
    std::lock_guard lock(mutex);
    const auto text = history.encode();
    if (!serial->OpenRecord(HistoryRecord, HistoryVersion) || !serial->WriteRecordData(text.data(), std::uint32_t(text.size()))) log::error("Could not save collection history");
}
void revert(SKSE::SerializationInterface*) {
    std::lock_guard lock(mutex);
    sessionOpen = false;
    invalidate();
    history.cards.clear();
    pending.clear();
    acknowledged.clear();
}
void load(SKSE::SerializationInterface* serial) {
    revert(serial);
    std::lock_guard lock(mutex);
    std::uint32_t type, version, length;
    while (serial->GetNextRecordInfo(type, version, length)) {
        if (type != HistoryRecord) continue;
        if (version != HistoryVersion || length > MaxHistoryBytes) { log::error("Unsupported collection history record"); continue; }
        std::string text(length, '\0');
        if (serial->ReadRecordData(text.data(), length) != length || !history.decode(text)) log::error("Invalid collection history record; existing Completionist checkmarks are retained");
    }
}
void onSKSE(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
    case SKSE::MessagingInterface::kPostLoad: {
        const auto module = GetModuleHandleW(L"TTCG.dll");
        const auto get = module ? reinterpret_cast<TesseraGetCollectionAPI>(GetProcAddress(module, "Tessera_GetCollectionAPI")) : nullptr;
        api = get ? get(TESSERA_COLLECTION_API_VERSION) : nullptr;
        if (!api || api->version != TESSERA_COLLECTION_API_VERSION || api->size < sizeof(TesseraCollectionAPI)) { api = nullptr; log::error("Tessera collection API v1 is unavailable"); }
        else if (!SKSE::GetMessagingInterface()->RegisterListener("TTCG", onCollection)) log::error("Cannot subscribe to Tessera collection changes");
        break;
    }
    case SKSE::MessagingInterface::kDataLoaded:
        try { loadMappings(); }
        catch (const std::exception& e) { log::error("Cannot read Completionist mappings: {}", e.what()); }
        break;
    case SKSE::MessagingInterface::kPreLoadGame: {
        std::lock_guard lock(mutex); sessionOpen = false; invalidate(); break;
    }
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame: {
        if (message->type == SKSE::MessagingInterface::kPostLoadGame && !message->data) break;
        if (message->type == SKSE::MessagingInterface::kNewGame) revert(nullptr);
        { std::lock_guard lock(mutex); sessionOpen = true; }
        // Runs after all plugins' load callbacks, including Completionist's
        // restoration of its per-section completion arrays.
        SKSE::GetTaskInterface()->AddTask([] { collect(); });
        break;
    }
    default: break;
    }
}
}

extern "C" {
__declspec(dllexport) constinit auto SKSEPlugin_Version = []() noexcept {
    SKSE::PluginVersionData data{};
    data.PluginName("TesseraCompletionist"); data.AuthorName("Tessera");
    data.PluginVersion(REL::Version(1, 0, 0, 0)); data.UsesAddressLibrary(); data.UsesNoStructs();
    return data;
}();
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info) {
    info->infoVersion = SKSE::PluginInfo::kVersion; info->name = "TesseraCompletionist"; info->version = SKSEPlugin_Version.pluginVersion;
    return !skse->IsEditor() && (skse->RuntimeVersion() == SKSE::RUNTIME_VR_1_4_15 || skse->RuntimeVersion() >= SKSE::RUNTIME_SSE_1_5_97);
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    log::info("Tessera Completionist 1.0.0");
    SKSE::GetMessagingInterface()->RegisterListener(onSKSE);
    SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("GetSession", "Tessera_CompletionistNative", getSession);
        vm->RegisterFunction("GetPendingPage", "Tessera_CompletionistNative", getPendingPage);
        vm->RegisterFunction("GetPageName", "Tessera_CompletionistNative", getPageName);
        vm->RegisterFunction("GetPageNumber", "Tessera_CompletionistNative", getPageNumber);
        vm->RegisterFunction("GetPendingForms", "Tessera_CompletionistNative", getPendingForms);
        vm->RegisterFunction("Acknowledge", "Tessera_CompletionistNative", acknowledge);
        return true;
    });
    auto* serial = SKSE::GetSerializationInterface();
    serial->SetUniqueID(0x54434350); // TCCP; separate from TTCG and Completionist.
    serial->SetSaveCallback(save); serial->SetLoadCallback(load); serial->SetRevertCallback(revert);
    return true;
}
