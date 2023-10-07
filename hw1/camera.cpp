#include "camera.h"

Camera::Camera(float w, float h) {
    top = right / ((float) w / h);

    view = {
		0.7071, 0, -0.7071, 0,
		-0.5, 0.7071, -0.5, 0,
		0.5, 0.7071, 0.5, -70,
		0, 0, 0, 1
	};

    projection = {
		near / right, 0.f, 0.f, 0.f,
		0.f, near / top, 0.f, 0.f,
		0.f, 0.f, -1 * (far + near) / (far - near), 
									-2 * (far * near) / (far - near),
		0.f, 0.f, -1.f, 0.f,
	};
}

const std::vector<float> &Camera::GetView() const {
    return view;
}

const std::vector<float> &Camera::GetProjection() const {
    return projection;
}

void Camera::SetZoom(float new_zoom) {
    view[11] -= zoom;
    zoom = new_zoom;
    view[11] += zoom;
}

void Camera::UpdateTop(float w, float h) {
    top = right / ((float) w / h);
}
