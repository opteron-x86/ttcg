// Exercise the actual pinned BSTHashMap, BSTTuple and GPtr implementations.
// Substitute only the game allocator, hash and target of intrusive references.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std::literals;
namespace stl {
template<class F> struct scope_exit { F f;~scope_exit() { f(); } };
template<class F> scope_exit(F) -> scope_exit<F>;
[[noreturn]] inline void report_and_fail(std::string_view message) { throw std::runtime_error(std::string(message)); }
}
#include "RE/B/BSTHashMap.h"
#include "RE/G/GPtr.h"
#ifdef TESSERA_REQUIRE_HASHMAP_PATCH
static_assert(TESSERA_COMMONLIB_BSTHASHMAP_LIFETIME_PATCH == 1);
#endif

namespace {
void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }

struct Menu {
    std::atomic<int> references{1};
    int destructions=0;
    void AddRef() { ++references; }
    // Keep the probe's storage alive so a premature/double release is reported
    // deterministically instead of accessing freed memory inside the test.
    void Release() { if(--references==0)++destructions; }
};
// Match UI::UIMenuEntry's copy/move behavior, including its user-defined dtor.
struct MenuEntry {
    MenuEntry() {}
    MenuEntry(RE::GPtr<Menu> a_menu,Menu* (*a_create)()) : menu(a_menu),create(a_create) {}
    ~MenuEntry() {}
    RE::GPtr<Menu> menu;
    Menu* (*create)();
};
using Registry=RE::BSTHashMap<std::uint32_t,MenuEntry>;

void registration(unsigned count) {
    std::array<Menu,64> menus;
    {
        Registry registry;
        registry.reserve(64);
        for(unsigned i=0;i<count;++i) {
            registry.insert({i,{RE::GPtr<Menu>(&menus[i]),nullptr}});
            menus[i].Release();
            require(menus[i].references==1 && menus[i].destructions==0,"Initial menu ownership is incorrect");
        }
        registry.insert({64,{nullptr,nullptr}});
        for(unsigned i=0;i<count;++i) {
            require(menus[i].references==1 && menus[i].destructions==0,
                    "Menu registration released an existing menu prematurely");
            require(registry.find(i)->second.menu.get()==&menus[i],"Menu lookup changed during registration");
        }
    }
    for(unsigned i=0;i<count;++i)
        require(menus[i].references==0 && menus[i].destructions==1,"Registry teardown released a menu more than once");
}

void collisionsAndReuse() {
    // 0 and 8 collide; 1 then evicts the displaced entry from its home slot.
    constexpr std::array<unsigned,6> keys{0,8,1,16,9,2};
    std::array<Menu,keys.size()> menus;
    {
        Registry registry;
        registry.reserve(8);
        for(unsigned i=0;i<keys.size();++i) {
            registry.insert({keys[i],{RE::GPtr<Menu>(&menus[i]),nullptr}});
            menus[i].Release();
        }
        registry.erase(0); // Erase a chain head, moving a successor.
        require(menus[0].references==0 && menus[0].destructions==1,"Erase did not release exactly its own menu");
        registry.reserve(128); // Rehash with empty/previously erased slots.
        for(unsigned i=1;i<keys.size();++i) {
            require(menus[i].references==1 && menus[i].destructions==0,"Collision/erase/rehash damaged another menu");
            require(registry.find(keys[i])->second.menu.get()==&menus[i],"Collision chain lost a menu");
        }
        registry.clear();
        registry.reserve(256); // Empty table growth must not revisit dead entries.
        registry.insert({99,{nullptr,nullptr}});
        registry.clear();
    }
    for(const auto& menu:menus)
        require(menu.references==0 && menu.destructions==1,"Clear/reuse/teardown released a dead menu");
}

struct Value {
    inline static int alive=0;
    Value() { ++alive; }
    Value(const Value&) { ++alive; }
    ~Value() { --alive; }
};
void emptyStorage() {
    {
        RE::BSTHashMap<unsigned,Value> values;
        values.reserve(64);
        require(Value::alive==0,"Empty hash slots constructed live values");
        values.insert({1,Value{}});
        require(Value::alive==1,"Insertion leaked a default-constructed value");
        values.reserve(128);
        require(Value::alive==1,"Growth changed the number of live values");
        values.erase(1);
        require(Value::alive==0,"Erase left a value alive");
    }
    require(Value::alive==0,"Empty storage destruction revisited a dead value");
}
}
int main() {
    try {
        registration(64); // Reproduces the reported mechanism before other cases.
        registration(63); // Additional registration without growth.
        collisionsAndReuse();
        emptyStorage();
        std::cout<<"CommonLib lifetime: registration with/without growth, collision/erase, clear/reuse and empty storage passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
