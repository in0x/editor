#version 450

const vec3 vertices[] = 
{
	vec3( 0.0,  0.5, 0.0),
	vec3( 0.5, -0.5, 0.0),
	vec3(-0.5, -0.5, 0.0),
};

const vec3 colors[] = 
{
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
};

layout(location = 0) out vec3 color;

layout(push_constant) uniform constants
{
	mat4 render_matrix;
} uniforms;

void main()
{
	color = colors[gl_VertexIndex];
	gl_Position = uniforms.render_matrix * vec4(vertices[gl_VertexIndex], 1.0);
}