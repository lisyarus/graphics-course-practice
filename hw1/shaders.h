#pragma once

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform float time;

layout (location = 0) in vec2 in_position;

out float p;

float func(float x, float z, float time) {
	return (sin(x + time * 10 - z) + cos(x + z * cos(time) * sin(time)));
}

void main()
{
	p = func(in_position.x, in_position.y, time);
	gl_Position = projection * view * model * vec4(in_position.x, p, in_position.y, 1.0);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform bool draw_isoline;

in float p;

layout (location = 0) out vec4 out_color;

float C1 = 0.5;
float C2 = 1;
float C3 = 1.5;
float C4 = -0.5;
float C5 = -1;
float C6 = -1.5;

float EPS = 0.03;

void main()
{
	float c = (p + 2) / 4 * 255;
	out_color = vec4(p / 2, 1.0 - p, 1.0 - sin(p), 1.0);

	if (draw_isoline) {
		if (abs(p - C1) < EPS || abs(p - C2) < EPS || abs(p - C3) < EPS || 
				abs(p - C4) < EPS || abs(p - C5) < EPS || abs(p - C6) < EPS) 
					out_color = vec4(0.2, 0.2, 0.2, 1.0);
	}
}
)";