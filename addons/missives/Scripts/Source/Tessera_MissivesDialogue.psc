Scriptname Tessera_MissivesDialogue extends TopicInfo Hidden
Function Fragment_0(ObjectReference speaker)
    (GetOwningQuest() as Tessera_MissivesQuest).Deliver(speaker as Actor)
EndFunction
