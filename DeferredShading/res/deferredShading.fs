#version 450 core

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct Light
{
	vec3 Position;
	vec3 Color;

	float Linear;
	float Quadratic;
};

const int NUM_LIGHTS = 16;
uniform Light lights[NUM_LIGHTS];
uniform vec3 viewPos;

in vec2 TexCoords;

out vec4 FragColor;

void main()
{
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	vec3 Normal = texture(gNormal, TexCoords).rgb;
	vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
	float Specular = texture(gAlbedoSpec, TexCoords).a;

	// Calculate lighting
	vec3 ambient = Diffuse * 0.1;
	vec3 color = ambient;
	vec3 viewDir = normalize(viewPos - FragPos);
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		// 1. diffuse
		vec3 lightDir = normalize(lights[i].Position - FragPos);
		vec3 diffuse = clamp(dot(Normal, lightDir), 0.0, 1.0) * Diffuse * lights[i].Color;
		// 2. Specular
		vec3 halfVec = normalize(lightDir + viewDir);
		float spec = pow(clamp(dot(Normal, halfVec), 0.0, 1.0), 16.0);
		vec3 specular = lights[i].Color * spec;
		// attenuation
		float distance = length(lights[i].Position - FragPos);
		float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance);
		// diffuse *= attenuation;
		// specular *= attenuation;
		color += diffuse + specular;
	}
	FragColor = vec4(color/NUM_LIGHTS, 1.0);
	// FragColor = vec4(FragPos, 1.0);
}