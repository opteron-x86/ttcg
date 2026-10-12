#pragma once
#include <array>

namespace preview {
enum class CardContext { General, Hand, Board };

struct MatchAppearance {
    int treatment=0,scope=2,palette=0,borderStyle=0;
    float strength=1,borderWidth=1.2f,borderOpacity=1;
    bool marker=false;
    unsigned playerColour=0x87c4dd,opponentColour=0xdf9585;

    bool treats(int owner,CardContext context) const {
        return owner==1&&treatment!=0&&strength>0&&context!=CardContext::General&&
            (scope==2||(scope==0&&context==CardContext::Hand)||(scope==1&&context==CardContext::Board));
    }
    unsigned colour(int owner,bool parchmentText=false) const {
        if(palette==4)return owner==0?playerColour:opponentColour;
        // Keep the preset hues legible when scores sit on a pale window.
        if(parchmentText){
            constexpr std::array<std::array<unsigned,2>,4> ink{{
                {0x1b6278,0x994d3d},{0x246091,0x8c591a},
                {0x744896,0x296c54},{0x554a3b,0x364650}
            }};
            return ink[palette>=0&&palette<4?palette:0][owner==0?0:1];
        }
        constexpr std::array<std::array<unsigned,2>,4> colours{{
            {0x87c4dd,0xdf9585},{0x65b8ee,0xf0b65d},
            {0xc5a1ee,0x81ccb2},{0xf0eadc,0x687580}
        }};
        return colours[palette>=0&&palette<4?palette:0][owner==0?0:1];
    }
};
}
