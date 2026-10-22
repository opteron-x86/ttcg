"""Child challenge conditions, also evaluated by native hotkey/lobby entry.

Quest stages and actor identities are from the official master records. Use
GetStageDone for completed objectives: later stages can represent other paths
(T01 205 is the stolen statue; DB02 220 is killing Astrid).
"""
import json
from plugin_records import ROOT, walk, parse_fields, build_record, apply, ctda


def child_conditions():
    conditions = [
        ctda(0, 1, 365, 0),                # IsChild
        ctda(0x60, 1, 74, 0x02000850),    # Album owned
        ctda(0, 0, 590, 0),               # IsInScene
        ctda(0, 0, 46, 0),                # GetDead
        ctda(0, 0, 35, 0),                # GetDisabled
        ctda(0, 0, 289, 0),               # IsInCombat
        ctda(0, 0, 237, 0),               # GetIsGhost
        ctda(0, 0, 244, 0),               # GetRestrained
        ctda(0, 0, 49, 0),                # GetSleeping
        ctda(0, 0, 719, 0x14),            # IsHostileToActor PlayerRef
        ctda(0, 0, 72, 0x274A5),          # Helgi: quest apparition
        ctda(0, 0, 72, 0x9F839),          # Little Pelagius: dream apparition
    ]
    for actor, quest, stage in (
        (0x14132, 0x1EA50, 200),           # Aventus: Innocence Lost reward handed over
        (0x1E82C, 0x23B6C, 200),          # Fjotra: The Heart of Dibella completed
        (0x01018FCA, 0x01017F90, 70),      # Aeta: Wind Stone freed (Dragonborn master index 1)
    ):
        # Each pair is (speaker is not this child OR required stage is done).
        # The last condition is AND, keeping different children's gates separate.
        conditions += [ctda(1, 0, 72, actor), ctda(0, 1, 59, quest, param2=stage)]
    # The tutorial actor and the later Haming at Froki's Shack have different
    # bases. Only the Helgen version needs this gate; stage 1000 also handles
    # starts that finish the introduction without its final journal entry.
    conditions += [ctda(1, 0, 72, 0x13642),
                   ctda(1, 1, 59, 0x3372B, param2=900),
                   ctda(0, 1, 59, 0x3372B, param2=1000)]
    return conditions


def generate_child_dialogue(blob):
    local = json.loads((ROOT / 'data/free-play-record-layout.json').read_text())['childAccept']
    start, end, parents = walk(blob)[0][0x02000000 | local]
    header, fields = parse_fields(blob[start:end])
    fields = [(key, value) for key, value in fields if key != b'CTDA']
    at = next((i for i, (key, _) in enumerate(fields) if key in
               (b'RNAM', b'ANAM', b'TWAT', b'ONAM')), len(fields))
    fields[at:at] = [(b'CTDA', value) for value in child_conditions()]
    return apply(blob, start, end, parents, build_record(header, fields))
