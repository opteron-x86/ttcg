# Collector exchanges

Exchange API version 2 is available through `Tessera_Exchange.psc`.

Use this API when an NPC offers gold or one of their cards for a player's card. Each running quest owns one offer. Tessera saves the terms, sets aside the reward card, checks the delivery and prevents a completed offer from paying twice.

## Create an offer

Register for the result event first, then create the offer:

```papyrus
RegisterForModEvent("Tessera_ExchangeChanged", "OnExchangeChanged")
Tessera_Exchange.CreateOffer(Self, collector, "my-set:forest-scout", "", 150)
```

That requests one standard Forest Scout for 150 gold. For a card trade, pass its key in the fourth argument and leave gold at zero:

```papyrus
Tessera_Exchange.CreateOffer(Self, collector, "my-set:forest-scout", "my-set:river-guard")
```

The collector must be a registered, eligible Tessera opponent and own the reward card. Tessera removes that reward copy from their playable collection while the offer is active. Cancellation returns it. Gold is a quest reward; it does not come from the NPC's betting purse.

Both sides use standard copies. Any available non-unique card can be exchanged, at any tier or rarity. Expansion cards do not need a pack or merchant acquisition route; a request still requires the player to obtain the card normally, and a card reward must already belong to the collector. Unique, unavailable and unreleased cards are excluded. An offer trades one card for one card, or one card for gold, up to 100,000 gold per authored offer.

For a generated notice, use `PostLocalOffer(Self, region)` instead. It chooses a local opponent, an eligible card and a reward. All eligible cards can be requested regardless of the collector's skill; regional cards and cards missing from their collection receive extra weight. Regions 1–9 are Whiterun, Falkreath, Haafingar, Hjaalmarch, The Pale, The Rift, Winterhold, Eastmarch and The Reach. Generated gold rewards are capped at 2,500.

## Accept, deliver or cancel

Calls that change an offer run on Skyrim's game thread and report their result through an event. Do not assume a call has succeeded just because it returned.

```papyrus
Event OnExchangeChanged(String eventName, String result, Float number, Form sender)
    If sender != Self
        Return
    EndIf
    If result == "posted"
        offerID = Tessera_Exchange.GetOfferID(Self)
        ; Display the terms before letting the player accept.
    ElseIf result == "completed"
        SetStage(100)
    EndIf
EndEvent
```

Keep the exact integer from `GetOfferID`; the event's float is informational. Pass that ID with each action so a delayed action cannot affect a newer offer:

```papyrus
Tessera_Exchange.AcceptOffer(Self, offerID)
Tessera_Exchange.CompleteOffer(Self, offerID, collector)
Tessera_Exchange.CancelOffer(Self, offerID)
```

`CompleteOffer` opens Skyrim's confirmation box with the exact card and reward. It warns about saved decks and the last playable hand. The player must be near the named collector. Tessera checks the terms again after confirmation, transfers the cards, records completion and delivers the gold. Finish your quest only after the `completed` result. Repeating completion on that same offer cannot give another reward.

Possible results: `posted`, `refreshed`, `text-refreshed`, `accepted`, `completed`, `cancelled`, `declined`, `missing-card`, `busy`, `changed`, `stale`, `unavailable`. A declined confirmation leaves the offer accepted. `changed` means the player should confirm again; `busy` means another game or payment must finish first. `stale` means the supplied offer ID or state no longer permits the action.

## Read the terms and restore after loading

| Function | Returns |
|---|---|
| `GetStatus(owner)` | 0 absent, 1 posted, 2 accepted, 3 completed, 4 cancelled |
| `GetOfferID(owner)` | Saved offer number for that quest |
| `GetCollector(owner)` | Named collector |
| `GetRequestedCard(owner)`, `GetRewardCard(owner)` | Stable card keys; empty reward key for gold |
| `GetRequestedName(owner)`, `GetRewardName(owner)` | Display names; gold rewards read “150 gold” |
| `GetGoldReward(owner)` | Gold amount, or zero for a card trade |

Register again when a save loads. Wait for `Tessera_Collection.IsReady()`, then call `RefreshOffer(owner)` to read the saved terms and resume your quest. Do not create a replacement offer on load. Getters return the most recently published state; `RefreshOffer` reports `refreshed` when it is available.

A new offer can replace a completed or cancelled one, and receives a new ID. Cancel before stopping an unfinished quest. On load, Tessera also releases reservations whose quest or collector has disappeared, whose quest has stopped, or whose cards are no longer available. The API adds no periodic world scan.

The Missives addon in `addons/missives` is a complete example of board registration, named notices, dialogue, cancellation and restoring accepted quests.

## Names on notes and objectives

Skyrim's `<Alias=Requested>` text uses the **base object's name**. `SetDisplayName` on a placed reference does not supply that name. Give each changing text field its own base object, name it with `Form.SetName`, and fill its reference alias with **Stores Text** enabled. Load the persistent references before the quest records. The note's alias needs **Uses Stored Text**.

After filling the aliases, call `Tessera_Exchange.RefreshText(Self)` and wait for `text-refreshed` before publishing the notice. This refreshes filled reference aliases marked Stores Text for the current quest instance. Use this only for your own active exchange quest. Restore the base names and repeat the call after loading; base-form names are not stored by Tessera.
