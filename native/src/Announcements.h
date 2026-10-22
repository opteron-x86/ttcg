#pragma once
#include "CardName.h"
#include <cmath>

namespace preview {
inline float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
inline ImU32 fade(ImU32 colour,float opacity){return (colour&0xffffff)|(unsigned(((colour>>24)&255)*std::clamp(opacity,0.f,1.f))<<24);}
inline void gradientLine(ImDrawList* d,ImVec2 at,float width,ImU32 colour,float thickness=1){
    const auto clear=colour&0xffffff;const auto middle=at+ImVec2(width*.5f,thickness);
    d->AddRectFilledMultiColor(at,middle,clear,colour,colour,clear);
    d->AddRectFilledMultiColor(ImVec2(middle.x,at.y),at+ImVec2(width,thickness),colour,clear,clear,colour);
}
inline void drawStarter(ImDrawList* d,ImVec2 at,ImVec2 size,float scale,float progress,int player){
    const float opacity=smooth(progress/.18f)*(1-smooth((progress-.84f)/.16f));
    const float travel=1-std::pow(1-std::clamp((progress-.24f)/.52f,0.f,1.f),3);
    const auto centre=at+size*.5f;
    gradientLine(d,centre-ImVec2(140*scale,0),280*scale,fade(IM_COL32(187,161,107,55),opacity));
    const auto gold=fade(IM_COL32(229,203,144,255),opacity);
    const float x=(player?1:-1)*120*scale*travel;
    const auto tip=centre+ImVec2(x,0);
    if(std::abs(x)>1){const auto left=x<0?tip:centre,right=x<0?centre:tip;
        const auto faint=fade(gold,.25f);d->AddRectFilledMultiColor(left,right+ImVec2(0,scale),x<0?gold:faint,x<0?faint:gold,x<0?faint:gold,x<0?gold:faint);}
    d->AddQuad(centre+ImVec2(0,-3)*scale,centre+ImVec2(3,0)*scale,centre+ImVec2(0,3)*scale,centre+ImVec2(-3,0)*scale,fade(gold,.45f),scale);
    for(int r=8;r>4;--r)d->AddCircleFilled(tip,r*scale,fade(gold,.025f));
    d->AddQuadFilled(tip+ImVec2(0,-5.5f)*scale,tip+ImVec2(5.5f,0)*scale,tip+ImVec2(0,5.5f)*scale,tip+ImVec2(-5.5f,0)*scale,gold);
}
inline void drawAnnouncement(ImDrawList* d,ImFont* font,ImVec2 at,ImVec2 size,float scale,
    const std::string& title,float progress,std::string_view tone="rule",bool result=false){
    if(title.empty()||progress<0||progress>=1)return;
    const float opacity=smooth(progress/.18f)*(1-smooth((progress-.80f)/.20f));
    const float zoom=progress<.18f?.94f+.06f*smooth(progress/.18f):1+.02f*smooth((progress-.8f)/.2f);
    const bool small=tone=="first"||tone=="group";
    const float pixels=(result?72:small?38:52)*scale*zoom,tracking=(small?4:8)*scale*zoom;
    const float width=std::min(size.x-40*scale,(small?560:1200)*scale);
    const auto lines=nameLines(font,title,pixels,width,0,tracking);
    const float height=lines.size()*pixels*1.2f;
    const auto centre=at+size*.5f;
    d->PushClipRect(at,at+size,true);
    // A soft band across the table, with transparent edges instead of a box.
    const float band=std::max(size.y*.14f,height*.5f+40*scale);
    const auto clear=IM_COL32(7,20,13,0),shoulder=fade(IM_COL32(7,20,13,187),opacity),dark=fade(IM_COL32(7,20,13,221),opacity);
    const float stops[]{-band,-band*3/7,0,band*3/7,band};const ImU32 colours[]{clear,shoulder,dark,shoulder,clear};
    for(int i=0;i<4;++i)d->AddRectFilledMultiColor(ImVec2(at.x,centre.y+stops[i]),ImVec2(at.x+size.x,centre.y+stops[i+1]),colours[i],colours[i],colours[i+1],colours[i+1]);
    const float shift=(-.35f+.5f*(1-std::pow(1-progress,3)))*width;
    const auto rule=fade(IM_COL32(220,195,133,220),opacity);
    gradientLine(d,ImVec2(centre.x-width/2+shift,centre.y-height/2-22*scale),width*.85f,rule,scale);
    gradientLine(d,ImVec2(centre.x+width/2-width*.85f-shift,centre.y+height/2+22*scale),width*.85f,rule,scale);
    const auto colour=fade(tone=="combo"?IM_COL32(203,234,254,255):tone=="lose"?IM_COL32(239,183,168,255):tone=="draw"?IM_COL32(220,229,233,255):IM_COL32(243,228,185,255),opacity);
    float y=centre.y-height/2;const float baseline=font->GetFontBaked(pixels)->Ascent;
    for(const auto& row:lines){float x=centre.x-(row.width-tracking)/2;
        for(const auto& g:row.glyphs){char utf8[5]{};ImTextCharToUtf8(utf8,g.value);
            const auto pos=ImVec2(x,y+baseline-font->GetFontBaked(g.size)->Ascent);
            for(const auto offset:{ImVec2(-1,3),ImVec2(1,3),ImVec2(0,4)})d->AddText(font,g.size,pos+offset*scale,fade(IM_COL32(0,0,0,105),opacity),utf8);
            d->AddText(font,g.size,pos,colour,utf8);x+=g.width;
        }y+=pixels*1.2f;
    }
    d->PopClipRect();
}
}
