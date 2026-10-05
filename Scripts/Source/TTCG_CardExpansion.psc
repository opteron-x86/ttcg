Scriptname TTCG_CardExpansion Hidden
; Permanent keys are setId:cardId, exactly as written in the expansion JSON.
Bool Function IsCardLoaded(String cardId) Global Native
; Queue a player reward on the game thread. Quantity is 1-99 (1 for uniques).
; Unavailable cards, duplicate uniques and invalid requests are logged to TTCG.log.
; This function has no return value. Quest authors manage their own reward-once flag.
Function GiveCard(String cardId, Int quantity = 1, Bool foil = False) Global Native
