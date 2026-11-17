#pragma once
#include "Campaign.h"
// A world as a save from 0.33 or earlier has it: every hold with the terms it
// began with then. Scenario tests start from it so their holds are known.
inline ttcg::CollectionSave legacyWorld() {
    ttcg::CollectionSave save;
    for (const auto& legacy : ttcg::legacyHoldTerms) save.holds[legacy.hold] = legacy.terms;
    return save;
}
