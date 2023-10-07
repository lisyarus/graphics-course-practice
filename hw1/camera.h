#pragma once

#include <cmath>
#include <vector>

class Camera {
public:
    Camera(float w, float h);

    const std::vector<float> &GetView() const;
    const std::vector<float> &GetProjection() const;

    void SetZoom(float new_zoom);
    void UpdateTop(float w, float h);

private:
    const float near = 0.01,
                far = 1000,
                ang = 45.f * M_PIl / 180.f,
                right = near * std::tan(ang);

    float top = 0;
    
    float zoom;

    std::vector<float> view;
    std::vector<float> projection;
};