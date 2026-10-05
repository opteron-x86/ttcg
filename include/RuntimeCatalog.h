#pragma once
#include <type_traits>
#include <vector>
namespace ttcg {
// Some session objects construct a Match while the DLL is loading. Materialize
// their catalog on first use, rather than relying on COMDAT initializer order
// between inline variables in the Windows build.
template<auto& Seed> struct RuntimeCatalog {
 using Value=typename std::remove_cvref_t<decltype(Seed)>::value_type;
 static std::vector<Value>& entries(){static std::vector<Value> values(Seed.begin(),Seed.end());return values;}
 auto size() const{return entries().size();}
 bool empty() const{return entries().empty();}
 auto begin() const{return entries().begin();}
 auto end() const{return entries().end();}
 Value& operator[](std::size_t i) const{return entries()[i];}
 Value& at(std::size_t i) const{return entries().at(i);}
 Value& back() const{return entries().back();}
 void push_back(const Value& value) const{entries().push_back(value);}
 template<class It> void assign(It first,It last) const{entries().assign(first,last);}
};
}
