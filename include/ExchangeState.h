#pragma once
#include <cstdint>
namespace ttcg {
enum class ExchangeStatus: unsigned { Posted=1, Accepted, Completed, Cancelled };
struct CardExchange {
    std::uint32_t serial=0, collector=0, ledger=0, requested=0, reward=0;
    unsigned gold=0;
    ExchangeStatus status=ExchangeStatus::Posted;
    bool active() const {return status==ExchangeStatus::Posted||status==ExchangeStatus::Accepted;}
    bool operator==(const CardExchange&) const=default;
};
inline constexpr unsigned maximumExchanges=512, maximumExchangeGold=100000;
}
