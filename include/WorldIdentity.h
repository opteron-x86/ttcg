#pragma once
#include <cstdint>
#include <string_view>
namespace ttcg {
// Authored identities are independent of the DLCs' runtime load indices.
inline const char* identityMaster(std::uint32_t id){
 switch(id>>24){case 0xDB:return "Dragonborn.esm";case 0xFB:return "HearthFires.esm";default:return nullptr;}
}
inline std::uint32_t authoredIdentity(std::string_view master,std::uint32_t local,std::uint32_t loaded){
 if(master=="Dragonborn.esm")return 0xDB000000u|(local&0xFFFFFFu);
 if(master=="HearthFires.esm")return 0xFB000000u|(local&0xFFFFFFu);
 return loaded;
}
}
