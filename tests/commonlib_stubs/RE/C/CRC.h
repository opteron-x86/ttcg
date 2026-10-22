#pragma once
namespace RE {
template<class T> struct BSCRC32 {
    std::uint32_t operator()(const T& value) const { return static_cast<std::uint32_t>(value); }
};
}
