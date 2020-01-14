#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;

void main()
{
	vec4 worldPos = u_model * vec4(position, 1.0);
	FragPos = worldPos.xyz;
	TexCoords = texCoord;
	Normal = normalize(transpose(inverse(mat3(u_model))) * normal);

	gl_Position = u_projection * u_view * worldPos;
}