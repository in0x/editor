#version 450

layout(location = 0) out vec4 outputColor;
layout(location = 0) in vec3 color;

void main()
{
	// outputColor = vec4(1.0, 0.64, 0.0, 1.0);
	outputColor = vec4(color, 1.0);
}