#pragma once
#include "Collection.h"
#include "SavedMatch.h"
#include <cstring>

namespace ttcg {
inline constexpr std::uint32_t collectionRecord = 0x434F4C4C, matchRecord = 0x4D415443;
// Far above any real collection; a larger length means a damaged co-save.
inline constexpr std::uint32_t saveRecordLimit = 64u * 1048576;
// One Tessera record exactly as the co-save holds it.
struct SaveRecord {
    std::uint32_t type = 0, version = 0;
    std::vector<std::uint8_t> bytes;
    bool operator==(const SaveRecord&) const = default;
    // Empty when the length is not a whole number of 32-bit words.
    std::vector<std::uint32_t> words() const {
        std::vector<std::uint32_t> out(bytes.size() % 4 ? 0 : bytes.size() / 4);
        if (!out.empty()) std::memcpy(out.data(), bytes.data(), bytes.size());
        return out;
    }
};
struct SaveRecords {
    std::vector<SaveRecord> records;
    // False when a record could not be read in full, so it cannot be written back.
    bool complete = true;
};
template <class Serial> SaveRecords readSaveRecords(Serial& serial) {
    SaveRecords result;
    std::uint32_t type = 0, version = 0, length = 0;
    while (serial.GetNextRecordInfo(type, version, length)) {
        if (type != collectionRecord && type != matchRecord) continue;
        SaveRecord record{type, version, {}};
        if (length > saveRecordLimit) {
            result.complete = false;
            continue;
        }
        record.bytes.resize(length);
        if (length && serial.ReadRecordData(record.bytes.data(), length) != length) {
            result.complete = false;
            continue;
        }
        result.records.push_back(std::move(record));
    }
    return result;
}
template <class Serial> bool writeSaveRecords(Serial& serial, const std::vector<SaveRecord>& records) {
    bool written = true;
    for (const auto& record : records)
        written &= serial.WriteRecord(record.type, record.version, record.bytes.data(),
                                      static_cast<std::uint32_t>(record.bytes.size()));
    return written;
}
struct LoadedSave {
    CollectionSave collection;
    // The interrupted table's words, before its world IDs are resolved.
    std::vector<std::uint32_t> match;
    bool valid = true;
    // When the collection can't be used, every record as it was read. Saving writes
    // these back unchanged, so a newer or repaired Tessera can still load them.
    std::vector<SaveRecord> preserved;
};
// `migrate` moves a pre-30 collection's world IDs to the current scheme; false rejects it.
template <class Migrate> LoadedSave decodeSaveRecords(SaveRecords read, Migrate migrate) {
    LoadedSave result;
    result.valid = read.complete;
    for (const auto& record : read.records) {
        if (record.type == matchRecord) {
            if (savedMatchRecord(record.version, record.bytes.size())) result.match = record.words();
            continue;
        }
        const auto words = record.words();
        if (!supportedCollectionFormat(record.version) || record.bytes.size() % 4 ||
            !decodeCollection(words, result.collection, record.version) ||
            (record.version < 30 && !migrate(result.collection)))
            result.valid = false;
    }
    if (!result.valid) {
        result.collection = {};
        result.match.clear();
        result.preserved = std::move(read.records);
    }
    return result;
}
}
