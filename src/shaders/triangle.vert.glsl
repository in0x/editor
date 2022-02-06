#version 450

struct Vertex
{
	vec3 pos;
	vec3 nml;
};

layout(binding = 0) readonly buffer Vertices
{
	Vertex vertices[];
};

layout(location = 0) out vec4 color;

void main()
{
	Vertex v = vertices[gl_VertexIndex];
	gl_Position = vec4(v.pos + vec3(0, 0, 0.5), 1.0);

	color = vec4(v.nml * 0.5 + vec3(0.5), 1.0);
}