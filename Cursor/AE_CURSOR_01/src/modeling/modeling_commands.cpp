#include "modeling_commands.h"

#include <sstream>

namespace aetdp1::commands {
namespace {

std::string UniqueName(scene::SceneData& scene, const char* prefix) {
    int i = 1;
    for (;;) {
        std::ostringstream oss;
        oss << prefix << i;
        const std::string candidate = oss.str();
        bool clash = false;
        for (const auto& o : scene.objects) {
            if (o.name == candidate) {
                clash = true;
                break;
            }
        }
        if (!clash) {
            return candidate;
        }
        ++i;
    }
}

} // namespace

scene::ObjectData MakeDefaultNamedObject(scene::SceneData& scene, const std::string& desired_name) {
    scene::ObjectData obj{};
    obj.id = scene.AllocateId();

    bool name_free = true;
    for (const auto& o : scene.objects) {
        if (o.name == desired_name) {
            name_free = false;
            break;
        }
    }
    obj.name = name_free ? desired_name : UniqueName(scene, "Object");

    obj.color = {0.55f, 0.65f, 0.95f};
    obj.transform.translation = {0.f, 0.5f, 0.f};
    obj.transform.scale = {1.f, 1.f, 1.f};
    obj.transform.rotation = {0.f, 0.f, 0.f, 1.f};
    scene::SceneData::EnsureDefaultBoxCorners(obj);
    return obj;
}

} // namespace aetdp1::commands
