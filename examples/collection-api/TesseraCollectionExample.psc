Scriptname TesseraCollectionExample extends ReferenceAlias
; Attach to a player alias in your own addon quest. Optional display globals.
GlobalVariable Property OwnedCards Auto
GlobalVariable Property DiscoveredCards Auto
String Property SetID = "" Auto

Event OnInit()
    Subscribe()
EndEvent

Event OnPlayerLoadGame()
    Subscribe()
EndEvent

Function Subscribe()
    RegisterForModEvent("Tessera_CollectionChanged", "OnTesseraCollectionChanged")
    RegisterForSingleUpdate(0.1)
EndFunction

Event OnTesseraCollectionChanged(String eventName, String status, Float revision, Form sender)
    If status == "ready"
        RegisterForSingleUpdate(0.1)
    EndIf
EndEvent

Event OnUpdate()
    If !Tessera_Collection.IsReady()
        Return
    EndIf
    Int revision = Tessera_Collection.GetRevision()
    Int owned = Tessera_Collection.GetOwnedCardTotal(SetID)
    Int discovered = Tessera_Collection.GetDiscoveredCardTotal(SetID)
    If !Tessera_Collection.IsReady()
        Return
    EndIf
    If revision != Tessera_Collection.GetRevision()
        RegisterForSingleUpdate(0.1)
        Return
    EndIf
    If OwnedCards
        OwnedCards.SetValue(owned)
    EndIf
    If DiscoveredCards
        DiscoveredCards.SetValue(discovered)
    EndIf
EndEvent
