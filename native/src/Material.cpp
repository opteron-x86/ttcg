#include "Renderer.h"

namespace preview {
static const char* materialBody = R"SHADER(
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float bell(float d, float width) { return exp(-d*d/(width*width)); }
vec3 spectrum(float phase) {
    vec3 bands=.5+.5*cos(phase*6.2831853+vec3(0.,2.0943951,4.1887902));
    return mix(vec3(.12,.12,.12),pow(bands,vec3(1.8,1.8,1.8)),.88);
}
vec3 finish(vec2 p, vec3 base, vec2 light, float strength) {
    // A holographic film under the ink: colour rides the printed art and never greys the shadows.
    vec2 paper=(p-.5)*vec2(.75,1.);
    vec2 lamp=light-.5;
    float luma=dot(base,vec3(.2126,.7152,.0722));
    float ink=smoothstep(.05,.6,luma);
    vec2 axis=normalize(vec2(.83,.56));
    float along=dot(paper,axis), across=dot(paper,vec2(-axis.y,axis.x));
    // Broad iridescence that drifts with the light, bent so it never reads as a ruled rainbow.
    float field=along*1.15+.16*sin(across*4.6+lamp.y*3.)+.07*sin(along*9.-across*3.+lamp.x*4.)+dot(lamp,vec2(1.9,1.3));
    vec3 hue=spectrum(field);
    float grey=dot(hue,vec3(.333,.333,.333));
    hue=mix(vec3(grey,grey,grey),hue,.7);
    // One soft sheen crossing the card, with a tight core.
    float band=along-dot(lamp,axis)*1.7+.03*sin(across*7.);
    float sheen=bell(band,.30), core=bell(band,.045);
    // Flakes in the film that flash as the sheen passes over them.
    vec2 cell=floor(p*vec2(150.,200.));
    float flake=hash(cell), turn=hash(cell+vec2(59.,23.));
    float flash=smoothstep(.93,1.,flake)*bell(band+(turn-.5)*.5,.06);
    // The film tints the ink at constant brightness; only the sheen and flakes add light.
    vec3 tint=hue/max(dot(hue,vec3(.2126,.7152,.0722)),.3);
    float cover=(.30+.42*sheen)*(.3+.7*ink)*strength;
    vec3 colour=mix(base,clamp(base*tint,0.,1.),clamp(cover,0.,1.));
    vec3 light3=base*hue*sheen*.38*ink+hue*(.02+.05*sheen)*(.4+.6*ink);
    light3+=vec3(1.,.98,.94)*(core*.13*(.35+.65*ink)+flash*.5);
    float edgeDistance=min(min(p.x,1.-p.x),min(p.y,1.-p.y));
    light3+=hue*(1.-smoothstep(0.,.014,edgeDistance))*sheen*.18;
    return clamp(colour+light3*strength,0.,1.);
}
vec3 artwork(vec2 p,vec3 base,vec2 light,float strength,float treatment,float amount) {
    vec3 colour=base;
    if(strength>0.)colour=finish(p,base,light,strength);
    float grey=dot(colour,vec3(.2126,.7152,.0722));
    vec3 toned=vec3(grey,grey,grey);
    if(treatment<.5)return colour;
    if(treatment<1.5)amount*=.6;
    if(treatment>2.5)toned=clamp(vec3(
        dot(colour,vec3(.393,.769,.189)),
        dot(colour,vec3(.349,.686,.168)),
        dot(colour,vec3(.272,.534,.131))),0.,1.);
    return mix(colour,toned,amount);
}
)SHADER";
std::string glFragment() {
    return std::string(
               "#version 330 core\nin vec2 uv;out vec4 outputColour;uniform sampler2D source;uniform vec2 light;uniform float strength;uniform float treatment;uniform float amount;\n") +
           materialBody +
           "\nvoid main(){vec4 b=texture(source,uv);outputColour=vec4(artwork(uv,b.rgb,light,strength,treatment,amount),b.a);}";
}
std::string hlslShader() {
    return std::string(R"SHADER(
#define vec2 float2
#define vec3 float3
#define mix lerp
#define fract frac
Texture2D source : register(t0);
SamplerState sampling : register(s0);
cbuffer Parameters : register(b0) { float2 light; float strength; float treatment; float amount; float3 padding; };
struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex vertex(uint id : SV_VertexID) {
    Vertex v;
    v.uv=float2((id<<1)&2,id&2);
    v.position=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);
    return v;
}
)SHADER") + materialBody +
           "\nfloat4 pixel(Vertex input):SV_TARGET{float4 b=source.Sample(sampling,input.uv);return float4(artwork(input.uv,b.rgb,light,strength,treatment,amount),b.a);}";
}
}
