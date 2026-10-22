"""Evaluate shipped child dialogue gates at quest and scene boundaries."""
from pathlib import Path
import json
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from plugin_records import walk, parse_fields
from child_dialogue_records import generate_child_dialogue
from game_settings_records import generate_game_settings

BLOB = (ROOT / 'Tessera TCG.esp').read_bytes()
CHILD_INFO = 0x02000000 | json.loads((ROOT / 'data/free-play-record-layout.json').read_text())['childAccept']
start, end, _ = walk(BLOB)[0][CHILD_INFO]
CONDITIONS = [v for k, v in parse_fields(BLOB[start:end])[1] if k == b'CTDA']


def offered(actor=0x1347E, stages=(), player_factions=(), actor_factions=(), states=None, enabled=True, album=True, child=True):
    # Skyrim chains OR conditions through the next non-OR entry. Different
    # chains are ANDed; checking these groups catches a gate leaking to all NPCs.
    states = states or {}
    group = False
    for condition in CONDITIONS:
        flags, = struct.unpack_from('<B', condition)
        target, = struct.unpack_from('<f', condition, 4)
        function, = struct.unpack_from('<H', condition, 8)
        p1, p2, run_on, reference = struct.unpack_from('<IIII', condition, 12)
        assert len(condition) == 32 and not flags & 0x1E
        assert run_on in (0, 2)
        if run_on == 2:
            assert reference == 0x14 and function == 71
        if function == 365:
            value = child
        elif function == 74:
            value = {0x02000FDA: enabled, 0x02000850: 2 if album else 0}[p1]
        elif function == 72:
            value = actor == p1
        elif function == 59:
            value = (p1, p2) in stages
        elif function == 71:
            value = p1 in (player_factions if run_on == 2 else actor_factions)
        elif function in (590, 46, 35, 289, 237, 244, 49, 719):
            if function == 719:
                assert p1 == 0x14
            value = states.get(function, 0)
        else:
            raise AssertionError(f'Unexpected condition {function}')
        opcode = flags >> 5
        result = {0: value == target, 1: value != target, 2: value > target,
                  3: value >= target, 4: value < target, 5: value <= target}[opcode]
        group |= result
        if not flags & 1:
            if not group:
                return False
            group = False
    assert not CONDITIONS[-1][0] & 1
    return True


class ChildDialogueTests(unittest.TestCase):
    def test_ordinary_and_mod_added_children(self):
        for actor in (0x1347E, 0x1434D, 0x13363, 0x136BA, 0x0A003F56):
            self.assertTrue(offered(actor))
            self.assertFalse(offered(actor, album=False))
            self.assertFalse(offered(actor, enabled=False))
            self.assertFalse(offered(actor, child=False))
        for function in (590, 46, 35, 289, 237, 244, 49, 719):
            self.assertFalse(offered(states={function: 1}), function)

    def test_aventus_requires_reward_handover(self):
        for stage in (0, 5, 20, 30, 255):
            self.assertFalse(offered(0x14132, stages={(0x1EA50, stage)}))
        self.assertTrue(offered(0x14132, stages={(0x1EA50, 200)}))
        self.assertFalse(offered(0x14132, stages={(0x1EA50, 200)}, states={590: 1}))

    def test_fjotra_requires_rescue_not_the_statue_route(self):
        for stage in (0, 60, 70, 80, 100, 205, 210):
            self.assertFalse(offered(0x1E82C, stages={(0x23B6C, stage)}))
        self.assertTrue(offered(0x1E82C, stages={(0x23B6C, 200)}))
        self.assertFalse(offered(0x1E82C, stages={(0x23B6C, 200)}, states={244: 1}))

    def test_aeta_requires_wind_stone(self):
        for stage in (0, 10, 60, 200):
            self.assertFalse(offered(0x01018FCA, stages={(0x01017F90, stage)}))
        self.assertTrue(offered(0x01018FCA, stages={(0x01017F90, 70)}))

    def test_haming_both_actors(self):
        for stage in (0, 60, 180, 500):
            self.assertFalse(offered(0x13642, stages={(0x3372B, stage)}))
        for stage in (900, 1000):
            self.assertTrue(offered(0x13642, stages={(0x3372B, stage)}))
        self.assertTrue(offered(0xC029C))  # Froki's Shack actor, including alternate starts.

    def test_babette_has_no_membership_requirement(self):
        self.assertTrue(offered(0x1D4B7))
        self.assertTrue(offered(0x1D4B7, actor_factions={0x1BDB3}))
        self.assertTrue(offered(0x1D4B7, stages={(0x1EA51, 220)}))
        self.assertTrue(offered(0x1D4B7, player_factions={0x1BDB3}))
        for function in (590, 289, 719):
            self.assertFalse(offered(0x1D4B7, states={function: 1}))

    def test_apparitions_are_never_practice_opponents(self):
        completed = {(0x1EA50, 200), (0x23B6C, 200), (0x01017F90, 70), (0x3372B, 900)}
        for actor in (0x274A5, 0x9F839):
            self.assertFalse(offered(actor, stages=completed, player_factions={0x1BDB3}))

    def test_update_is_idempotent_and_changes_only_child_response(self):
        self.assertEqual(BLOB, generate_game_settings(generate_child_dialogue(BLOB)))
        new = generate_child_dialogue(BLOB)
        before, after = walk(BLOB)[0], walk(new)[0]
        self.assertEqual(set(before), set(after))
        for form, (a, b, _) in before.items():
            if form == CHILD_INFO:
                continue
            c, d, _ = after[form]
            self.assertEqual(BLOB[a:b], new[c:d], hex(form))


if __name__ == '__main__':
    unittest.main()
