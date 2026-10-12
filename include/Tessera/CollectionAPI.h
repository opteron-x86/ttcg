#pragma once
// Tessera collection API v1. Standalone C ABI; no SKSE or C++ library dependency.
// Obtain Tessera_GetCollectionAPI with GetProcAddress(GetModuleHandleW(L"TTCG.dll"), ...).
// See docs/collection-api.md for lifetime, threading and collection semantics.
#include <stdint.h>

#define TESSERA_COLLECTION_API_VERSION 1u
#define TESSERA_COLLECTION_CHANGED 0x54434F4Cu
#define TESSERA_CARD_UNIQUE 1u
#define TESSERA_CARD_DISCOVERED 2u
#define TESSERA_CARD_FOIL_DISCOVERED 4u
#define TESSERA_CARD_PENDING 8u

typedef struct TesseraCollectionCard {
    uint32_t size;
    const char *key, *name, *setID, *setName, *rarity;
    const char *groupsJSON, *affinitiesJSON, *art, *thumbnail;
    uint32_t tier, sides[4], flags, standardCount, foilCount;
} TesseraCollectionCard;

typedef struct TesseraCollectionSummary {
    uint32_t size, revision, cardCount, ownedCards, discoveredCards;
    uint32_t foilOwnedCards, foilDiscoveredCards;
    uint64_t standardCopies, foilCopies;
} TesseraCollectionSummary;

// SKSE message sent by "TTCG", type TESSERA_COLLECTION_CHANGED. Copy the payload
// during the callback. ready == 0 means a load/reset, not an empty collection.
typedef struct TesseraCollectionChanged {
    uint32_t size, apiVersion, revision, ready;
} TesseraCollectionChanged;

typedef struct TesseraCollectionAPI {
    uint32_t size, version;
    uint32_t (*GetRevision)(void);
    // NULL until a valid save/new game has finished recovery. Any thread.
    const void* (*AcquireSnapshot)(void);
    void (*ReleaseSnapshot)(const void* snapshot);
    // All returned pointers/UTF-8 strings belong to the acquired snapshot.
    const TesseraCollectionSummary* (*GetSummary)(const void* snapshot);
    const TesseraCollectionCard* (*GetCard)(const void* snapshot, uint32_t index);
    const TesseraCollectionCard* (*FindCard)(const void* snapshot, const char* key);
} TesseraCollectionAPI;

typedef const TesseraCollectionAPI* (*TesseraGetCollectionAPI)(uint32_t version);
