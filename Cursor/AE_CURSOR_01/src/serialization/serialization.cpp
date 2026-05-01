#include "serialization.h"

#include <sstream>

namespace aetdp1::serialization {
namespace {

static std::string Trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    return s;
}

static std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(Trim(line));
    }
    return lines;
}

static bool StartsWith(const std::string& s, const char* p) {
    const std::size_t n = std::char_traits<char>::length(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

static std::string AfterPrefix(const std::string& s, const char* prefix) {
    const std::size_t n = std::char_traits<char>::length(prefix);
    if (s.size() < n) {
        return {};
    }
    std::string rest = s.substr(n);
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
        rest.erase(rest.begin());
    }
    return rest;
}

} // namespace

std::string SaveTextDocument(const scene::SceneData& scene, const animation::AnimationDocument& anim) {
    std::ostringstream oss;
    oss << "AETDP1_TEXT_V1\n";

    oss << "SCENE_BEGIN\n";
    oss << "NEXT_ID " << scene.next_id << "\n";
    for (const auto& o : scene.objects) {
        oss << "OBJ_BEGIN " << o.id << "\n";
        oss << "NAME " << o.name << "\n";
        oss << "COLOR " << o.color.x << " " << o.color.y << " " << o.color.z << "\n";
        oss << "TRANS " << o.transform.translation.x << " " << o.transform.translation.y << " " << o.transform.translation.z << "\n";
        oss << "SCALE " << o.transform.scale.x << " " << o.transform.scale.y << " " << o.transform.scale.z << "\n";
        oss << "ROT " << o.transform.rotation.x << " " << o.transform.rotation.y << " " << o.transform.rotation.z << " " << o.transform.rotation.w
            << "\n";
        oss << "SEL " << static_cast<int>(o.selected_vertex_index) << "\n";
        for (int fi = 0; fi < 6; ++fi) {
            const auto& fc = o.face_colors.rgb[static_cast<std::size_t>(fi)];
            oss << "FACE " << fi << " " << fc.x << " " << fc.y << " " << fc.z << "\n";
        }
        for (int i = 0; i < 8; ++i) {
            const auto& v = o.local_box.v[i];
            oss << "V " << v.x << " " << v.y << " " << v.z << "\n";
        }
        oss << "OBJ_END\n";
    }
    oss << "SCENE_END\n";

    oss << "ANIM_BEGIN\n";
    oss << "DUR " << anim.duration_seconds << "\n";
    oss << "TIME " << anim.current_time_seconds << "\n";
    oss << "PLAYING " << (anim.playback.playing ? 1 : 0) << "\n";
    oss << "PAUSED " << (anim.playback.paused ? 1 : 0) << "\n";
    for (const auto& tr : anim.tracks) {
        oss << "TRACK " << tr.target << " " << tr.keys.size() << "\n";
        for (const auto& k : tr.keys) {
            const auto& t = k.value.translation;
            const auto& s = k.value.scale;
            const auto& r = k.value.rotation;
            oss << "KEY " << k.time_seconds << " ";
            oss << t.x << " " << t.y << " " << t.z << " ";
            oss << s.x << " " << s.y << " " << s.z << " ";
            oss << r.x << " " << r.y << " " << r.z << " " << r.w << "\n";
        }
    }
    oss << "ANIM_END\n";

    return oss.str();
}

LoadResult LoadTextDocument(const std::string& text, scene::SceneData& out_scene, animation::AnimationDocument& out_anim) {
    LoadResult r{};
    const auto lines = SplitLines(text);
    std::size_t i = 0;

    auto fail = [&](const char* msg) {
        r.ok = false;
        r.error = msg;
    };

    if (lines.empty() || lines[0] != "AETDP1_TEXT_V1") {
        fail("Missing header AETDP1_TEXT_V1");
        return r;
    }
    i = 1;

    out_scene.objects.clear();
    out_anim.tracks.clear();

    if (i >= lines.size() || lines[i] != "SCENE_BEGIN") {
        fail("Expected SCENE_BEGIN");
        return r;
    }
    ++i;

    if (i >= lines.size() || !StartsWith(lines[i], "NEXT_ID")) {
        fail("Expected NEXT_ID");
        return r;
    }
    {
        std::istringstream iss(lines[i++]);
        std::string tag;
        iss >> tag >> out_scene.next_id;
    }

    while (i < lines.size() && lines[i] != "SCENE_END") {
        if (i >= lines.size() || !StartsWith(lines[i], "OBJ_BEGIN")) {
            fail("Expected OBJ_BEGIN");
            return r;
        }
        scene::ObjectData o{};
        {
            std::istringstream iss(lines[i++]);
            std::string tag;
            iss >> tag >> o.id;
        }

        if (i >= lines.size() || !StartsWith(lines[i], "NAME")) {
            fail("Expected NAME");
            return r;
        }
        o.name = AfterPrefix(lines[i++], "NAME");

        auto read3 = [&](const char* prefix, DirectX::XMFLOAT3& v) -> bool {
            if (i >= lines.size() || !StartsWith(lines[i], prefix)) {
                fail("Expected vector line");
                return false;
            }
            std::istringstream iss(lines[i++]);
            std::string tag;
            iss >> tag >> v.x >> v.y >> v.z;
            return true;
        };

        if (!read3("COLOR", o.color)) {
            return r;
        }
        if (!read3("TRANS", o.transform.translation)) {
            return r;
        }
        if (!read3("SCALE", o.transform.scale)) {
            return r;
        }
        if (i >= lines.size() || !StartsWith(lines[i], "ROT")) {
            fail("Expected ROT");
            return r;
        }
        {
            std::istringstream iss(lines[i++]);
            std::string tag;
            iss >> tag >> o.transform.rotation.x >> o.transform.rotation.y >> o.transform.rotation.z >> o.transform.rotation.w;
        }
        if (i >= lines.size() || !StartsWith(lines[i], "SEL")) {
            fail("Expected SEL");
            return r;
        }
        {
            std::istringstream iss(lines[i++]);
            std::string tag;
            int sel = 0;
            iss >> tag >> sel;
            o.selected_vertex_index = static_cast<std::uint8_t>(sel);
        }
        for (int fi = 0; fi < 6; ++fi) {
            if (i >= lines.size() || !StartsWith(lines[i], "FACE")) {
                fail("Expected FACE line");
                return r;
            }
            std::istringstream fss(lines[i++]);
            std::string ftag;
            int idx = -1;
            fss >> ftag >> idx >> o.face_colors.rgb[static_cast<std::size_t>(fi)].x >> o.face_colors.rgb[static_cast<std::size_t>(fi)].y >>
                o.face_colors.rgb[static_cast<std::size_t>(fi)].z;
            if (idx != fi) {
                fail("FACE index mismatch");
                return r;
            }
        }

        for (int vi = 0; vi < 8; ++vi) {
            if (i >= lines.size() || !StartsWith(lines[i], "V")) {
                fail("Expected V line");
                return r;
            }
            std::istringstream vss(lines[i++]);
            std::string vtag;
            vss >> vtag >> o.local_box.v[vi].x >> o.local_box.v[vi].y >> o.local_box.v[vi].z;
        }

        if (i >= lines.size() || lines[i] != "OBJ_END") {
            fail("Expected OBJ_END");
            return r;
        }
        ++i;

        out_scene.objects.push_back(std::move(o));
    }

    if (i >= lines.size() || lines[i] != "SCENE_END") {
        fail("Expected SCENE_END");
        return r;
    }
    ++i;

    if (i >= lines.size() || lines[i] != "ANIM_BEGIN") {
        fail("Expected ANIM_BEGIN");
        return r;
    }
    ++i;

    while (i < lines.size() && lines[i] != "ANIM_END") {
        std::istringstream iss(lines[i++]);
        std::string tag;
        iss >> tag;
        if (tag == "DUR") {
            iss >> out_anim.duration_seconds;
        } else if (tag == "TIME") {
            iss >> out_anim.current_time_seconds;
        } else if (tag == "PLAYING") {
            int v = 0;
            iss >> v;
            out_anim.playback.playing = v != 0;
        } else if (tag == "PAUSED") {
            int v = 0;
            iss >> v;
            out_anim.playback.paused = v != 0;
        } else if (tag == "TRACK") {
            animation::TransformTrack tr{};
            std::size_t key_count = 0;
            iss >> tr.target >> key_count;
            tr.keys.reserve(key_count);
            for (std::size_t k = 0; k < key_count; ++k) {
                if (i >= lines.size()) {
                    fail("Unexpected EOF reading keys");
                    return r;
                }
                animation::TransformKeyframe key{};
                std::istringstream kss(lines[i++]);
                std::string ktag;
                kss >> ktag;
                if (ktag != "KEY") {
                    fail("Expected KEY");
                    return r;
                }
                kss >> key.time_seconds;
                kss >> key.value.translation.x >> key.value.translation.y >> key.value.translation.z;
                kss >> key.value.scale.x >> key.value.scale.y >> key.value.scale.z;
                kss >> key.value.rotation.x >> key.value.rotation.y >> key.value.rotation.z >> key.value.rotation.w;
                tr.keys.push_back(key);
            }
            out_anim.tracks.push_back(std::move(tr));
        } else {
            fail("Unknown ANIM token");
            return r;
        }
    }

    if (i >= lines.size() || lines[i] != "ANIM_END") {
        fail("Expected ANIM_END");
        return r;
    }

    r.ok = true;
    return r;
}

} // namespace aetdp1::serialization
