#include "GLTFLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include <unordered_map>

#include "Debug/Log.h"
#include "Utils/File.h"

static std::vector<uint8_t> Base64Decode(const std::string& encoded)
{
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);

    int val = 0, valb = -8;
    for (unsigned char c : encoded)
    {
        if (c == '=') break;
        const char* pos = std::strchr(kTable, c);
        if (!pos) continue;
        val = (val << 6) + static_cast<int>(pos - kTable);
        valb += 6;
        if (valb >= 0)
        {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

static std::vector<uint8_t> LoadBuffer(const nlohmann::json& bufferJson,
                                       const std::filesystem::path& basePath)
{
    if (!bufferJson.contains("uri"))
        return {};

    const std::string uri = bufferJson["uri"].get<std::string>();

    if (uri.rfind("data:", 0) == 0)
    {
        const size_t comma = uri.find(',');
        if (comma != std::string::npos)
            return Base64Decode(uri.substr(comma + 1));
        return {};
    }

    std::ifstream binFile(basePath / uri, std::ios::binary);
    if (!binFile.is_open())
        return {};

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(binFile), {});
}

template <typename T>
static std::vector<T> ReadAccessor(const nlohmann::json& gltf,
                                   const std::vector<std::vector<uint8_t>>& buffers,
                                   size_t accessorIndex)
{
    const auto& accessor = gltf["accessors"][accessorIndex];
    const size_t bvIndex = accessor["bufferView"].get<size_t>();
    const size_t count = accessor["count"].get<size_t>();
    const size_t aOffset = accessor.contains("byteOffset") ? accessor["byteOffset"].get<size_t>() : 0;

    const auto& bv = gltf["bufferViews"][bvIndex];
    const size_t bufIdx = bv["buffer"].get<size_t>();
    const size_t bvOffset = bv.contains("byteOffset") ? bv["byteOffset"].get<size_t>() : 0;
    const size_t stride = bv.contains("byteStride") ? bv["byteStride"].get<size_t>() : sizeof(T);

    const uint8_t* base = buffers[bufIdx].data() + bvOffset + aOffset;

    std::vector<T> result(count);
    for (size_t i = 0; i < count; ++i)
        std::memcpy(&result[i], base + i * stride, sizeof(T));

    return result;
}

static void ComputeVertices(GLTFLoader::Mesh& mesh)
{
    const size_t vertCount = mesh.positions.size();

    mesh.tangents.resize(vertCount, {0.f, 0.f, 0.f, 0.f});
    std::vector<Vec3f> bitangents(vertCount, {0.f, 0.f, 0.f});

    for (size_t k = 0; k < mesh.indices.size(); k += 3)
    {
        const Vec3i& idx0 = mesh.indices[k];
        const Vec3i& idx1 = mesh.indices[k + 1];
        const Vec3i& idx2 = mesh.indices[k + 2];

        const Vec3f& E1 = mesh.positions[idx1.x] - mesh.positions[idx0.x];
        const Vec3f& E2 = mesh.positions[idx2.x] - mesh.positions[idx0.x];

        const float dU1 = mesh.textureUVs[idx1.y].x - mesh.textureUVs[idx0.y].x;
        const float dV1 = mesh.textureUVs[idx1.y].y - mesh.textureUVs[idx0.y].y;
        const float dU2 = mesh.textureUVs[idx2.y].x - mesh.textureUVs[idx0.y].x;
        const float dV2 = mesh.textureUVs[idx2.y].y - mesh.textureUVs[idx0.y].y;

        float f = dU1 * dV2 - dU2 * dV1;
        f = (std::fabs(f) < 1e-6f) ? 1.f : 1.f / f;

        Vec3f T, B;
        T.x = f * (dV2 * E1.x - dV1 * E2.x);
        T.y = f * (dV2 * E1.y - dV1 * E2.y);
        T.z = f * (dV2 * E1.z - dV1 * E2.z);

        B.x = f * (-dU2 * E1.x + dU1 * E2.x);
        B.y = f * (-dU2 * E1.y + dU1 * E2.y);
        B.z = f * (-dU2 * E1.z + dU1 * E2.z);

        mesh.tangents[idx0.x] = mesh.tangents[idx0.x] + T;
        mesh.tangents[idx1.x] = mesh.tangents[idx1.x] + T;
        mesh.tangents[idx2.x] = mesh.tangents[idx2.x] + T;

        bitangents[idx0.x] = bitangents[idx0.x] + B;
        bitangents[idx1.x] = bitangents[idx1.x] + B;
        bitangents[idx2.x] = bitangents[idx2.x] + B;
    }

    for (size_t i = 0; i < mesh.indices.size(); ++i)
    {
        const Vec3i& idx = mesh.indices[i];
        const Vec3f& N = mesh.normals[idx.z];
        Vec3f T = Vec3f(mesh.tangents[idx.x].x, mesh.tangents[idx.x].y, mesh.tangents[idx.x].z);
        const Vec3f& Bref = bitangents[idx.x];

        const float NdotT = N.x * T.x + N.y * T.y + N.z * T.z;
        T.x -= NdotT * N.x;
        T.y -= NdotT * N.y;
        T.z -= NdotT * N.z;
        T.Normalize();

        Vec3f cross;
        cross.x = N.y * T.z - N.z * T.y;
        cross.y = N.z * T.x - N.x * T.z;
        cross.z = N.x * T.y - N.y * T.x;
        const float sign = (cross.x * Bref.x + cross.y * Bref.y + cross.z * Bref.z < 0.f) ? -1.f : 1.f;

        mesh.intermediateVertices.push_back(mesh.positions[idx.x].x);
        mesh.intermediateVertices.push_back(mesh.positions[idx.x].y);
        mesh.intermediateVertices.push_back(mesh.positions[idx.x].z);

        mesh.intermediateVertices.push_back(mesh.textureUVs[idx.y].x);
        mesh.intermediateVertices.push_back(mesh.textureUVs[idx.y].y);

        mesh.intermediateVertices.push_back(N.x);
        mesh.intermediateVertices.push_back(N.y);
        mesh.intermediateVertices.push_back(N.z);

        mesh.intermediateVertices.push_back(T.x);
        mesh.intermediateVertices.push_back(T.y);
        mesh.intermediateVertices.push_back(T.z);
        mesh.intermediateVertices.push_back(sign);
    }

    std::unordered_map<GLTFLoader::Vertex, uint32_t, GLTFLoader::VertexHash, GLTFLoader::VertexEqual> hashed_vertices;
    const GLTFLoader::Vertex    *ptr = reinterpret_cast<GLTFLoader::Vertex*>(mesh.intermediateVertices.data());

    for (size_t i = 0; i < mesh.indices.size(); i++)
    {
        GLTFLoader::Vertex v = *(ptr++);
        const auto &res = hashed_vertices.find(v);
        if (res != hashed_vertices.end())
        {
            mesh.finalIndices.push_back(res->second);
        }
        else
        {
            const uint32_t id = static_cast<uint32_t>(mesh.finalVertices.size());
            mesh.finalVertices.push_back(v);
            mesh.finalIndices.push_back(id);
            hashed_vertices[v] = id;
        }
    }
}

bool GLTFLoader::Load(const std::filesystem::path& fullPath, Model& model)
{
    model.path = fullPath;

    std::string content;
    if (!File::ReadAllText(fullPath, content))
    {
        PrintError("gLTF file %s does not exist", fullPath.generic_string().c_str());
        return false;
    }

    const nlohmann::json file = nlohmann::json::parse(content);
    const std::filesystem::path basePath = fullPath.parent_path();

    // Buffers
    std::vector<std::vector<uint8_t>> buffers;
    if (file.contains("buffers"))
        for (const auto& buf : file["buffers"])
            buffers.push_back(LoadBuffer(buf, basePath));

    // Images
    std::vector<std::string> imageURIs;
    if (file.contains("images"))
        for (const auto& img : file["images"])
            imageURIs.push_back(img.contains("uri") ? img["uri"].get<std::string>() : "");

    // Texture -> Image indirection
    std::vector<size_t> textureToImage;
    if (file.contains("textures"))
        for (const auto& tex : file["textures"])
            textureToImage.push_back(tex.contains("source") ? tex["source"].get<size_t>() : 0);

    auto getImagePath = [&](size_t texIdx) -> std::filesystem::path
    {
        if (texIdx < textureToImage.size())
        {
            const size_t imgIdx = textureToImage[texIdx];
            if (imgIdx < imageURIs.size())
                return imageURIs[imgIdx];
        }
        return {};
    };

    // Materials
    if (file.contains("materials"))
    {
        size_t index = 0;
        for (const auto& matJson : file["materials"])
        {
            Material mat;

            if (matJson.contains("name"))
                mat.name = matJson["name"].get<std::string>();
            else
                mat.name = fullPath.filename().stem().generic_string() + "_Material_" + std::to_string(index++);

            if (matJson.contains("normalTexture"))
                mat.normal = getImagePath(matJson.at("normalTexture").at("index").get<size_t>());

            if (matJson.contains("occlusionTexture"))
                mat.ao = getImagePath(matJson.at("occlusionTexture").at("index").get<size_t>());
            
            if (matJson.contains("pbrMetallicRoughness"))
            {
                const auto& pbr = matJson.at("pbrMetallicRoughness");

                if (pbr.contains("baseColorTexture"))
                    mat.albedo = getImagePath(pbr.at("baseColorTexture").at("index").get<size_t>());

                if (pbr.contains("metallicRoughnessTexture"))
                {
                    const auto p = getImagePath(pbr.at("metallicRoughnessTexture").at("index").get<size_t>());
                    mat.metallic = p;
                    mat.roughness = p;
                }

                if (pbr.contains("baseColorFactor"))
                {
                    const auto& v = pbr.at("baseColorFactor");
                    mat.color = Vec4f(v[0].get<float>(), v[1].get<float>(),
                                      v[2].get<float>(), v[3].get<float>());
                }

                if (pbr.contains("metallicFactor"))
                    mat.metallicFactor = pbr.at("metallicFactor").get<float>();

                if (pbr.contains("roughnessFactor"))
                    mat.roughnessFactor = pbr.at("roughnessFactor").get<float>();
            }

            model.materials.push_back(mat);
        }
    }

    // Meshes
    if (file.contains("meshes"))
    {
        size_t index = 0;
        for (const auto& meshJson : file["meshes"])
        {
            Mesh mesh;

            if (meshJson.contains("name"))
            {
                mesh.name = meshJson["name"].get<std::string>();
            }
            else
            {
                mesh.name = fullPath.filename().stem().generic_string() + "_Mesh_" + std::to_string(index++);
            }

            if (!meshJson.contains("primitives"))
            {
                model.meshes.push_back(mesh);
                continue;
            }

            for (const auto& primitive : meshJson["primitives"])
            {
                SubMesh subMesh;
                subMesh.startIndex = static_cast<uint32_t>(mesh.indices.size());

                const uint32_t vertexOffset = static_cast<uint32_t>(mesh.positions.size());
                const auto& attributes = primitive["attributes"];

                if (attributes.contains("POSITION"))
                {
                    auto positions = ReadAccessor<Vec3f>(file, buffers, attributes["POSITION"].get<size_t>());
                    mesh.positions.insert(mesh.positions.end(), positions.begin(), positions.end());
                }

                // UVs
                if (attributes.contains("TEXCOORD_0"))
                {
                    auto uvs = ReadAccessor<Vec2f>(file, buffers, attributes["TEXCOORD_0"].get<size_t>());
                    mesh.textureUVs.insert(mesh.textureUVs.end(), uvs.begin(), uvs.end());
                }

                if (attributes.contains("NORMAL"))
                {
                    auto normals = ReadAccessor<Vec3f>(file, buffers, attributes["NORMAL"].get<size_t>());
                    mesh.normals.insert(mesh.normals.end(), normals.begin(), normals.end());
                }

                // Indices
                if (primitive.contains("indices"))
                {
                    const size_t accessorIdx = primitive["indices"].get<size_t>();
                    const int componentType = file["accessors"][accessorIdx]["componentType"].get<int>();

                    auto pushIndices = [&](auto rawIndices)
                    {
                        for (auto idx : rawIndices)
                        {
                            const uint32_t v = static_cast<uint32_t>(idx) + vertexOffset;
                            mesh.indices.push_back(Vec3i(v, v, v));
                        }
                    };

                    if (componentType == 5125)
                        pushIndices(ReadAccessor<uint32_t>(file, buffers, accessorIdx));
                    else
                        pushIndices(ReadAccessor<uint16_t>(file, buffers, accessorIdx));
                }

                // Material
                if (primitive.contains("material"))
                {
                    const size_t matIdx = primitive["material"].get<size_t>();
                    if (matIdx < model.materials.size())
                        subMesh.materialName = model.materials[matIdx].name.generic_string();
                }

                subMesh.count = static_cast<uint32_t>(mesh.indices.size()) - subMesh.startIndex;
                mesh.subMeshes.push_back(subMesh);
            }

            ComputeVertices(mesh);
            model.meshes.push_back(mesh);
        }
    }

    // Nodes
    if (file.contains("nodes"))
    {
        const auto& nodesJson = file["nodes"];
        model.nodes.resize(nodesJson.size());

        for (size_t ni = 0; ni < nodesJson.size(); ++ni)
        {
            const auto& nj = nodesJson[ni];
            Node& node = model.nodes[ni];

            node.name = nj.contains("name") ? nj["name"].get<std::string>() : "";
            node.meshIndex = nj.contains("mesh") ? nj["mesh"].get<int>() : -1;

            if (nj.contains("children"))
            {
                for (const auto& child : nj["children"])
                {
                    const int childIdx = child.get<int>();
                    node.children.push_back(childIdx);
                    model.nodes[childIdx].parent = static_cast<int>(ni);
                }
            }

            if (nj.contains("matrix"))
            {
                const auto& m = nj["matrix"];
                float mat[16];
                for (int i = 0; i < 16; ++i)
                    mat[i] = m[i].get<float>();

                const float sx = std::sqrt(mat[0] * mat[0] + mat[1] * mat[1] + mat[2] * mat[2]);
                const float sy = std::sqrt(mat[4] * mat[4] + mat[5] * mat[5] + mat[6] * mat[6]);
                const float sz = std::sqrt(mat[8] * mat[8] + mat[9] * mat[9] + mat[10] * mat[10]);
                node.transform.scale = {sx, sy, sz};

                const float r00 = mat[0] / sx, r10 = mat[1] / sx, r20 = mat[2] / sx;
                const float r01 = mat[4] / sy, r11 = mat[5] / sy, r21 = mat[6] / sy;
                const float r02 = mat[8] / sz, r12 = mat[9] / sz, r22 = mat[10] / sz;

                float qx, qy, qz, qw;
                const float trace = r00 + r11 + r22;
                if (trace > 0.f)
                {
                    const float s = 0.5f / std::sqrt(trace + 1.f);
                    qw = 0.25f / s;
                    qx = (r21 - r12) * s;
                    qy = (r02 - r20) * s;
                    qz = (r10 - r01) * s;
                }
                else if (r00 > r11 && r00 > r22)
                {
                    const float s = 2.f * std::sqrt(1.f + r00 - r11 - r22);
                    qw = (r21 - r12) / s;
                    qx = 0.25f * s;
                    qy = (r01 + r10) / s;
                    qz = (r02 + r20) / s;
                }
                else if (r11 > r22)
                {
                    const float s = 2.f * std::sqrt(1.f + r11 - r00 - r22);
                    qw = (r02 - r20) / s;
                    qx = (r01 + r10) / s;
                    qy = 0.25f * s;
                    qz = (r12 + r21) / s;
                }
                else
                {
                    const float s = 2.f * std::sqrt(1.f + r22 - r00 - r11);
                    qw = (r10 - r01) / s;
                    qx = (r02 + r20) / s;
                    qy = (r12 + r21) / s;
                    qz = 0.25f * s;
                }

                node.transform.translation = {mat[12], mat[13], mat[14]};
                node.transform.rotation = Quat(qx, qy, qz, qw);
            }
            else
            {
                if (nj.contains("translation"))
                {
                    const auto& t = nj["translation"];
                    node.transform.translation = {
                        t[0].get<float>(),
                        t[1].get<float>(),
                        t[2].get<float>()
                    };
                }
                if (nj.contains("rotation"))
                {
                    const auto& r = nj["rotation"];
                    node.transform.rotation = {
                        r[0].get<float>(),
                        r[1].get<float>(),
                        r[2].get<float>(),
                        r[3].get<float>()
                    };
                }
                if (nj.contains("scale"))
                {
                    const auto& s = nj["scale"];
                    node.transform.scale = {
                        s[0].get<float>(),
                        s[1].get<float>(),
                        s[2].get<float>()
                    };
                }
            }
        }
    }

    // Root nodes from the default scene
    if (file.contains("scene") && file.contains("scenes"))
    {
        const size_t sceneIdx = file["scene"].get<size_t>();
        const auto& sceneJson = file["scenes"][sceneIdx];
        if (sceneJson.contains("nodes"))
            for (const auto& n : sceneJson["nodes"])
                model.rootNodes.push_back(n.get<int>());
    }
    else
    {
        // Fallback: every parentless node is a root
        for (size_t i = 0; i < model.nodes.size(); ++i)
            if (model.nodes[i].parent == -1)
                model.rootNodes.push_back(static_cast<int>(i));
    }

    return true;
}
