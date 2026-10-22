# Collection API v1

Read the player's collection from Papyrus or a native SKSE plugin, including cards from installed expansions.

## Identity and counts

- Use `tessera:<id>` for built-in cards and `<setId>:<cardId>` for expansions, as in opponent integrations. Obtain keys from the API; never persist catalog positions, display names, art filenames or numeric handles as identities.
- Standard and foil are finishes of one card key. `finish`: **0 both**, **1 standard**, **2 foil**. Card totals count distinct definitions; copy totals include duplicates.
- Ownership includes the player's reserved match stakes and committed, undelivered credits. The opponent's hand and unchosen prizes are excluded. Once a trade is settled, losses and gains are reflected normally.
- **Discovered means seen**, matching the album. It does not mean ever owned. Foil discovery also discovers the base card; selling or losing a card does not erase discovery. An addon needing an “ever acquired” checklist must keep its own history from ownership changes; past acquisitions cannot be reconstructed reliably.
- Totals include the album's full loaded catalog, including special rewards and pending entries. Use `IsCardPending` / `TESSERA_CARD_PENDING` to exclude unreleased entries from your addon's completion target without hiding existing ownership. Removed expansion definitions and saved orphan identities are excluded until reinstalled. Unknown keys return empty/zero/false. An unknown set filter returns no results. No collection state is changed by querying.
- Art and thumbnail paths are UTF-8 and relative to Data, beginning with `TesseraUI/art/`. These describe the registered asset; normal replacers can override it. Sides are **top, right, bottom, left**.

## Papyrus

Compile against [Tessera_Collection.psc](../Scripts/Source/Tessera_Collection.psc); the compiled script ships in Main. The API requires Tessera and SKSE.

```papyrus
If Tessera_Collection.IsReady()
    Int owned = Tessera_Collection.GetOwnedCount("tessera:murkwater-goblin")
    Int foils = Tessera_Collection.GetOwnedCount("my-set:my-card", 2)
    Int total = Tessera_Collection.GetCardTotal("my-set")
EndIf
```

| Functions | Result |
|---|---|
| `GetAPIVersion`, `IsReady`, `GetRevision` | API availability and current snapshot revision |
| `GetCardKeys(offset, limit, setID)` | Keys in pages of at most 128; empty set ID means all sets |
| `IsCardLoaded(key)`, `IsCardPending(key)` | Definition is present / marked pending in the current snapshot |
| `GetCardName`, `GetCardSet`, `GetCardTier`, `GetCardRarity` | Display metadata (`GetCardSet` returns the set ID) |
| `GetCardGroups`, `GetCardAffinities` | String arrays; an empty affinity array means Neutral |
| `GetCardArt`, `GetCardThumbnail` | Data-relative asset paths |
| `GetOwnedCount(key, finish)` | Copies of one card |
| `IsDiscovered(key, foil)` | Base-card or foil discovery |
| `GetCardTotal(setID)` | Loaded album card definitions |
| `GetOwnedCardTotal(setID, finish)` | Distinct owned definitions |
| `GetOwnedCopyTotal(setID, finish)` | Owned copies (clamped to Papyrus's maximum signed integer) |
| `GetDiscoveredCardTotal(setID, foil)` | Discovered definitions |

Negative offsets and nonpositive page limits return empty arrays; larger limits clamp to 128. Invalid finishes return zero. Keys are case-sensitive. Page order is stable during one game process, not across different expansion installations.

Register on initialization **and** `OnPlayerLoadGame`:

```papyrus
RegisterForModEvent("Tessera_CollectionChanged", "OnTesseraCollectionChanged")

Event OnTesseraCollectionChanged(String eventName, String status, Float revision, Form sender)
    If status == "ready" && Tessera_Collection.IsReady()
        ; Refresh your display or schedule a single update to combine notifications.
    EndIf
EndEvent
```

Notifications arrive after completed collection operations and after save/new-game recovery, even with the album closed. `status` is `ready` or `unavailable`; sender is `None`. **Unavailable is a load/reset, not an empty collection:** wait before clearing any completion flags. Register first, then query immediately so subscribing after the ready event also works. The float revision is a convenience; use `GetRevision()` for exact comparison.

Papyrus calls individually read the latest snapshot. For multiple calls/pages, record `GetRevision()` before and after, and apply the results only if the revision is unchanged and `IsReady()` is still true. Retry once on a later update when it changes. Revisions are process-local change tokens, not save data. See the [player-alias example](../examples/collection-api/TesseraCollectionExample.psc).

## Native SKSE plugins

Include [Tessera/CollectionAPI.h](../include/Tessera/CollectionAPI.h). At SKSE PostLoad or later, use `GetModuleHandleW(L"TTCG.dll")`, then `GetProcAddress(module, "Tessera_GetCollectionAPI")` and request version **1**. Do not call `LoadLibrary` to load Tessera yourself. An absent export or unsupported version means this integration is unavailable.

`AcquireSnapshot()` returns an opaque handle, or null before a valid save is ready. `GetSummary`, `GetCard(index)` and `FindCard(key)` return const pointers owned by that handle. Release it exactly once with `ReleaseSnapshot`; never free its pointers or pass a stale/foreign handle. Null handles and invalid indices/keys return null. All getters are thread-safe, call no engine functions and may be used while the album is closed. No STL objects or game objects cross the DLL boundary.

A held snapshot stays consistent and valid through subsequent updates and save loads until released. Reacquire for fresh data. UTF-8 string pointers share the snapshot's lifetime. The summary uses 64-bit copy totals. Card flags identify unique, pending, discovered and foil-discovered entries; standard/foil copy counts are separate.

For notifications, register an SKSE listener for sender **`TTCG`**. Message type `TESSERA_COLLECTION_CHANGED` carries `TesseraCollectionChanged`; verify `dataLen` and `apiVersion`, and copy it before returning. `ready == 0` invalidates the current collection. Notifications run on the publisher's thread, without API locks held; enqueue your own work if needed. Do not block that callback. See the [native example](../examples/collection-api/CollectionExample.cpp).

## Completionist

The optional [Tessera Completionist addon](../addons/completionist/README.md) adds card and foil checklists grouped by tier. It records ownership through this API and keeps cards checked after they are sold, traded or lost. Discovery alone does not count. Existing saves import their current collection.

The [checklist builder](../tools/build-completionist-addon.py) also supports custom expansions.

## Collector trades

Use the [exchange API](exchange-api.md) for quests that trade a player's card for gold or an NPC's card. It handles confirmation, reserved rewards, cancellation and save/load recovery.
