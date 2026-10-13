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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cstring>
#include <functional>
#include <set>


namespace gfx {


namespace {

const quint32 MagicGlb = 0x46546C67;     // "glTF"
const quint32 ChunkJson = 0x4E4F534A;    // "JSON"
const quint32 ChunkBin = 0x004E4942;     // "BIN\0"

int componentCount(const QString &type)
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

// one component of an accessor element as float (normalized integers scaled to 0..1 or -1..1)
float component(const char *p, int componentType, bool normalized)
{
    switch (componentType) {
    case 5120: { qint8 v; std::memcpy(&v, p, 1); return normalized ? std::max(v/127.0f, -1.0f) : v; }
    case 5121: { quint8 v; std::memcpy(&v, p, 1); return normalized ? v/255.0f : v; }
    case 5122: { const qint16 v = qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(p)); return normalized ? std::max(v/32767.0f, -1.0f) : v; }
    case 5123: { const quint16 v = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(p)); return normalized ? v/65535.0f : v; }
    case 5125: return float(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(p)));
    default: { const quint32 v = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(p)); float f; std::memcpy(&f, &v, 4); return f; }
    }
}

class Reader
{
public:
    Reader(const QJsonObject &json, const QByteArray &bin) : m_json(json), m_bin(bin) {}

    QByteArray bufferView(int index) const
    {
        const QJsonObject view = m_json["bufferViews"].toArray()[index].toObject();
        const qint64 offset = view["byteOffset"].toVariant().toLongLong();
        const qint64 length = view["byteLength"].toVariant().toLongLong();
        if (view["buffer"].toInt() != 0 || offset < 0 || length < 0 || offset + length > m_bin.size())
            return QByteArray();
        return m_bin.mid(int(offset), int(length));
    }

    // accessor elements with `components` floats each; false if it doesn't fit
    bool floats(int index, int components, std::vector<float> &out) const
    {
        const QJsonObject accessor = m_json["accessors"].toArray()[index].toObject();
        const int count = accessor["count"].toInt();
        const int type = accessor["componentType"].toInt();
        const int size = componentSize(type);
        const int n = componentCount(accessor["type"].toString());
        if (size == 0 || n < components || !accessor.contains("bufferView"))
            return false;
        const QJsonObject view = m_json["bufferViews"].toArray()[accessor["bufferView"].toInt()].toObject();
        const qint64 offset = view["byteOffset"].toVariant().toLongLong() + accessor["byteOffset"].toVariant().toLongLong();
        const int stride = view.contains("byteStride") ? view["byteStride"].toInt() : n*size;
        if (view["buffer"].toInt() != 0 || offset < 0 || (count > 0 && offset + qint64(count - 1)*stride + n*size > m_bin.size()))
            return false;
        const bool normalized = accessor["normalized"].toBool();
        out.resize(size_t(count)*components);
        for (int i = 0; i < count; i++) {
            const char *element = m_bin.constData() + offset + qint64(i)*stride;
            for (int c = 0; c < components; c++)
                out[size_t(i)*components + c] = component(element + c*size, type, normalized);
        }
        return true;
    }

    bool indices(int index, std::vector<quint32> &out) const
    {
        std::vector<float> values;
        if (!floats(index, 1, values))
            return false;
        // floats hold indices up to 2^24 exactly; read 32-bit ones directly
        const QJsonObject accessor = m_json["accessors"].toArray()[index].toObject();
        out.resize(values.size());
        if (accessor["componentType"].toInt() == 5125) {
            const QJsonObject view = m_json["bufferViews"].toArray()[accessor["bufferView"].toInt()].toObject();
            const qint64 offset = view["byteOffset"].toVariant().toLongLong() + accessor["byteOffset"].toVariant().toLongLong();
            const int stride = view.contains("byteStride") ? view["byteStride"].toInt() : 4;
            for (size_t i = 0; i < values.size(); i++)
                out[i] = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(m_bin.constData() + offset + qint64(i)*stride));
        } else {
            for (size_t i = 0; i < values.size(); i++)
                out[i] = quint32(values[i]);
        }
        return true;
    }

private:
    const QJsonObject &m_json;
    const QByteArray &m_bin;
};

}


bool GlbModel::fail(const QString &error)
{
    m_error = error;
    m_meshes.clear();
    m_images.clear();
    return false;
}


bool GlbModel::load(const QString &fileName)
{
    m_error.clear();
    m_meshes.clear();
    m_images.clear();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return fail(QString("cannot open %1").arg(fileName));
    const QByteArray data = file.readAll();
    auto u32 = [&data](int offset) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset)); };
    if (data.size() < 20 || u32(0) != MagicGlb || u32(4) != 2)
        return fail(QString("%1 is not a binary glTF 2.0 file").arg(fileName));

    // chunks: JSON first, then the binary buffer
    QByteArray jsonData, bin;
    for (int offset = 12; offset + 8 <= data.size(); ) {
        const quint32 length = u32(offset);
        const quint32 type = u32(offset + 4);
        if (offset + 8 + qint64(length) > data.size())
            return fail(QString("%1 is truncated").arg(fileName));
        if (type == ChunkJson && jsonData.isEmpty())
            jsonData = data.mid(offset + 8, int(length));
        else if (type == ChunkBin && bin.isEmpty())
            bin = data.mid(offset + 8, int(length));
        offset += 8 + int((length + 3) & ~3u);
    }
    QJsonParseError parseError;
    const QJsonObject json = QJsonDocument::fromJson(jsonData, &parseError).object();
    if (parseError.error != QJsonParseError::NoError)
        return fail(QString("%1: %2").arg(fileName, parseError.errorString()));
    const Reader reader(json, bin);

    const QJsonArray images = json["images"].toArray();
    for (const QJsonValue &value : images) {
        const QJsonObject image = value.toObject();
        QImage decoded;
        if (image.contains("bufferView"))
            decoded = QImage::fromData(reader.bufferView(image["bufferView"].toInt()));
        if (decoded.isNull())
            return fail(QString("%1: cannot decode image %2 (%3)").arg(fileName, image["name"].toString(), image["mimeType"].toString()));
        m_images.push_back(decoded);
    }

    const QJsonArray textures = json["textures"].toArray();
    const QJsonArray materials = json["materials"].toArray();
    const QJsonArray meshes = json["meshes"].toArray();
    const QJsonArray nodes = json["nodes"].toArray();

    std::set<int> visited;
    std::function<bool(int, const Matrix &)> visit = [&](int index, const Matrix &parent) {
        if (index < 0 || index >= nodes.size() || !visited.insert(index).second)
            return false;
        const QJsonObject node = nodes[index].toObject();
        Matrix local(1);
        if (node.contains("matrix")) {
            const QJsonArray m = node["matrix"].toArray();
            for (int i = 0; i < 16; i++)
                glm::value_ptr(local)[i] = float(m[i].toDouble());
        } else {
            const QJsonArray t = node["translation"].toArray();
            const QJsonArray r = node["rotation"].toArray();
            const QJsonArray s = node["scale"].toArray();
            if (t.size() == 3)
                local = glm::translate(Vector3D(t[0].toDouble(), t[1].toDouble(), t[2].toDouble()));
            if (r.size() == 4)
                local *= glm::mat4_cast(glm::quat(float(r[3].toDouble()), float(r[0].toDouble()), float(r[1].toDouble()), float(r[2].toDouble())));
            if (s.size() == 3)
                local *= glm::scale(Vector3D(s[0].toDouble(), s[1].toDouble(), s[2].toDouble()));
        }
        const Matrix world = parent*local;

        if (node.contains("mesh")) {
            const QJsonArray primitives = meshes[node["mesh"].toInt()].toObject()["primitives"].toArray();
            for (const QJsonValue &value : primitives) {
                const QJsonObject primitive = value.toObject();
                if (primitive["mode"].toInt(4) != 4)
                    continue;   // triangles only
                Mesh mesh;
                mesh.name = node["name"].toString();
                mesh.extras = node["extras"].toObject().toVariantMap();
                int texCoord = 0;
                if (primitive.contains("material")) {
                    const QJsonObject material = materials[primitive["material"].toInt()].toObject();
                    const QJsonObject pbr = material["pbrMetallicRoughness"].toObject();
                    mesh.material = material["name"].toString();
                    mesh.blend = material["alphaMode"].toString() == "BLEND";
                    const QJsonArray color = pbr["baseColorFactor"].toArray();
                    if (color.size() == 4)
                        mesh.color = Vector4D(color[0].toDouble(), color[1].toDouble(), color[2].toDouble(), color[3].toDouble());
                    if (pbr.contains("baseColorTexture")) {
                        const QJsonObject texture = pbr["baseColorTexture"].toObject();
                        texCoord = texture["texCoord"].toInt();
                        mesh.image = textures[texture["index"].toInt()].toObject()["source"].toInt(-1);
                        if (mesh.image >= int(m_images.size()))
                            mesh.image = -1;
                    }
                }

                const QJsonObject attributes = primitive["attributes"].toObject();
                std::vector<float> positions, uvs;
                if (!attributes.contains("POSITION") || !reader.floats(attributes["POSITION"].toInt(), 3, positions))
                    return fail(QString("%1: mesh %2 has no positions").arg(fileName, mesh.name));
                const size_t count = positions.size()/3;
                mesh.positions.reserve(count);
                for (size_t i = 0; i < count; i++)
                    mesh.positions.push_back(Vector3D(world*Vector4D(positions[3*i], positions[3*i + 1], positions[3*i + 2], 1)));
                const QString uvName = QString("TEXCOORD_%1").arg(texCoord);
                if (mesh.image >= 0 && attributes.contains(uvName) && reader.floats(attributes[uvName].toInt(), 2, uvs) && uvs.size() == 2*count) {
                    mesh.texCoords.reserve(count);
                    for (size_t i = 0; i < count; i++)
                        mesh.texCoords.push_back(Vector2D(uvs[2*i], uvs[2*i + 1]));
                } else {
                    mesh.image = -1;
                }

                if (primitive.contains("indices")) {
                    if (!reader.indices(primitive["indices"].toInt(), mesh.indices))
                        return fail(QString("%1: mesh %2 has broken indices").arg(fileName, mesh.name));
                } else {
                    for (size_t i = 0; i < count; i++)
                        mesh.indices.push_back(quint32(i));
                }
                mesh.indices.resize(mesh.indices.size()/3*3);
                for (quint32 i : mesh.indices)
                    if (i >= count)
                        return fail(QString("%1: mesh %2 has an index out of range").arg(fileName, mesh.name));
                m_meshes.push_back(std::move(mesh));
            }
        }
        for (const QJsonValue &child : node["children"].toArray())
            if (!visit(child.toInt(), world) && !m_error.isEmpty())
                return false;
        return true;
    };

    const QJsonArray scenes = json["scenes"].toArray();
    if (!scenes.isEmpty()) {
        for (const QJsonValue &root : scenes[json["scene"].toInt()].toObject()["nodes"].toArray())
            if (!visit(root.toInt(), Matrix(1)) && !m_error.isEmpty())
                return false;
    } else {
        // no scene: every node that isn't a child is a root
        std::set<int> children;
        for (const QJsonValue &node : nodes)
            for (const QJsonValue &child : node.toObject()["children"].toArray())
                children.insert(child.toInt());
        for (int i = 0; i < nodes.size(); i++)
            if (children.count(i) == 0 && !visit(i, Matrix(1)) && !m_error.isEmpty())
                return false;
    }

    if (m_meshes.empty())
        return fail(QString("%1 has no triangle meshes").arg(fileName));
    return true;
}


} // namespace gfx
