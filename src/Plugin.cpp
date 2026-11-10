// SKSE entry points, Papyrus registration and save callbacks.
#include "PluginState.h"

// The functions below used to share an anonymous namespace with the rest.
using namespace tessera_plugin;

extern "C" __declspec(dllexport) const TesseraCollectionAPI* Tessera_GetCollectionAPI(std::uint32_t version) noexcept {
    return version == TESSERA_COLLECTION_API_VERSION ? &ttcg::collectionAPI::api : nullptr;
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = []() noexcept {
    SKSE::PluginVersionData data{};
    data.PluginName("TTCG");
    data.AuthorName("TTCG");
    data.PluginVersion(REL::Version(TTCG_VERSION_MAJOR, TTCG_VERSION_MINOR, TTCG_VERSION_PATCH, 0));
    // CommonLib selects engine layouts at runtime.
    data.UsesAddressLibrary();
    data.UsesNoStructs();
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info) {
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = "TTCG";
    info->version = SKSEPlugin_Version.pluginVersion;
    return !skse->IsEditor() &&
           (skse->RuntimeVersion() == SKSE::RUNTIME_VR_1_4_15 || skse->RuntimeVersion() >= SKSE::RUNTIME_SSE_1_5_97);
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    const bool diagnostics = GetPrivateProfileIntW(L"Diagnostics", L"Enabled", 0, settingsPath.c_str()) != 0;
    SKSE::InitInfo initInfo;
    // CommonLib creates the log and flushes at this level, preserving the last
    // match/recovery message if Skyrim exits unexpectedly.
    initInfo.logLevel = diagnostics ? spdlog::level::debug : spdlog::level::info;
    SKSE::Init(skse, initInfo);
    log::info("Diagnostics: {}", diagnostics ? "enabled" : "disabled");
    SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        exchangeIntegration::bind(vm);
        vm->RegisterFunction("GetAPIVersion", "Tessera_Collection", collectionIntegration::apiVersion);
        vm->RegisterFunction("IsReady", "Tessera_Collection", collectionIntegration::isReady);
        vm->RegisterFunction("GetRevision", "Tessera_Collection", collectionIntegration::revision);
        vm->RegisterFunction("GetCardKeys", "Tessera_Collection", collectionIntegration::cardKeys);
        vm->RegisterFunction("IsCardLoaded", "Tessera_Collection", collectionIntegration::cardLoaded);
        vm->RegisterFunction("IsCardPending", "Tessera_Collection", collectionIntegration::cardPending);
        vm->RegisterFunction("GetCardName", "Tessera_Collection",
                             collectionIntegration::text<&TesseraCollectionCard::name>);
        vm->RegisterFunction("GetCardSet", "Tessera_Collection",
                             collectionIntegration::text<&TesseraCollectionCard::setID>);
        vm->RegisterFunction("GetCardTier", "Tessera_Collection", collectionIntegration::tier);
        vm->RegisterFunction("GetCardRarity", "Tessera_Collection",
                             collectionIntegration::text<&TesseraCollectionCard::rarity>);
        vm->RegisterFunction("GetCardGroups", "Tessera_Collection",
                             collectionIntegration::strings<&TesseraCollectionCard::groupsJSON>);
        vm->RegisterFunction("GetCardAffinities", "Tessera_Collection",
                             collectionIntegration::strings<&TesseraCollectionCard::affinitiesJSON>);
        vm->RegisterFunction("GetCardArt", "Tessera_Collection",
                             collectionIntegration::text<&TesseraCollectionCard::art>);
        vm->RegisterFunction("GetCardThumbnail", "Tessera_Collection",
                             collectionIntegration::text<&TesseraCollectionCard::thumbnail>);
        vm->RegisterFunction("GetOwnedCount", "Tessera_Collection", collectionIntegration::ownedCount);
        vm->RegisterFunction("IsDiscovered", "Tessera_Collection", collectionIntegration::discovered);
        vm->RegisterFunction("GetCardTotal", "Tessera_Collection", collectionIntegration::cardTotal);
        vm->RegisterFunction("GetOwnedCardTotal", "Tessera_Collection",
                             collectionIntegration::total<collectionIntegration::Total::Owned>);
        vm->RegisterFunction("GetOwnedCopyTotal", "Tessera_Collection",
                             collectionIntegration::total<collectionIntegration::Total::Copies>);
        vm->RegisterFunction("GetDiscoveredCardTotal", "Tessera_Collection", collectionIntegration::discoveredTotal);
        vm->RegisterFunction("IsCardLoaded", "TTCG_CardExpansion", expansionLoadedPapyrus);
        vm->RegisterFunction("GiveCard", "TTCG_CardExpansion", expansionGivePapyrus);
        vm->RegisterFunction("GetAvailability", "TTCG_Opponents", npcAvailabilityPapyrus);
        vm->RegisterFunction("IsRegistered", "TTCG_Opponents", npcRegisteredPapyrus);
        vm->RegisterFunction("AddPlayer", "TTCG_Opponents", npcAddPapyrus);
        vm->RegisterFunction("SetEnabled", "TTCG_Opponents", npcEnabledPapyrus);
        vm->RegisterFunction("Refresh", "TTCG_Opponents", npcRefreshPapyrus);
        vm->RegisterFunction("GiveCard", "TTCG_Opponents", npcGivePapyrus);
        vm->RegisterFunction("HasReceivedGrant", "TTCG_Opponents", npcGrantedPapyrus);
        vm->RegisterFunction("BeginMatch", "TTCG_Native", beginPapyrus);
        vm->RegisterFunction("BackgroundUpdate", "TTCG_Native", backgroundUpdatePapyrus);
        vm->RegisterFunction("NextMaster", "TTCG_Native", legacyPapyrus::nextMaster);
        vm->RegisterFunction("MasterArrived", "TTCG_Native", legacyPapyrus::masterArrived);
        vm->RegisterFunction("MasterResponse", "TTCG_Native", legacyPapyrus::masterResponse);
        vm->RegisterFunction("OpenTournaments", "TTCG_Native", tournamentPapyrus);
        vm->RegisterFunction("RegisterTournament", "TTCG_Native", tournamentEntryPapyrus);
        vm->RegisterFunction("NextTournamentInvitation", "TTCG_Native", nextTournamentInvitationPapyrus);
        vm->RegisterFunction("TournamentInvitation", "TTCG_Native", tournamentInvitationPapyrus);
        vm->RegisterFunction("OpenCardShop", "TTCG_Native", shopPapyrus);
        vm->RegisterFunction("BuyAlbum", "TTCG_Native", buyAlbumPapyrus);
        vm->RegisterFunction("OpenAlbum", "TTCG_Native", albumPapyrus);
        return true;
    });
    auto serial = SKSE::GetSerializationInterface();
    serial->SetUniqueID(0x54544347);
    serial->SetSaveCallback([](SKSE::SerializationInterface* serial) {
        checkpointMatch();
        campaign::save(serial);
    });
    serial->SetLoadCallback([](SKSE::SerializationInterface* serial) {
        collectionIntegration::reset();
        campaign::load(serial);
    });
    serial->SetRevertCallback([](SKSE::SerializationInterface* serial) {
        collectionIntegration::reset();
        campaign::revert(serial);
    });
    SKSE::GetMessagingInterface()->RegisterListener(message);
    log::info("Tessera {} loaded ({} / CommonLibSSE-NG {} / {} / Skyrim {})", TTCG_VERSION, ttcg::ui::name,
              TTCG_COMMONLIB_VERSION, TTCG_COMMONLIB_COMMIT, REL::Module::get().version().string());
    log::info("CommonLib container fix: {}", TTCG_COMMONLIB_HASHMAP_PATCH);
    if constexpr (sizeof(TTCG_BUILD_LABEL) > 1) log::info("Tessera test build: {}", TTCG_BUILD_LABEL);
    return true;
}
