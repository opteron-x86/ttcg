// Authoring-only adapter from Blender's evaluated meshes to Skyrim SE NIFs.
// Uses nifly (GPL-3.0); neither this tool nor nifly is a game dependency.
#include <NifFile.hpp>
#include <bhk.hpp>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace nifly;
using json = nlohmann::json;
constexpr float havokScale = 69.99125f;

json vector(const Vector3& v) { return {v.x, v.y, v.z}; }

json inspect(NifFile& nif) {
    json out;
    auto& header = nif.GetHeader();
    auto* root = nif.GetRootNode();
    out["sse"] = header.GetVersion().IsSSE();
    out["root"] = root->GetBlockName();
    out["meshes"] = json::array();
    for (auto* shape : nif.GetShapes()) {
        json m;
        m["name"] = shape->name.get();
        m["vertices"] = json::array();
        m["normals"] = json::array();
        m["uvs"] = json::array();
        m["triangles"] = json::array();
        for (auto v : *nif.GetVertsForShape(shape)) m["vertices"].push_back(vector(v));
        if (auto* normals = nif.GetNormalsForShape(shape))
            for (auto v : *normals) m["normals"].push_back(vector(v));
        if (auto* uvs = nif.GetUvsForShape(shape))
            for (auto v : *uvs) m["uvs"].push_back({v.u, 1.0f-v.v});
        std::vector<Triangle> tris;
        shape->GetTriangles(tris);
        for (auto t : tris) m["triangles"].push_back({t.p1,t.p2,t.p3});
        for (unsigned i=0; i<9; ++i) {
            std::string path;
            nif.GetTextureSlot(shape, path, i);
            if (!path.empty()) m["textures"][std::to_string(i)] = path;
        }
        if (auto* shader = dynamic_cast<BSLightingShaderProperty*>(nif.GetShader(shape))) {
            m["glossiness"] = shader->glossiness;
            m["specularStrength"] = shader->specularStrength;
            m["flags1"] = shader->shaderFlags1;
            m["flags2"] = shader->shaderFlags2;
        }
        out["meshes"].push_back(m);
    }
    if (auto* c = header.GetBlock<bhkCollisionObject>(root->collisionRef)) {
        out["collision"]["flags"] = c->flags;
        out["collision"]["target"] = c->targetRef.index;
        if (auto* body = header.GetBlock<bhkRigidBody>(c->bodyRef)) {
            auto& b = out["collision"];
            b["body"] = body->GetBlockName(); b["mass"] = body->mass;
            b["layer"] = body->collisionFilter.layer;
            b["motion"] = body->motionSystem; b["quality"] = body->qualityType;
            b["deactivator"] = body->deactivatorType;
            b["solver"] = body->solverDeactivation;
            b["bodyFlags"] = body->bodyFlagsInt;
            b["broadPhase"] = body->broadPhaseType;
            b["inertia"] = body->inertiaMatrix;
            if (auto* box = header.GetBlock<bhkBoxShape>(body->shapeRef)) {
                b["halfExtents"] = vector(box->dimensions);
                b["material"] = box->GetMaterial();
                b["radius"] = box->radius;
            }
        }
    }
    if (auto* marker = nif.FindBlockByName<BSInvMarker>("INV"))
        out["inventory"] = {{"rotation", {marker->rotationX,marker->rotationY,marker->rotationZ}}, {"zoom", marker->zoom}};
    if (auto* flags = nif.FindBlockByName<BSXFlags>("BSX")) out["bsx"] = flags->integerData;
    return out;
}

void write(const json& data, const std::filesystem::path& path) {
    NifFile nif;
    nif.Create(NiVersion::getSSE());
    auto& h = nif.GetHeader();
    auto fade = std::make_unique<BSFadeNode>();
    fade->name.get() = data.at("name");
    fade->flags = 14;
    h.ReplaceBlock(0, std::move(fade));
    auto* root = nif.GetRootNode();
    for (const auto& mesh : data.at("meshes")) {
        std::vector<Vector3> verts, normals;
        std::vector<Vector2> uvs;
        std::vector<Triangle> tris;
        for (const auto& v : mesh.at("vertices")) verts.emplace_back(v[0],v[1],v[2]);
        for (const auto& v : mesh.at("normals")) normals.emplace_back(v[0],v[1],v[2]);
        // Blender's image origin is bottom-left; NIF texture coordinates use top-left.
        for (const auto& v : mesh.at("uvs")) uvs.emplace_back(v[0],1.0f-v[1].get<float>());
        for (const auto& t : mesh.at("triangles")) tris.emplace_back(t[0],t[1],t[2]);
        if (verts.size() >= 65536 || verts.empty() || normals.size()!=verts.size() || uvs.size()!=verts.size())
            throw std::runtime_error("Invalid shape buffers");
        auto* shape = nif.CreateShapeFromData(mesh.at("name"), &verts, &tris, &uvs, &normals);
        shape->flags = 14;
        auto* shader = dynamic_cast<BSLightingShaderProperty*>(nif.GetShader(shape));
        shader->shaderFlags1 = SLSF1_ZBUFFER_TEST | SLSF1_SPECULAR | SLSF1_RECEIVE_SHADOWS | SLSF1_CAST_SHADOWS;
        shader->shaderFlags2 = SLSF2_ZBUFFER_WRITE;
        shader->glossiness = mesh.value("glossiness", 20.0f);
        shader->specularStrength = mesh.value("specularStrength", 0.35f);
        shader->specularColor = Vector3(1.0f,0.94f,0.84f);
        shader->emissiveMultiple = 0.0f;
        for (const auto& [slot, value] : mesh.at("textures").items()) {
            std::string texture = value;
            nif.SetTextureSlot(shape, texture, std::stoul(slot));
        }
        nif.CalcTangentsForShape(shape);
    }
    auto flags = std::make_unique<BSXFlags>();
    flags->name.get() = "BSX";
    flags->integerData = BSX_HAVOK | BSX_DYNAMIC | BSX_ARTICULATED;
    root->extraDataRefs.AddBlockRef(h.AddBlock(std::move(flags)));
    auto marker = std::make_unique<BSInvMarker>();
    marker->name.get() = "INV";
    auto rotations = data.at("inventoryRotation").get<std::vector<uint16_t>>();
    marker->rotationX=rotations[0];marker->rotationY=rotations[1];marker->rotationZ=rotations[2];
    marker->zoom=1.0f;
    root->extraDataRefs.AddBlockRef(h.AddBlock(std::move(marker)));

    auto box = std::make_unique<bhkBoxShape>();
    auto dims = data.at("collisionHalfExtents");
    box->dimensions = Vector3(dims[0].get<float>()/havokScale,dims[1].get<float>()/havokScale,dims[2].get<float>()/havokScale);
    box->SetMaterial(3839073443u); // Skyrim cloth material.
    box->radius=0.003f;
    box->radius2=0.003f;
    auto shapeID = h.AddBlock(std::move(box));
    auto body = std::make_unique<bhkRigidBody>();
    body->shapeRef.index = shapeID;
    body->collisionFilter.layer = 4; // CLUTTER
    body->collisionFilterCopy = body->collisionFilter;
    body->broadPhaseType=1; // ENTITY
    body->mass=0.2f;body->friction=0.65f;body->restitution=0.08f;
    body->linearDamping=0.1f;body->angularDamping=0.25f;
    body->motionSystem=4; // BOX_INERTIA
    body->deactivatorType=1;body->solverDeactivation=2;
    body->qualityType=4; // MOVING
    body->penetrationDepth=0.005f;
    body->rotation.w=1.0f;
    const float x=dims[0].get<float>()/havokScale, y=dims[1].get<float>()/havokScale, z=dims[2].get<float>()/havokScale;
    body->inertiaMatrix[0]=body->mass*(y*y+z*z)/3.0f;
    body->inertiaMatrix[5]=body->mass*(x*x+z*z)/3.0f;
    body->inertiaMatrix[10]=body->mass*(x*x+y*y)/3.0f;
    auto collision = std::make_unique<bhkCollisionObject>();
    collision->targetRef.index = 0;
    collision->flags=129; // ACTIVE | SYNC_ON_UPDATE
    collision->bodyRef.index = h.AddBlock(std::move(body));
    root->collisionRef.index = h.AddBlock(std::move(collision));
    if (nif.Save(path)!=0) throw std::runtime_error("NIF save failed");
    NifFile reopened;
    if (reopened.Load(path)!=0 || !reopened.GetHeader().GetVersion().IsSSE())
        throw std::runtime_error("NIF did not reopen as Skyrim SE");
    if (reopened.GetShapes().size()!=data.at("meshes").size())
        throw std::runtime_error("Shape count changed on round trip");
    std::cout << path << ": " << reopened.GetShapes().size() << " shapes; SSE round trip passed\n";
}

int main(int argc, char** argv) {
    try {
        if (argc!=4) throw std::runtime_error("Usage: nif-export write|inspect input output");
        if (std::string(argv[1])=="write") {
            std::ifstream stream(argv[2]);json data;stream>>data;write(data,argv[3]);
        } else if (std::string(argv[1])=="inspect") {
            NifFile nif;
            if(nif.Load(argv[2])!=0)throw std::runtime_error("Cannot load NIF");
            std::ofstream stream(argv[3]);stream<<inspect(nif).dump(2)<<'\n';
        } else throw std::runtime_error("Unknown operation");
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
