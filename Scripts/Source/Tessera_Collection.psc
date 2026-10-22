Scriptname Tessera_Collection Hidden
; Read-only collection API. See docs/collection-api.md in the authoring kit.
; Keys: tessera:<built-in id> or <setId>:<cardId>. Never use numeric card handles.
Int Function GetAPIVersion() Global Native
Bool Function IsReady() Global Native
Int Function GetRevision() Global Native
; Arrays are paged at a maximum of 128 entries. Empty setID means all sets.
String[] Function GetCardKeys(Int offset = 0, Int limit = 128, String setID = "") Global Native
Bool Function IsCardLoaded(String key) Global Native
; Pending catalog entries may be omitted from an addon's completion target.
Bool Function IsCardPending(String key) Global Native
String Function GetCardName(String key) Global Native
String Function GetCardSet(String key) Global Native
Int Function GetCardTier(String key) Global Native
String Function GetCardRarity(String key) Global Native
String[] Function GetCardGroups(String key) Global Native
String[] Function GetCardAffinities(String key) Global Native
; Data-relative paths, e.g. TesseraUI/art/cards/Frostbite Spider.webp.
String Function GetCardArt(String key) Global Native
String Function GetCardThumbnail(String key) Global Native
; finish: 0 = both, 1 = standard, 2 = foil. Includes held stakes/owed credits.
Int Function GetOwnedCount(String key, Int finish = 0) Global Native
; Discovery means seen in the album, not necessarily acquired or currently owned.
; Foil discovery also discovers the base card. Lost/sold cards stay discovered.
Bool Function IsDiscovered(String key, Bool foil = False) Global Native
Int Function GetCardTotal(String setID = "") Global Native
Int Function GetOwnedCardTotal(String setID = "", Int finish = 0) Global Native
Int Function GetOwnedCopyTotal(String setID = "", Int finish = 0) Global Native
Int Function GetDiscoveredCardTotal(String setID = "", Bool foil = False) Global Native
; RegisterForModEvent("Tessera_CollectionChanged", "OnTesseraCollectionChanged")
; Event signature: (String eventName, String status, Float revision, Form sender)
; status = "ready" or "unavailable". Re-read this API; events may be coalesced.
; Check IsReady before applying results; a load/reset is not an empty collection.
