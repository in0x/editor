#version 450

layout(location = 0) out vec4 outputColor;
layout(location = 0) in vec3 color;

void main()
{
	// outputColor = vec4(1.0, 0.64, 0.0, 1.0);
	float gamma = 2.2;
    vec3 correctedColor = pow(color, vec3(1.0/gamma));
	outputColor = vec4(correctedColor, 1.0);
}