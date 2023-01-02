#version 450

layout(location = 0) in vec3 vpos;
layout(location = 1) in vec3 vcol;

layout(location = 0) out vec3 fcol;

layout(push_constant) uniform constants
{
	mat4 mvp;
} uniforms;

void main()
{
	fcol = vcol;
	gl_Position = uniforms.mvp * vec4(vpos, 1.0);
}