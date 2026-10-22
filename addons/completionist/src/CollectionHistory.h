#pragma once
#include <Tessera/CollectionAPI.h>
#include <cstdint>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace tessera::completionist {
// Stable card keys, never catalog positions or runtime FormIDs. Completionist
// saves its checkmarks too; this journal also covers acquisitions still waiting
// for the Papyrus bridge when the player saves or trades a card away.
class CollectionHistory {
public:
    enum : unsigned { Collected = 1, Foil = 2 };
    std::map<std::string, unsigned> cards;

    bool observe(const TesseraCollectionCard& card) {
        if (!card.key || !*card.key || (card.flags & TESSERA_CARD_PENDING)) return false;
        unsigned bits = card.foilCount ? unsigned(Collected | Foil) : card.standardCount ? unsigned(Collected) : 0u;
        if (!bits) return false;
        auto& previous = cards[card.key];
        const bool changed = (previous | bits) != previous;
        previous |= bits;
        return changed;
    }
    bool collected(const std::string& key, bool foil) const {
        const auto it = cards.find(key);
        return it != cards.end() && (it->second & (foil ? Foil : Collected));
    }
    std::string encode() const { return nlohmann::json(cards).dump(); }
    bool decode(const std::string& data) {
        // Validate the entire record before replacing this save's history.
        const auto value = nlohmann::json::parse(data, nullptr, false);
        if (!value.is_object() || value.size() > 50000) return false;
        std::map<std::string, unsigned> restored;
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key().empty() || it.key().size() > 256 || it.key().find(':') == std::string::npos ||
                !it.value().is_number_unsigned()) return false;
            const auto bits = it.value().get<std::uint64_t>();
            if (bits != Collected && bits != (Collected | Foil)) return false;
            restored.emplace(it.key(), static_cast<unsigned>(bits));
        }
        cards = std::move(restored);
        return true;
    }
};
}
