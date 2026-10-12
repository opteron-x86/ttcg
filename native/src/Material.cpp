#include "Renderer.h"

namespace preview {
// The approved Prismatic finish, shared by the OpenGL and D3D11 backends.
// Grain is fixed in card coordinates; rotation moves the light, never the grain.
static const char* materialBody = R"SHADER(
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float bell(float d, float width) { return exp(-d*d/(width*width)); }
vec3 spectrum(float phase) {
    vec3 bands=.5+.5*cos(phase*6.2831853+vec3(0.,2.0943951,4.1887902));
    return mix(vec3(.12,.12,.12),pow(bands,vec3(1.8,1.8,1.8)),.88);
}
vec3 finish(vec2 p, vec3 base, vec2 light, float strength) {
    vec2 paper=(p-.5)*vec2(.75,1.);
    vec2 lamp=(light-.5)*vec2(.96,1.15);
    vec2 cell=floor(p*vec2(300.,400.));
    float grain=hash(cell), grain2=hash(cell+vec2(59.,23.));
    float deposit=.76+.24*grain;
    vec2 axis=normalize(vec2(.88,.47));
    float band=dot(paper,axis)-dot(lamp,axis)+.017*sin(p.y*9.)+.010*sin(p.x*17.+p.y*4.);
    float phase=band*1.47+light.x*.30-light.y*.19;
    vec3 colour=spectrum(phase);
    vec3 incident=normalize(vec3(lamp*1.45,1.));
    vec3 eye=normalize(vec3(-paper*.18,1.));
    vec3 halfVector=normalize(incident+eye);
    vec3 normal=normalize(vec3((grain-.5)*.30,(grain2-.5)*.30,1.));
    float glint=pow(max(0.,dot(normal,halfVector)),160.)*smoothstep(.965,1.,grain2);
    vec2 distanceToLight=(paper-lamp)*vec2(2.1,2.8);
    float clearCoat=exp(-dot(distanceToLight,distanceToLight));
    vec3 reflected=colour*(.018+.34*bell(band,.23))*deposit;
    reflected+=vec3(.96,.98,1.)*(.04*clearCoat+.19*bell(band,.035)+glint*.38);
    reflected+=spectrum(phase+.43)*bell(band-.29,.07)*.06;
    reflected*=1.+clamp(length(lamp)*.72,0.,.42);
    float edgeDistance=min(min(p.x,1.-p.x),min(p.y,1.-p.y));
    reflected+=vec3(.68,.79,.82)*(1.-smoothstep(0.,.012,edgeDistance))*clearCoat*.10;
    return 1.-(1.-base)*(1.-clamp(reflected,0.,.72)*strength);
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
    return std::string("#version 330 core\nin vec2 uv;out vec4 outputColour;uniform sampler2D source;uniform vec2 light;uniform float strength;uniform float treatment;uniform float amount;\n") + materialBody +
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
