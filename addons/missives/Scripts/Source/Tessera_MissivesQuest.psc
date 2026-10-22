Scriptname Tessera_MissivesQuest extends Quest

Int Property Region Auto
Book Property Notice Auto
ReferenceAlias Property Collector Auto
ReferenceAlias Property Board Auto
ReferenceAlias Property Missive Auto
ReferenceAlias Property Requested Auto
ReferenceAlias Property Reward Auto
Int offerID = 0
Bool closing = False
Bool accepting = False

; Stage zero runs each time Missives starts this repeatable quest.
Function Fragment_0()
    closing = False
    accepting = False
    offerID = 0
    RegisterForModEvent("Tessera_ExchangeChanged", "OnExchangeChanged")
    Tessera_Exchange.PostLocalOffer(Self, Region)
EndFunction

Function Fragment_1()
    CompleteAllObjectives()
    Cleanup()
EndFunction

Function Fragment_2()
    FailAllObjectives()
    Cleanup()
EndFunction

Function Fragment_3()
    Cleanup()
EndFunction

Function Restore()
    If IsRunning()
        RegisterForModEvent("Tessera_ExchangeChanged", "OnExchangeChanged")
        Tessera_Exchange.RefreshOffer(Self)
    EndIf
EndFunction

Function AcceptNotice()
    If !closing && !accepting && GetStage() == 0 && offerID > 0
        accepting = True
        Tessera_Exchange.AcceptOffer(Self, offerID)
    EndIf
EndFunction

Function Deliver(Actor speaker)
    If GetStage() == 20 && !closing
        Tessera_Exchange.CompleteOffer(Self, offerID, speaker)
    EndIf
EndFunction

Function Cleanup()
    closing = True
    If offerID > 0 && Tessera_Exchange.GetStatus(Self) < 3
        Tessera_Exchange.CancelOffer(Self, offerID)
    Else
        RemoveNotice()
        Stop()
    EndIf
EndFunction

Function RemoveNotice()
    ObjectReference note = Missive.GetReference()
    Missive.Clear()
    If note
        If Game.GetPlayer().GetItemCount(note) > 0
            Game.GetPlayer().RemoveItem(note, 1, True)
        ElseIf Board.GetReference()
            Board.GetReference().RemoveItem(note, 1, True)
        EndIf
        note.Disable()
        note.Delete()
    EndIf
EndFunction

Bool Function FillNames()
    ; Resolve explicitly: an old save can have empty forced aliases.
    If Region < 1 || Region > 9
        Return False
    EndIf
    Int offset = (Region - 1) * 2
    ObjectReference requestedRef = Game.GetFormFromFile(0x910 + offset, "Tessera Missives.esp") as ObjectReference
    ObjectReference rewardRef = Game.GetFormFromFile(0x911 + offset, "Tessera Missives.esp") as ObjectReference
    Actor collectorRef = Tessera_Exchange.GetCollector(Self)
    String requestedName = Tessera_Exchange.GetRequestedName(Self)
    String rewardName = Tessera_Exchange.GetRewardName(Self)
    If !requestedRef || !rewardRef || !collectorRef || requestedName == "" || rewardName == ""
        Debug.Trace("Tessera Missives: cannot fill notice names for region " + Region)
        Return False
    EndIf
    ; Skyrim's stored text uses FULL on the base, not SetDisplayName on a ref.
    ; Separate bases also prevent one hold's offer from renaming another's.
    Form requestedBase = Game.GetFormFromFile(0x930 + offset, "Tessera Missives.esp")
    Form rewardBase = Game.GetFormFromFile(0x931 + offset, "Tessera Missives.esp")
    If !requestedBase || !rewardBase || requestedRef.GetBaseObject() != requestedBase || rewardRef.GetBaseObject() != rewardBase
        Debug.Trace("Tessera Missives: notice name records need an addon update for region " + Region)
        Return False
    EndIf
    requestedBase.SetName(requestedName)
    rewardBase.SetName(rewardName)
    Collector.Clear()
    Collector.ForceRefTo(collectorRef)
    Requested.Clear()
    Requested.ForceRefTo(requestedRef)
    Reward.Clear()
    Reward.ForceRefTo(rewardRef)
    Return True
EndFunction

Function RefreshNotice()
    ; Refresh a notice already on the board or in the player's inventory.
    ; Keep its reference, the accepted quest and the reserved reward intact.
    ObjectReference note = Missive.GetReference()
    If note
        Missive.Clear()
        Missive.ForceRefTo(note)
    EndIf
EndFunction

Event OnExchangeChanged(String eventName, String result, Float number, Form sender)
    If sender != Self || !IsRunning()
        Return
    EndIf
    offerID = Tessera_Exchange.GetOfferID(Self)
    Int offerState = Tessera_Exchange.GetStatus(Self)
    If closing
        If offerState >= 3 || offerState == 0
            RemoveNotice()
            Stop()
        EndIf
        Return
    EndIf
    If offerState == 3
        If GetStage() == 20
            SetStage(100)
        Else
            SetStage(110)
        EndIf
    ElseIf offerState == 4
        SetStage(105)
    ElseIf result == "posted" || result == "refreshed" || result == "accepted"
        If offerState == 0
            SetStage(110)
            Return
        EndIf
        If FillNames()
            Tessera_Exchange.RefreshText(Self)
        EndIf
    ElseIf result == "text-refreshed"
        RefreshNotice()
        If offerState == 1 && !Missive.GetReference()
            ObjectReference boardRef = Board.GetReference()
            If !boardRef
                SetStage(110)
                Return
            EndIf
            ObjectReference note = boardRef.PlaceAtMe(Notice, 1, True, True)
            If !note
                SetStage(110)
                Return
            EndIf
            Missive.ForceRefTo(note)
            boardRef.AddItem(note, 1, True)
        EndIf
        If offerState == 2
            accepting = False
            SetStage(20)
            SetObjectiveDisplayed(20)
        ElseIf offerState == 1 && Game.GetPlayer().GetItemCount(Missive.GetReference()) > 0
            accepting = False
            AcceptNotice()
        EndIf
    ElseIf result == "missing-card"
        Debug.Notification("You need a standard copy of the requested card.")
    ElseIf result == "changed"
        Debug.Notification("Your collection changed. Speak to the collector again.")
    ElseIf result == "busy"
        accepting = False
        If GetStage() == 0 && !Missive.GetReference()
            SetStage(110)
        Else
            Debug.Notification("Finish your current Tessera game or trade first.")
        EndIf
    ElseIf result == "unavailable" || result == "stale"
        If GetStage() == 0
            SetStage(110)
        Else
            Debug.Notification("This exchange is unavailable right now.")
        EndIf
    EndIf
EndEvent
