// Exercise the production plugin with only Skyrim/SKSE engine calls mocked.
#include "../addons/completionist/src/Plugin.cpp"
#include <cassert>
#include <iostream>

namespace fixture {
struct Snapshot { TesseraCollectionSummary summary{}; std::vector<TesseraCollectionCard> cards; };
std::vector<TesseraCollectionCard> cards;
bool available = true;
unsigned releases = 0, acquisitions = 0;
std::uint32_t revision() { return 1; }
const void* acquire() {
    if (!available) return nullptr;
    ++acquisitions;
    auto* snapshot = new Snapshot;
    snapshot->summary.size = sizeof(TesseraCollectionSummary);
    snapshot->summary.cardCount = cards.size(); snapshot->cards = cards;
    return snapshot;
}
void release(const void* s) { ++releases; delete static_cast<const Snapshot*>(s); }
const TesseraCollectionSummary* summary(const void* s) { return &static_cast<const Snapshot*>(s)->summary; }
const TesseraCollectionCard* get(const void* s, std::uint32_t index) { return &static_cast<const Snapshot*>(s)->cards.at(index); }
const TesseraCollectionCard* find(const void*, const char*) { return nullptr; }
const TesseraCollectionAPI service{sizeof(TesseraCollectionAPI), 1, revision, acquire, release, summary, get, find};
const TesseraCollectionAPI* exported(std::uint32_t version) { return version == 1 ? &service : nullptr; }
TesseraCollectionCard card(const char* key, unsigned standard, unsigned foil = 0, unsigned flags = 0) {
    TesseraCollectionCard result{}; result.size = sizeof(result); result.key = key;
    result.standardCount = standard; result.foilCount = foil; result.flags = flags; return result;
}
void skse(unsigned type, void* value = reinterpret_cast<void*>(1)) {
    SKSE::MessagingInterface::Message msg{type, 0, value}; onSKSE(&msg); SKSE::GetTaskInterface()->drain();
}
void publish(bool isReady = true) {
    TesseraCollectionChanged change{sizeof(change), 1, 1, isReady ? 1u : 0u};
    SKSE::MessagingInterface::Message message{TESSERA_COLLECTION_CHANGED, sizeof(change), &change};
    onCollection(&message); SKSE::GetTaskInterface()->drain();
}
void ackAll() {
    const auto token = getSession(nullptr);
    int page;
    while ((page = getPendingPage(nullptr)) >= 0) for (auto* form : getPendingForms(nullptr, page, token)) acknowledge(nullptr, form, token);
}
}
int main() {
    using namespace fixture;
    const auto previous = fs::current_path();
    const auto temp = fs::temp_directory_path() / ("tessera-completionist-" + std::to_string(epoch));
    fs::create_directories(temp / "Data/SKSE/Plugins/Tessera/Completionist");
    fs::create_directories(temp / "Data/SKSE/Plugins/CompletionistData/Translations");
    fs::current_path(temp);
    std::ofstream("Data/SKSE/Plugins/CompletionistData/Translations/Cards.txt") << "Cards_Page\t|Cartes Tessera\r\n";
    const nlohmann::json mapping = {{"version", 1}, {"plugin", "Tessera Completionist.esp"}, {"pages", {
        {{"nameKey", "Cards_Page"}, {"translation", "Cards.txt"}, {"number", 1}, {"entries", {
            {{"key", "tessera:one"}, {"form", 0x810}, {"foil", false}},
            {{"key", "tessera:two"}, {"form", 0x812}, {"foil", false}}}}},
        {{"nameKey", "Foils_Page"}, {"translation", "Foils.txt"}, {"number", 1}, {"entries", {
            {{"key", "tessera:one"}, {"form", 0x811}, {"foil", true}},
            {{"key", "tessera:two"}, {"form", 0x813}, {"foil", true}}}}}
    }}};
    std::ofstream("Data/SKSE/Plugins/Tessera/Completionist/tessera.json") << mapping.dump();
    auto* data = RE::TESDataHandler::GetSingleton();
    data->mods = {{"Completionist.esp", 1}, {"Tessera Completionist.esp", 2}};
    for (unsigned local = 0x810; local <= 0x813; ++local) data->add("Tessera Completionist.esp", local);
    testCollectionExport = reinterpret_cast<void*>(exported);
    assert(SKSEPlugin_Load(nullptr));
    assert(SKSE::GetPapyrusInterface()->vm.registered.size() == 6);
    skse(SKSE::MessagingInterface::kPostLoad);
    skse(SKSE::MessagingInterface::kDataLoaded);
    assert(pages.size() == 2 && pages[0].name == "Cartes Tessera");

    // Seen is not collected; unavailable startup cannot mutate any checkmarks.
    cards = {card("tessera:one", 0, 0, TESSERA_CARD_DISCOVERED), card("tessera:two", 1, 1, TESSERA_CARD_PENDING)};
    skse(SKSE::MessagingInterface::kNewGame);
    assert(getSession(nullptr) > 0 && getPendingPage(nullptr) == -1 && history.cards.empty());

    // Acquire, then sell before any Papyrus acknowledgement. The saved journal
    // and queued marker must both retain the acquisition.
    cards = {card("tessera:one", 1)}; publish();
    cards = {card("tessera:one", 0)}; publish();
    const auto token = getSession(nullptr);
    auto forms = getPendingForms(nullptr, 0, token);
    assert(forms.size() == 1 && history.collected("tessera:one", false));
    assert(!history.collected("tessera:one", true));
    assert(getPendingForms(nullptr, 0, token + 1).empty());
    acknowledge(nullptr, forms[0], token + 1); assert(pending.size() == 1);
    ackAll();
    const auto notifications = SKSE::GetModCallbackEventSource()->count;
    publish(); assert(pending.empty() && SKSE::GetModCallbackEventSource()->count == notifications);

    // A foil-only acquisition counts on both lists; another card must not
    // enqueue already acknowledged markers on every collection update.
    cards = {card("tessera:two", 0, 1)}; publish();
    assert(pending.size() == 2 && !pending.contains(forms[0]->GetFormID()));
    assert(history.collected("tessera:two", false) && history.collected("tessera:two", true));
    cards.clear(); publish();
    SKSE::SerializationInterface saved;
    save(&saved); // Save before Completionist has checked the foil entries.
    ackAll();

    // A different/new character must not inherit the previous save's journal.
    skse(SKSE::MessagingInterface::kNewGame);
    assert(history.cards.empty() && pending.empty() && getSession(nullptr) != token);

    // Restore pending acquisitions even though this save no longer owns them.
    skse(SKSE::MessagingInterface::kPreLoadGame);
    load(&saved);
    publish(); assert(!getSession(nullptr)); // Ignore notifications mid-load.
    skse(SKSE::MessagingInterface::kPostLoadGame);
    assert(pending.size() == 3 && history.cards.size() == 2);
    acknowledge(nullptr, forms[0], token); assert(pending.size() == 3);
    ackAll();
    available = false; publish(false);
    assert(!getSession(nullptr) && history.cards.size() == 2);
    available = true; publish(); assert(pending.size() == 3);
    ackAll();

    // Later expansion checklists can pick up history captured before their
    // mapping was installed; temporarily missing cards do not erase it.
    cards = {card("custom-set:one", 1)}; publish();
    assert(history.collected("custom-set:one", false) && pending.empty());
    cards.clear(); publish(); assert(history.collected("custom-set:one", false));

    // Reject malformed history atomically, without merging another character.
    CollectionHistory restored;
    assert(restored.decode(history.encode()));
    const auto valid = restored.encode();
    for (const auto* invalid : {"{", "[]", "{\"bad\":1}", "{\"set:one\":2}", "{\"set:one\":-1}", "{\"set:one\":4294967297}", "{\"set:one\":true}"}) {
        assert(!restored.decode(invalid)); assert(restored.encode() == valid);
    }
    assert(restored.decode("{}") && restored.cards.empty());
    // A full page still returns bounded Papyrus arrays and makes progress.
    skse(SKSE::MessagingInterface::kNewGame);
    Page fullPage{"Batch test", 1, {}};
    std::vector<std::string> keys;
    for (unsigned i = 0; i < 40; ++i) keys.push_back("batch-set:card-" + std::to_string(i));
    for (unsigned i = 0; i < 40; ++i) {
        data->add("Tessera Completionist.esp", 0x900 + i);
        fullPage.entries.push_back({keys[i], data->LookupForm<RE::TESObjectMISC>(0x900 + i, "Tessera Completionist.esp"), false});
        cards.push_back(card(keys[i].c_str(), 1));
    }
    pages.push_back(std::move(fullPage));
    publish();
    const auto batchToken = getSession(nullptr);
    for (unsigned expected : {16u, 16u, 8u}) {
        auto batch = getPendingForms(nullptr, 2, batchToken);
        assert(batch.size() == expected);
        for (auto* marker : batch) acknowledge(nullptr, marker, batchToken);
    }
    assert(pending.empty());
    assert(acquisitions == releases);
    assert(SKSE::log::errors.empty());
    fs::current_path(previous); fs::remove_all(temp);
    std::cout << "Completionist bridge: acquisition/foil history, rapid sale, delta updates, translated pages, save isolation, load recovery, stale acknowledgements, expansion history and corrupt-record handling passed\n";
}
