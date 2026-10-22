; Compile-only declarations for the public Completionist Papyrus functions.
; The addon never installs or replaces Completionist's scripts.
Scriptname Completionist_Native Hidden
String[] Function GetPageConfiguration(String mcmpage, Int pageNumber) Global Native
Int Function IsOptionCompleted(Int frameworkID, Form item) Global Native
Function SetOptionCompleted(Int frameworkID, Form item) Global Native
