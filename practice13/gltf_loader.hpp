#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <algorithm>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>

struct gltf_model
{
    struct buffer_view
    {
        unsigned int offset;
        unsigned int size;
    };

    struct accessor
    {
        buffer_view view;
        unsigned int type;
        unsigned int size;
        unsigned int count;
    };

    struct material
    {
        bool two_sided;
        bool transparent;
        std::optional<std::string> texture_path;
        std::optional<glm::vec4> color;
    };

    struct bone
    {
        unsigned int parent = -1;
        std::string name;
        glm::mat4 inverse_bind_matrix;
    };

    template <typename T>
    struct spline
    {
        std::vector<float> timestamps;
        std::vector<T> values;

        T operator()(float time) const;
    };

    struct bone_animation
    {
        spline<glm::vec3> translation;
        spline<glm::quat> rotation;
        spline<glm::vec3> scale;
    };

    struct animation
    {
        std::vector<bone_animation> bones;
        float max_time = 0.f;
    };

    struct mesh
    {
        std::string name;
        struct material material;

        accessor indices;

        accessor position;
        accessor normal;
        accessor texcoord;
        accessor joints;
        accessor weights;
    };

    std::vector<char> buffer;
    std::vector<mesh> meshes;
    std::vector<bone> bones;
    std::unordered_map<std::string, animation> animations;
};

gltf_model load_gltf(std::filesystem::path const & path);

template <>
inline glm::vec3 gltf_model::spline<glm::vec3>::operator()(float time) const
{
    assert(!values.empty());

    auto it = std::lower_bound(timestamps.begin(), timestamps.end(), time);
    if (it == timestamps.begin())
        return values.back();
    if (it == timestamps.end())
        return values.back();

    int i = it - timestamps.begin();

    float t = (time - timestamps[i - 1]) / (timestamps[i] - timestamps[i - 1]);
    return glm::lerp(values[i - 1], values[i], t);
}

template <>
inline glm::quat gltf_model::spline<glm::quat>::operator()(float time) const
{
    assert(!values.empty());

    auto it = std::lower_bound(timestamps.begin(), timestamps.end(), time);
    if (it == timestamps.begin())
        return values.back();
    if (it == timestamps.end())
        return values.back();

    int i = it - timestamps.begin();

    float t = (time - timestamps[i - 1]) / (timestamps[i] - timestamps[i - 1]);
    return glm::slerp(values[i - 1], values[i], t);
}
