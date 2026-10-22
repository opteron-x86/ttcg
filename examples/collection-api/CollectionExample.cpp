// Copy values while holding one immutable snapshot. Call from an SKSE plugin
// after PostLoad, and again after Tessera collection notifications.
#include <windows.h>
#include <Tessera/CollectionAPI.h>
#include <optional>
#include <string>
#include <vector>

struct CollectedCard { std::string key,name; unsigned standard,foil; bool discovered; };
std::optional<std::vector<CollectedCard>> readTesseraCollection() {
    const auto module=GetModuleHandleW(L"TTCG.dll");
    const auto get=module?reinterpret_cast<TesseraGetCollectionAPI>(GetProcAddress(module,"Tessera_GetCollectionAPI")):nullptr;
    const auto* api=get?get(TESSERA_COLLECTION_API_VERSION):nullptr;
    if(!api||api->version!=1||api->size<sizeof(TesseraCollectionAPI))return std::nullopt;
    const auto snapshot=api->AcquireSnapshot();if(!snapshot)return std::nullopt;
    struct Release {const TesseraCollectionAPI* api;const void* snapshot;~Release(){api->ReleaseSnapshot(snapshot);}} release{api,snapshot};
    std::vector<CollectedCard> result;
    const auto* summary=api->GetSummary(snapshot);result.reserve(summary->cardCount);
    for(uint32_t i=0;i<summary->cardCount;++i) {
        const auto* card=api->GetCard(snapshot,i);
        result.push_back({card->key,card->name,card->standardCount,card->foilCount,bool(card->flags&TESSERA_CARD_DISCOVERED)});
    }
    return result;
}
