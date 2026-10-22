Scriptname Tessera_MissivesNote extends ReferenceAlias
Message Property DiscardMessage Auto

Event OnContainerChanged(ObjectReference newContainer, ObjectReference oldContainer)
    Tessera_MissivesQuest job = GetOwningQuest() as Tessera_MissivesQuest
    If newContainer == Game.GetPlayer()
        job.AcceptNotice()
    ElseIf oldContainer == Game.GetPlayer() && job.GetStage() == 20
        job.SetStage(105)
    EndIf
EndEvent

Event OnEquipped(Actor reader)
    If reader == Game.GetPlayer() && GetOwningQuest().GetStage() == 20
        Utility.WaitMenuMode(2.0)
        If DiscardMessage.Show() == 0
            GetOwningQuest().SetStage(105)
        EndIf
    EndIf
EndEvent
