/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "glbmodel.h"

#include "util/path.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cstring>
#include <functional>
#include <set>
#include <type_traits>


namespace gfx {


namespace {

const std::uint32_t MagicGlb = 0x46546C67;     // "glTF"
const std::uint32_t ChunkJson = 0x4E4F534A;    // "JSON"
const std::uint32_t ChunkBin = 0x004E4942;     // "BIN\0"

int componentCount(const std::string &type)
{
    if (type == "SCALAR")
        return 1;
    if (type == "VEC2")
        return 2;
    if (type == "VEC3")
        return 3;
    if (type == "VEC4")
        return 4;
    if (type == "MAT4")
        return 16;
    return 0;
}

int componentSize(int componentType)
{
    switch (componentType) {
    case 5120: case 5121: return 1;     // (unsigned) byte
    case 5122: case 5123: return 2;     // (unsigned) short
    case 5125: case 5126: return 4;     // unsigned int, float
    default: return 0;
    }
}

template <typename T>
T fromLittleEndian(const char *p)
{
    typename std::make_unsigned<T>::type bits = 0;
    for (std::size_t i = 0; i < sizeof(T); i++)
        bits |= typename std::make_unsigned<T>::type(std::uint8_t(p[i])) << (8*i);
    return T(bits);
}

// one component of an accessor element as float (normalized integers scaled to 0..1 or -1..1)
float component(const char *p, int componentType, bool normalized)
{
    switch (componentType) {
    case 5120: { std::int8_t v; std::memcpy(&v, p, 1); return normalized ? std::max(v/127.0f, -1.0f) : v; }
    case 5121: { std::uint8_t v; std::memcpy(&v, p, 1); return normalized ? v/255.0f : v; }
    case 5122: { const std::int16_t v = fromLittleEndian<std::int16_t>(p); return normalized ? std::max(v/32767.0f, -1.0f) : v; }
    case 5123: { const std::uint16_t v = fromLittleEndian<std::uint16_t>(p); return normalized ? v/65535.0f : v; }
    case 5125: return float(fromLittleEndian<std::uint32_t>(p));
    default: { const std::uint32_t v = fromLittleEndian<std::uint32_t>(p); float f; std::memcpy(&f, &v, 4); return f; }
    }
}

class Reader
{
public:
    Reader(const util::Json &json, const std::string &bin) : m_json(json), m_bin(bin) {}

    std::string bufferView(int index) const
    {
        const util::Json &view = m_json["bufferViews"][index];
        const std::int64_t offset = view["byteOffset"].toInt64();
        const std::int64_t length = view["byteLength"].toInt64();
        if (view["buffer"].toInt() != 0 || offset < 0 || length < 0 || offset + length > std::int64_t(m_bin.size()))
            return std::string();
        return m_bin.substr(std::size_t(offset), std::size_t(length));
    }

    // accessor elements with `components` floats each; false if it doesn't fit
    bool floats(int index, int components, std::vector<float> &out) const
    {
        const util::Json &accessor = m_json["accessors"][index];
        const int count = accessor["count"].toInt();
        const int type = accessor["componentType"].toInt();
        const int size = componentSize(type);
        const int n = componentCount(accessor["type"].toString());
        if (size == 0 || n < components || !accessor.contains("bufferView"))
            return false;
        const util::Json &view = m_json["bufferViews"][accessor["bufferView"].toInt()];
        const std::int64_t offset = view["byteOffset"].toInt64() + accessor["byteOffset"].toInt64();
        const int stride = view.contains("byteStride") ? view["byteStride"].toInt() : n*size;
        if (view["buffer"].toInt() != 0 || offset < 0 || (count > 0 && offset + std::int64_t(count - 1)*stride + n*size > std::int64_t(m_bin.size())))
            return false;
        const bool normalized = accessor["normalized"].toBool();
        out.resize(size_t(count)*components);
        for (int i = 0; i < count; i++) {
            const char *element = m_bin.data() + offset + std::int64_t(i)*stride;
            for (int c = 0; c < components; c++)
                out[size_t(i)*components + c] = component(element + c*size, type, normalized);
        }
        return true;
    }

    bool indices(int index, std::vector<std::uint32_t> &out) const
    {
        std::vector<float> values;
        if (!floats(index, 1, values))
            return false;
        // floats hold indices up to 2^24 exactly; read 32-bit ones directly
        const util::Json &accessor = m_json["accessors"][index];
        out.resize(values.size());
        if (accessor["componentType"].toInt() == 5125) {
            const util::Json &view = m_json["bufferViews"][accessor["bufferView"].toInt()];
            const std::int64_t offset = view["byteOffset"].toInt64() + accessor["byteOffset"].toInt64();
            const int stride = view.contains("byteStride") ? view["byteStride"].toInt() : 4;
            for (size_t i = 0; i < values.size(); i++)
                out[i] = fromLittleEndian<std::uint32_t>(m_bin.data() + offset + std::int64_t(i)*stride);
        } else {
            for (size_t i = 0; i < values.size(); i++)
                out[i] = std::uint32_t(values[i]);
        }
        return true;
    }

private:
    const util::Json &m_json;
    const std::string &m_bin;
};

}


bool GlbModel::fail(const std::string &error)
{
    m_error = error;
    m_meshes.clear();
    m_images.clear();
    return false;
}


bool GlbModel::load(const std::string &fileName)
{
    m_error.clear();
    m_meshes.clear();
    m_images.clear();

    std::vector<char> bytes;
    if (!util::readFile(fileName, bytes))
        return fail("cannot open " + fileName);
    const std::string data(bytes.begin(), bytes.end());
    auto u32 = [&data](std::size_t offset) { return fromLittleEndian<std::uint32_t>(data.data() + offset); };
    if (data.size() < 20 || u32(0) != MagicGlb || u32(4) != 2)
        return fail(fileName + " is not a binary glTF 2.0 file");

    // chunks: JSON first, then the binary buffer
    std::string jsonData, bin;
    for (std::size_t offset = 12; offset + 8 <= data.size(); ) {
        const std::uint32_t length = u32(offset);
        const std::uint32_t type = u32(offset + 4);
        if (offset + 8 + std::uint64_t(length) > data.size())
            return fail(fileName + " is truncated");
        if (type == ChunkJson && jsonData.empty())
            jsonData = data.substr(offset + 8, length);
        else if (type == ChunkBin && bin.empty())
            bin = data.substr(offset + 8, length);
        offset += 8 + std::size_t((std::uint64_t(length) + 3) & ~std::uint64_t(3));
    }
    std::string parseError;
    const util::Json json = util::Json::parse(jsonData, &parseError);
    if (!parseError.empty())
        return fail(fileName + ": " + parseError);
    const Reader reader(json, bin);

    for (const util::Json &image : json["images"].elements()) {
        Image decoded;
        if (image.contains("bufferView")) {
            const std::string data = reader.bufferView(image["bufferView"].toInt());
            decoded = Image::fromData(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
        }
        if (decoded.isNull())
            return fail(fileName + ": cannot decode image " + image["name"].toString() + " (" + image["mimeType"].toString() + ")");
        m_images.push_back(std::move(decoded));
    }

    const util::Json &textures = json["textures"];
    const util::Json &materials = json["materials"];
    const util::Json &meshes = json["meshes"];
    const util::Json &nodes = json["nodes"];

    std::set<int> visited;
    std::function<bool(int, const Matrix &)> visit = [&](int index, const Matrix &parent) {
        if (index < 0 || index >= int(nodes.size()) || !visited.insert(index).second)
            return false;
        const util::Json &node = nodes[index];
        Matrix local(1);
        if (node.contains("matrix")) {
            const util::Json &m = node["matrix"];
            for (int i = 0; i < 16; i++)
                glm::value_ptr(local)[i] = float(m[i].toDouble());
        } else {
            const util::Json &t = node["translation"];
            const util::Json &r = node["rotation"];
            const util::Json &s = node["scale"];
            if (t.size() == 3)
                local = glm::translate(Vector3D(t[0].toDouble(), t[1].toDouble(), t[2].toDouble()));
            if (r.size() == 4)
                local *= glm::mat4_cast(glm::quat(float(r[3].toDouble()), float(r[0].toDouble()), float(r[1].toDouble()), float(r[2].toDouble())));
            if (s.size() == 3)
                local *= glm::scale(Vector3D(s[0].toDouble(), s[1].toDouble(), s[2].toDouble()));
        }
        const Matrix world = parent*local;

        if (node.contains("mesh")) {
            for (const util::Json &primitive : meshes[node["mesh"].toInt()]["primitives"].elements()) {
                if (primitive["mode"].toInt(4) != 4)
                    continue;   // triangles only
                Mesh mesh;
                mesh.name = node["name"].toString();
                mesh.extras = node["extras"];
                int texCoord = 0;
                if (primitive.contains("material")) {
                    const util::Json &material = materials[primitive["material"].toInt()];
                    const util::Json &pbr = material["pbrMetallicRoughness"];
                    mesh.material = material["name"].toString();
                    mesh.blend = material["alphaMode"].toString() == "BLEND";
                    const util::Json &color = pbr["baseColorFactor"];
                    if (color.size() == 4)
                        mesh.color = Vector4D(color[0].toDouble(), color[1].toDouble(), color[2].toDouble(), color[3].toDouble());
                    if (pbr.contains("baseColorTexture")) {
                        const util::Json &texture = pbr["baseColorTexture"];
                        texCoord = texture["texCoord"].toInt();
                        mesh.image = textures[texture["index"].toInt()]["source"].toInt(-1);
                        if (mesh.image >= int(m_images.size()))
                            mesh.image = -1;
                    }
                }

                const util::Json &attributes = primitive["attributes"];
                std::vector<float> positions, uvs;
                if (!attributes.contains("POSITION") || !reader.floats(attributes["POSITION"].toInt(), 3, positions))
                    return fail(fileName + ": mesh " + mesh.name + " has no positions");
                const size_t count = positions.size()/3;
                mesh.positions.reserve(count);
                for (size_t i = 0; i < count; i++)
                    mesh.positions.push_back(Vector3D(world*Vector4D(positions[3*i], positions[3*i + 1], positions[3*i + 2], 1)));
                const std::string uvName = "TEXCOORD_" + std::to_string(texCoord);
                if (mesh.image >= 0 && attributes.contains(uvName) && reader.floats(attributes[uvName].toInt(), 2, uvs) && uvs.size() == 2*count) {
                    mesh.texCoords.reserve(count);
                    for (size_t i = 0; i < count; i++)
                        mesh.texCoords.push_back(Vector2D(uvs[2*i], uvs[2*i + 1]));
                } else {
                    mesh.image = -1;
                }

                if (primitive.contains("indices")) {
                    if (!reader.indices(primitive["indices"].toInt(), mesh.indices))
                        return fail(fileName + ": mesh " + mesh.name + " has broken indices");
                } else {
                    for (size_t i = 0; i < count; i++)
                        mesh.indices.push_back(std::uint32_t(i));
                }
                mesh.indices.resize(mesh.indices.size()/3*3);
                for (std::uint32_t i : mesh.indices)
                    if (i >= count)
                        return fail(fileName + ": mesh " + mesh.name + " has an index out of range");
                m_meshes.push_back(std::move(mesh));
            }
        }
        for (const util::Json &child : node["children"].elements())
            if (!visit(child.toInt(), world) && !m_error.empty())
                return false;
        return true;
    };

    const util::Json &scenes = json["scenes"];
    if (scenes.size() > 0) {
        for (const util::Json &root : scenes[json["scene"].toInt()]["nodes"].elements())
            if (!visit(root.toInt(), Matrix(1)) && !m_error.empty())
                return false;
    } else {
        // no scene: every node that isn't a child is a root
        std::set<int> children;
        for (const util::Json &node : nodes.elements())
            for (const util::Json &child : node["children"].elements())
                children.insert(child.toInt());
        for (int i = 0; i < int(nodes.size()); i++)
            if (children.count(i) == 0 && !visit(i, Matrix(1)) && !m_error.empty())
                return false;
    }

    if (m_meshes.empty())
        return fail(fileName + " has no triangle meshes");
    return true;
}


} // namespace gfx
