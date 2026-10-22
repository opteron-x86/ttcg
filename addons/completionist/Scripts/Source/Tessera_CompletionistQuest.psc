Scriptname Tessera_CompletionistQuest extends Quest
Import Tessera_CompletionistNative
Bool working = False
Int retries = 0

Event OnInit()
    Setup()
EndEvent

Function Setup()
    UnregisterForUpdate()
    working = False
    retries = 0
    RegisterForModEvent("Tessera_CompletionistChanged", "OnCollectionChanged")
    RegisterForMenu("Journal Menu")
    RegisterForSingleUpdate(1.0)
EndFunction

Event OnCollectionChanged(String eventName, String status, Float value, Form sender)
    retries = 0
    RegisterForSingleUpdate(0.25)
EndEvent

Event OnMenuClose(String menuName)
    ; Also retry after MCM setup or a manually disabled page is re-enabled.
    retries = 0
    RegisterForSingleUpdate(0.25)
EndEvent

Bool Function SyncBatch(Int session)
    Int page = GetPendingPage()
    If page < 0 || GetSession() != session
        Return True
    EndIf
    String pageName = GetPageName(page)
    String[] config = Completionist_Native.GetPageConfiguration(pageName, GetPageNumber(page))
    ; Each generated page has exactly one section. Framework IDs are assigned
    ; by Completionist each launch, so never save or hard-code them.
    If config.Length != 5 || config[0] != "1" || config[4] != "0"
        Return False
    EndIf
    Int framework = config[1] as Int
    Form[] markers = GetPendingForms(page, session)
    Int i = 0
    While i < markers.Length && GetSession() == session
        Form marker = markers[i]
        Int status = Completionist_Native.IsOptionCompleted(framework, marker)
        If GetSession() != session
            Return True
        EndIf
        If status == 0
            ; This function TOGGLES; never call it on an already checked card.
            Completionist_Native.SetOptionCompleted(framework, marker)
            status = Completionist_Native.IsOptionCompleted(framework, marker)
        EndIf
        If status != 1
            Return False
        EndIf
        Acknowledge(marker, session)
        i += 1
    EndWhile
    Return True
EndFunction

Event OnUpdate()
    If working
        Return
    EndIf
    Int session = GetSession()
    If session == 0 || GetPendingPage() < 0
        Return
    EndIf
    working = True
    Bool succeeded = SyncBatch(session)
    working = False
    If GetSession() != session || GetPendingPage() < 0
        Return
    EndIf
    If succeeded
        retries = 0
        RegisterForSingleUpdate(0.1)
    ElseIf retries < 5
        retries += 1
        RegisterForSingleUpdate(2.0)
    Else
        Debug.Trace("Tessera Completionist: checklist unavailable. Progress is saved; retrying on the next collection change or MCM close.")
    EndIf
EndEvent
