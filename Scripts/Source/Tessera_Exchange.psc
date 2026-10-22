Scriptname Tessera_Exchange Hidden

; Collector exchange API v2. Standard copies only; quests own one offer slot.
; Mutations are queued. Register for Tessera_ExchangeChanged before calling.
; Event sender = owning quest; string = result; number = offer ID.
Int Function GetAPIVersion() Global Native
; 0 absent, 1 posted, 2 accepted, 3 completed, 4 cancelled.
Int Function GetStatus(Quest owner) Global Native
Int Function GetOfferID(Quest owner) Global Native
Actor Function GetCollector(Quest owner) Global Native
String Function GetRequestedCard(Quest owner) Global Native
String Function GetRewardCard(Quest owner) Global Native
String Function GetRequestedName(Quest owner) Global Native
String Function GetRewardName(Quest owner) Global Native
Int Function GetGoldReward(Quest owner) Global Native

; rewardCard and goldReward are alternatives. Cards use set:card keys.
; A card reward is reserved from the collector's existing collection.
Function CreateOffer(Quest owner, Actor collector, String requestedCard, String rewardCard = "", Int goldReward = 0) Global Native
; Generate an ordinary local offer. Region IDs: see docs/collection-api.md.
Function PostLocalOffer(Quest owner, Int region) Global Native
Function RefreshOffer(Quest owner) Global Native
; After naming and filling Stores Text reference aliases, rebuild the current
; text record. Sends text-refreshed; keeps existing quest progress and notes.
; Each dynamic name needs its own base form, named with Form.SetName.
Function RefreshText(Quest owner) Global Native
Function AcceptOffer(Quest owner, Int offerID) Global Native
Function CancelOffer(Quest owner, Int offerID) Global Native
; Shows an exact confirmation, including saved-deck warnings. Delivery requires
; the named collector nearby. Completion is saved and cannot pay twice.
Function CompleteOffer(Quest owner, Int offerID, Actor collector) Global Native
