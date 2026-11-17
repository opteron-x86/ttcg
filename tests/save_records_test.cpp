// A save Tessera can't read must come back out of the next save unchanged.
#include "Campaign.h"
#include "SaveRecords.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace ttcg;

struct Serial {
    std::vector<SaveRecord> records, written;
    std::size_t at = 0;
    // Bytes withheld from the next read, as a truncated co-save would.
    std::uint32_t shortBy = 0;
    bool GetNextRecordInfo(std::uint32_t& type, std::uint32_t& version, std::uint32_t& length) {
        if (at == records.size()) return false;
        type = records[at].type;
        version = records[at].version;
        length = static_cast<std::uint32_t>(records[at].bytes.size());
        ++at;
        return true;
    }
    std::uint32_t ReadRecordData(void* out, std::uint32_t length) {
        const auto& bytes = records[at - 1].bytes;
        const auto count = std::min<std::uint32_t>(length, static_cast<std::uint32_t>(bytes.size())) - shortBy;
        std::memcpy(out, bytes.data(), count);
        return count;
    }
    bool WriteRecord(std::uint32_t type, std::uint32_t version, const void* data, std::uint32_t length) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        written.push_back({type, version, {bytes, bytes + length}});
        return true;
    }
};
static SaveRecord record(std::uint32_t type, std::uint32_t version, const std::vector<std::uint32_t>& words) {
    SaveRecord out{type, version, std::vector<std::uint8_t>(words.size() * 4)};
    if (!words.empty()) std::memcpy(out.bytes.data(), words.data(), out.bytes.size());
    return out;
}
static LoadedSave load(Serial& serial, bool migrated = true) {
    return decodeSaveRecords(readSaveRecords(serial), [&](CollectionSave&) { return migrated; });
}
int main() {
    CollectionSave current;
    grantStarter(current);
    const auto words = encodeCollection(current);
    SavedMatch table;
    table.present = true;
    table.opponent = 0xA2C94;
    table.ledger = 0x13BB9;
    table.competitive = true;
    table.table = Match(7, 0);
    const auto match = record(matchRecord, savedMatchFormat, encodeMatch(table));
    assert(match.bytes.size() == savedMatchWords * 4 && savedMatchRecord(savedMatchFormat, match.bytes.size()));

    // A readable save loads, and nothing is held back for rewriting.
    Serial good{{record(0x4F544852, 1, {1, 2, 3}), record(collectionRecord, collectionFormat, words), match}};
    auto loaded = load(good);
    assert(loaded.valid && loaded.preserved.empty() && loaded.match == encodeMatch(table));
    assert(encodeCollection(loaded.collection) == words);

    // A format from a newer Tessera, a damaged payload and a ragged length are all
    // kept byte for byte, together with the interrupted table that belongs to them.
    auto damaged = words;
    damaged.resize(damaged.size() / 2);
    auto ragged = record(collectionRecord, collectionFormat, words);
    ragged.bytes.push_back(0x5A);
    for (const auto& unreadable :
         {record(collectionRecord, collectionFormat + 2, {9, 8, 7}),
          record(collectionRecord, collectionFormat, damaged), ragged, record(collectionRecord, 34, words)}) {
        Serial serial{{unreadable, match}};
        loaded = load(serial);
        assert(!loaded.valid && loaded.match.empty() && loaded.preserved == serial.records);
        assert(encodeCollection(loaded.collection) == encodeCollection(CollectionSave{}));
        Serial next;
        assert(writeSaveRecords(next, loaded.preserved) && next.written == serial.records);
        // The rewritten save is unreadable in the same way, and survives again.
        next.records = next.written;
        const auto again = load(next);
        assert(!again.valid && again.preserved == serial.records);
    }

    // A pre-30 collection whose world IDs can't be migrated is kept, not dropped.
    std::ifstream old("tests/fixtures/collection-v26-courier.txt");
    assert(old);
    std::vector<std::uint32_t> wire;
    for (std::uint32_t n; old >> n;) wire.push_back(n);
    Serial legacy{{record(collectionRecord, 26, wire)}};
    assert(load(legacy).valid);
    legacy.at = 0;
    loaded = load(legacy, false);
    assert(!loaded.valid && loaded.preserved == legacy.records);

    // A record cut short by the file can't be written back; the rest still is.
    Serial truncated{{match, record(collectionRecord, collectionFormat, words)}};
    auto read = readSaveRecords(truncated);
    assert(read.complete && read.records.size() == 2);
    truncated.at = 1;
    truncated.shortBy = 4;
    read = readSaveRecords(truncated);
    assert(!read.complete && read.records.empty());
    loaded = decodeSaveRecords({{match}, false}, [](CollectionSave&) { return true; });
    assert(!loaded.valid && loaded.preserved == std::vector{match});

    {
        // A table saved before the Vanguard existed is one word shorter and still loads.
        auto older = match;
        older.version = 1;
        older.bytes.resize(savedMatchWordsV1 * 4);
        Serial serial{{record(collectionRecord, collectionFormat, words), older}};
        const auto old = load(serial);
        SavedMatch restored;
        assert(old.valid && old.match.size() == savedMatchWordsV1 && decodeMatch(old.match, restored));
        assert(restored.table.vanguard == -1 && !savedMatchRecord(1, savedMatchWords * 4) &&
               !savedMatchRecord(2, savedMatchWordsV1 * 4) && !savedMatchRecord(3, savedMatchWords * 4));
    }
    // An unsupported table beside a good collection is dropped; its stakes are recovered.
    Serial stale{{record(collectionRecord, collectionFormat, words), record(matchRecord, savedMatchFormat + 1, {1})}};
    loaded = load(stale);
    assert(loaded.valid && loaded.match.empty() && loaded.preserved.empty());
    std::cout << "Save records: unreadable collections and their tables survive a save unchanged\n";
}
