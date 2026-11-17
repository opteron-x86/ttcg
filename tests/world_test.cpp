#include "Exchange.h"
#include "Campaign.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;
int main() {
    CollectionSave s;
    for (int i : {1, 2, 3}) assert(unlockedOpponent(s, i));
    assert(unlockedOpponent(s, 5));
    const auto stock = s.player;
    const auto deck = s.deck;
    for (auto base : {0x13285u, 0x12345678u}) {
        const auto loan = childStock(base);
        assert(cardCount(loan) >= 24 && validHand(selectHand(loan, 123), loan));
        for (auto [id, n] : loan)
            assert(cards[cardIndex(id)].tier <= 2 && std::string_view(cards[cardIndex(id)].rarity) != "Epic");
        bool offers = false, ordinary = false;
        for (unsigned day = 0; day < 20; ++day) {
            auto rule = childRequestedRule(s, base, day);
            offers |= rule != 0;
            ordinary |= rule == 0;
            assert(!(rule & ~(Same | Plus | Reverse | Open)));
        }
        assert(offers && ordinary && s.players.empty());
    }
    assert(count(personalPool(opponentIndex(0x132AB)), 1257661) == 1);
    assert(!s.starter && s.player == stock && s.deck == deck && s.opponents.empty() && s.reputation.empty());
    CollectionSave restored;
    assert(decodeCollection(encodeCollection(s), restored) && encodeCollection(restored) == encodeCollection(s));
    // All personal decks are legal. Timed replenishment supplies ordinary themed cards.
    for (int i = 0; i < static_cast<int>(opponents.size()); ++i) {
        CollectionSave c;
        const auto actor = opponents[i].reference;
        replenishOpponent(c, i, actor, 100);
        auto& own = c.opponents[actor];
        assert(cardCount(own) == cardCount(personalPool(i)));
        for (unsigned n = 0; n < 32; ++n) assert(validHand(opponentHand(own, i, n), own));
        auto acquired = cards[600].form;
        own[acquired] = 2;
        replenishOpponent(c, i, actor, 101);
        assert(count(own, acquired) == 2);
        for (auto& [id, n] : own) n = 0;
        assert(decodeCollection(encodeCollection(c), restored));
        replenishOpponent(restored, i, actor, 171);
        assert(cardCount(restored.opponents[actor]) == 0);
        replenishOpponent(restored, i, actor, 172);
        assert(playableCount(restored.opponents[actor]) >= 5);
        for (auto [id, n] : restored.opponents[actor])
            if (n)
                assert((std::string_view(cards[cardIndex(id)].rarity) == "Common" ||
                        std::string_view(cards[cardIndex(id)].rarity) == "Rare") &&
                       n <= 2);
        assert(validHand(opponentHand(restored.opponents[actor], i, 123), restored.opponents[actor]));
    }
    // A copy offered in an exchange is not restocked behind the offer's back:
    // cancelling returns it, and the collection is the size it started at.
    unsigned offers = 0;
    for (int i = 0; i < static_cast<int>(opponents.size()); ++i) {
        if (!opponents[i].restock) continue;
        CollectionSave c;
        const auto actor = opponents[i].reference, quest = worldFormID({"Collector.esp", 0x800});
        replenishOpponent(c, i, actor, 100);
        auto& own = c.opponents[actor];
        const int size = cardCount(own);
        CardID reward = 0, wanted = 0;
        for (auto [id, n] : own)
            if (n > 0 && exchangeCard(id) && !reward) reward = id;
        for (const auto& card : cards)
            if (exchangeCard(card.form) && card.form != reward && !wanted) wanted = card.form;
        if (!reward) continue;
        const int copies = count(own, reward);
        assert(postExchange(c, quest, actor, actor, wanted, reward, 0) == ExchangeResult::Done);
        for (unsigned hour = 172; hour < 172 + 72 * 6; hour += 72) replenishOpponent(c, i, actor, hour);
        assert(cardCount(own) == size - 1 && count(own, reward) == copies - 1);
        assert(cancelExchange(c, quest, 1) == ExchangeResult::Done);
        assert(cardCount(own) == size && count(own, reward) == copies);
        ++offers;
    }
    assert(offers > 100);
    std::cout << "World: children have no game history; 320 personal collections, timed restocking passed\n";
}
