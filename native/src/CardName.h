#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string_view>
#include <vector>

namespace preview {
// Small caps are synthesized like CSS font-variant on a font without an smcp
// face. Work in codepoints so wrapping cannot split UTF-8 expansion names.
inline unsigned nameUpper(unsigned c) {
    if((c>='a'&&c<='z')||(c>=0xe0&&c<=0xf6)||(c>=0xf8&&c<=0xfe)||
       (c>=0x430&&c<=0x44f)||(c>=0x3b1&&c<=0x3c1)||(c>=0x3c3&&c<=0x3cb))return c-32;
    if(c==0xff)return 0x178;
    if(c>=0x450&&c<=0x45f)return c-80;
    return c;
}
struct NameGlyph {unsigned value;float size,width;};
struct NameLine {std::vector<NameGlyph> glyphs;float width=0;};
inline std::vector<NameLine> nameLines(ImFont* font,std::string_view name,float size,float wrap,int lettering,float tracking=0) {
    std::vector<NameLine> lines(1);
    std::vector<NameGlyph> word;
    const float space=font->CalcTextSizeA(size,1e5f,0," ").x+tracking;
    const auto emit=[&] {
        if(word.empty())return;
        float width=0;for(const auto& g:word)width+=g.width;
        if(!lines.back().glyphs.empty()&&lines.back().width+space+width>wrap)lines.emplace_back();
        if(!lines.back().glyphs.empty()){lines.back().glyphs.push_back({' ',size,space});lines.back().width+=space;}
        for(const auto& g:word) {
            // Extremely long unbroken names still stay inside the card.
            if(!lines.back().glyphs.empty()&&lines.back().width+g.width>wrap)lines.emplace_back();
            lines.back().glyphs.push_back(g);lines.back().width+=g.width;
        }
        word.clear();
    };
    const char* p=name.data();const char* end=p+name.size();
    while(p<end) {
        unsigned c=0;const auto used=ImTextCharFromUtf8(&c,p,end);if(!used)break;p+=used;
        if(c==' '||c=='\n'||c=='\t'){emit();if(c=='\n'&&!lines.back().glyphs.empty())lines.emplace_back();continue;}
        const auto upper=nameUpper(c);const float pixels=lettering==0&&upper!=c?size*.78f:size;
        if(lettering!=1)c=upper;
        char utf8[5]{};ImTextCharToUtf8(utf8,c);
        word.push_back({c,pixels,font->CalcTextSizeA(pixels,1e5f,0,utf8).x+tracking});
    }
    emit();return lines;
}
inline void drawNameLines(ImDrawList* draw,ImFont* font,ImVec2 at,ImVec2 size,const std::vector<NameLine>& lines,float y,float pixels,float scale,ImU32 ink) {
    const auto* baked=font->GetFontBaked(pixels);
    const float baseline=baked->Ascent;
    draw->PushClipRect(at,at+size,true);
    for(const auto& line:lines) {
        float x=at.x+(size.x-line.width)*.5f;
        for(const auto& g:line.glyphs) {
            char utf8[5]{};ImTextCharToUtf8(utf8,g.value);
            const ImVec2 pos(x,y+baseline-font->GetFontBaked(g.size)->Ascent);
            // The released face uses a soft, short shadow rather than an outline.
            for(const auto offset:{ImVec2(-.6f,1),ImVec2(.6f,1),ImVec2(0,1.6f)})
                draw->AddText(font,g.size,pos+offset*scale,IM_COL32(0,0,0,100),utf8);
            draw->AddText(font,g.size,pos,ink,utf8);
            x+=g.width;
        }
        y+=pixels*1.16f;
    }
    draw->PopClipRect();
}
inline void drawCardName(ImDrawList* draw,ImFont* font,ImVec2 at,ImVec2 size,std::string_view name,int plate,int lettering,float scale,std::string_view group={}) {
    float bottom=at.y+size.y*.96f;
    if(!group.empty()) {
        const float pixels=std::clamp(size.x*.056f,9.f*scale,18.f*scale),wrap=size.x*.78f;
        auto lines=nameLines(font,group,pixels,wrap,1);
        // Long expansion group lists stay within two lines; inspection retains
        // the complete list. Leave the outer corners clear for control markers.
        if(lines.size()>2) {
            lines.resize(2);auto& last=lines.back();
            const float dots=font->CalcTextSizeA(pixels,1e5f,0,"…").x;
            while(!last.glyphs.empty()&&last.width+dots>wrap){last.width-=last.glyphs.back().width;last.glyphs.pop_back();}
            last.glyphs.push_back({0x2026,pixels,dots});last.width+=dots;
        }
        const float y=at.y+size.y*.986f-float(lines.size())*pixels*1.16f;
        drawNameLines(draw,font,at,size,lines,y,pixels,scale,IM_COL32(233,217,183,210));
        bottom=std::min(at.y+size.y*.93f,y-size.y*.01f);
    }
    if(name.empty())return;
    const float fontSize=std::clamp(size.x*(lettering==2?.06f:.069f),(lettering==2?12.f:13.f)*scale,(lettering==2?25.f:28.f)*scale);
    auto lines=nameLines(font,name,fontSize,size.x*.86f,lettering);
    // Fit long expansion titles while preserving their complete names.
    float textScale=1;
    if(lines.size()>3){textScale=3.f/lines.size();lines=nameLines(font,name,fontSize*textScale,size.x*.86f,lettering);}
    const float pixels=fontSize*textScale,lineHeight=pixels*1.16f;
    const float height=std::max(size.y*.15f,float(lines.size())*lineHeight+size.x*.08f);
    const float top=bottom-height;
    if(plate==0) {
        constexpr unsigned opacity[]{0,153,223,153,0};
        for(int n=0;n<4;++n) {
            const auto a=IM_COL32(16,27,22,opacity[n]),b=IM_COL32(16,27,22,opacity[n+1]);
            draw->AddRectFilledMultiColor(ImVec2(at.x+size.x*n*.25f,top),ImVec2(at.x+size.x*(n+1)*.25f,bottom),a,b,b,a);
        }
    }
    const float y=(top+bottom-float(lines.size())*lineHeight)*.5f;
    drawNameLines(draw,font,at,size,lines,y,pixels,scale,IM_COL32(245,232,205,255));
}
inline void drawCardShade(ImDrawList* draw,ImVec2 at,ImVec2 size) {
    const ImVec2 start=at+ImVec2(0,size.y*.58f),mid=at+ImVec2(size.x,size.y*.7564f);
    const auto clear=IM_COL32(16,29,22,0),middle=IM_COL32(16,29,22,77),dark=IM_COL32(16,29,22,179);
    draw->AddRectFilledMultiColor(start,mid,clear,clear,middle,middle);
    draw->AddRectFilledMultiColor(ImVec2(at.x,mid.y),at+size,middle,middle,dark,dark);
}
}
