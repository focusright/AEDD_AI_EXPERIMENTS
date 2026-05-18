#include "rig_serializer.h"

#include <fstream>
#include <sstream>
#include <string>

#include "skeleton.h"

namespace aerigp1 {
namespace {

static std::string Trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return s.substr(a, b - a);
}

static std::vector<std::string> ReadLines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

static bool ParseFloat3(const std::string& s, DirectX::XMFLOAT3& out) {
    std::istringstream iss(s);
    return static_cast<bool>(iss >> out.x >> out.y >> out.z);
}

static bool ParseFloat4(const std::string& s, DirectX::XMFLOAT4& out) {
    std::istringstream iss(s);
    return static_cast<bool>(iss >> out.x >> out.y >> out.z >> out.w);
}

} // namespace

bool RigSerializer::SaveRig(const RigDocument& doc, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "AERIG_V1\n";
    out << "joint_count " << doc.skeleton.joints.size() << "\n";
    for (const Joint& j : doc.skeleton.joints) {
        out << "joint " << j.name << " " << j.parent << " ";
        out << j.local_bind.position.x << " " << j.local_bind.position.y << " " << j.local_bind.position.z << " ";
        out << j.local_bind.rotation.x << " " << j.local_bind.rotation.y << " " << j.local_bind.rotation.z << " " << j.local_bind.rotation.w << " ";
        out << j.local_bind.scale.x << " " << j.local_bind.scale.y << " " << j.local_bind.scale.z << "\n";
    }
    out << "vertex_count " << doc.mesh.vertices.size() << "\n";
    for (const Vertex& v : doc.mesh.vertices) {
        out << "vertex " << v.position.x << " " << v.position.y << " " << v.position.z << " ";
        out << v.normal.x << " " << v.normal.y << " " << v.normal.z << " ";
        out << v.color.x << " " << v.color.y << " " << v.color.z << " " << v.color.w << " ";
        for (int i = 0; i < kMaxSkinInfluences; ++i) {
            out << v.skin.joint_indices[i] << " " << v.skin.weights[i] << " ";
        }
        out << "\n";
    }
    out << "index_count " << doc.mesh.indices.size() << "\n";
    for (std::uint32_t idx : doc.mesh.indices) {
        out << "index " << idx << "\n";
    }
    out << "end\n";
    return true;
}

LoadResult RigSerializer::LoadRig(RigDocument& doc, const std::string& path) {
    LoadResult result{};
    const std::vector<std::string> lines = ReadLines(path);
    if (lines.empty() || Trim(lines[0]) != "AERIG_V1") {
        result.error = "Missing AERIG_V1 header.";
        return result;
    }

    doc = RigDocument{};
    std::size_t li = 1;
    while (li < lines.size()) {
        const std::string line = Trim(lines[li++]);
        if (line.empty() || line == "end") {
            break;
        }
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "joint_count") {
            continue;
        }
        if (tag == "joint") {
            Joint j{};
            iss >> j.name >> j.parent;
            iss >> j.local_bind.position.x >> j.local_bind.position.y >> j.local_bind.position.z;
            iss >> j.local_bind.rotation.x >> j.local_bind.rotation.y >> j.local_bind.rotation.z >> j.local_bind.rotation.w;
            iss >> j.local_bind.scale.x >> j.local_bind.scale.y >> j.local_bind.scale.z;
            j.local_pose = j.local_bind;
            doc.skeleton.joints.push_back(j);
            continue;
        }
        if (tag == "vertex_count") {
            continue;
        }
        if (tag == "vertex") {
            Vertex v{};
            iss >> v.position.x >> v.position.y >> v.position.z;
            iss >> v.normal.x >> v.normal.y >> v.normal.z;
            iss >> v.color.x >> v.color.y >> v.color.z >> v.color.w;
            for (int i = 0; i < kMaxSkinInfluences; ++i) {
                iss >> v.skin.joint_indices[i] >> v.skin.weights[i];
            }
            doc.mesh.vertices.push_back(v);
            continue;
        }
        if (tag == "index_count") {
            continue;
        }
        if (tag == "index") {
            std::uint32_t idx = 0;
            iss >> idx;
            doc.mesh.indices.push_back(idx);
        }
    }

    SkeletonRecomputeInverseBind(doc.skeleton);
    SkeletonCopyPoseFromBind(doc.skeleton);
    doc.skinned_vertices = doc.mesh.vertices;
    doc.bind_pose.locals.resize(doc.skeleton.joints.size());
    for (std::size_t i = 0; i < doc.skeleton.joints.size(); ++i) {
        doc.bind_pose.locals[i] = doc.skeleton.joints[i].local_bind;
    }
    result.ok = true;
    return result;
}

bool RigSerializer::SaveAnim(const RigDocument& doc, int clip_index, const std::string& path) {
    if (clip_index < 0 || clip_index >= static_cast<int>(doc.clips.size())) {
        return false;
    }
    const AnimClip& clip = doc.clips[static_cast<std::size_t>(clip_index)];
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "AEANIM_V1\n";
    out << "name " << clip.name << "\n";
    out << "fps " << clip.fps << "\n";
    out << "frame_count " << clip.frame_count << "\n";
    out << "track_count " << clip.tracks.size() << "\n";
    for (const AnimTrack& tr : clip.tracks) {
        out << "track " << tr.joint_index << " " << tr.keys.size() << "\n";
        for (const AnimKey& k : tr.keys) {
            out << "key " << k.frame << " ";
            out << k.local.position.x << " " << k.local.position.y << " " << k.local.position.z << " ";
            out << k.local.rotation.x << " " << k.local.rotation.y << " " << k.local.rotation.z << " " << k.local.rotation.w << " ";
            out << k.local.scale.x << " " << k.local.scale.y << " " << k.local.scale.z << "\n";
        }
    }
    out << "end\n";
    return true;
}

LoadResult RigSerializer::LoadAnim(RigDocument& doc, const std::string& path) {
    LoadResult result{};
    const std::vector<std::string> lines = ReadLines(path);
    if (lines.empty() || Trim(lines[0]) != "AEANIM_V1") {
        result.error = "Missing AEANIM_V1 header.";
        return result;
    }

    AnimClip clip{};
    AnimTrack* cur_track = nullptr;
    std::size_t li = 1;
    while (li < lines.size()) {
        const std::string line = Trim(lines[li++]);
        if (line.empty() || line == "end") {
            break;
        }
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "name") {
            iss >> clip.name;
            continue;
        }
        if (tag == "fps") {
            iss >> clip.fps;
            continue;
        }
        if (tag == "frame_count") {
            iss >> clip.frame_count;
            continue;
        }
        if (tag == "track_count") {
            continue;
        }
        if (tag == "track") {
            AnimTrack tr{};
            int key_count = 0;
            iss >> tr.joint_index >> key_count;
            clip.tracks.push_back(tr);
            cur_track = &clip.tracks.back();
            continue;
        }
        if (tag == "key" && cur_track) {
            AnimKey k{};
            iss >> k.frame;
            iss >> k.local.position.x >> k.local.position.y >> k.local.position.z;
            iss >> k.local.rotation.x >> k.local.rotation.y >> k.local.rotation.z >> k.local.rotation.w;
            iss >> k.local.scale.x >> k.local.scale.y >> k.local.scale.z;
            cur_track->keys.push_back(k);
        }
    }

    if (doc.clips.empty()) {
        doc.clips.push_back(clip);
        doc.active_clip = 0;
    } else {
        doc.clips[static_cast<std::size_t>(doc.active_clip)] = clip;
    }
    result.ok = true;
    return result;
}

} // namespace aerigp1
