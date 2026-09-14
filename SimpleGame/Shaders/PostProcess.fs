#version 330

in vec2 v_UV;
layout(location = 0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform vec2 u_Resolution;
uniform float u_Time;
uniform vec3 u_Lights[16];

float randomNoise(vec2 value)
{
	return fract(sin(dot(value, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	vec2 centerOffset = v_UV - vec2(0.5);
	float edge = length(centerOffset);
	vec2 chroma = centerOffset * 1.3 / u_Resolution;

	float red = texture(u_Scene, v_UV + chroma).r;
	float green = texture(u_Scene, v_UV).g;
	float blue = texture(u_Scene, v_UV - chroma).b;
	vec3 color = vec3(red, green, blue);

	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color = mix(vec3(luminance), color, 0.78);
	vec3 illumination = vec3(0.48, 0.61, 0.76);
	for (int i = 0; i < 16; ++i) {
		float radius = u_Lights[i].z;
		if (radius <= 0.0) continue;
		vec2 delta = v_UV * u_Resolution - u_Lights[i].xy;
		float falloff = max(0.0, 1.0 - length(delta) / radius);
		float flicker = 0.96 + 0.04 * sin(u_Time * 7.0 + float(i) * 2.1);
		illumination += vec3(2.6, 1.55, 0.58) * falloff * falloff * flicker;
	}
	color *= illumination;
	color = color / (vec3(1.0) + color * 0.25);
	color += vec3(0.035, 0.018, -0.012) * smoothstep(0.35, 0.85, luminance);

	float fog = sin(v_UV.x * 9.0 + u_Time * 0.10) *
		sin(v_UV.y * 7.0 - u_Time * 0.07) * 0.5 + 0.5;
	color = mix(color, vec3(0.12, 0.17, 0.18), fog * 0.035);

	float vignette = 1.0 - smoothstep(0.25, 0.76, edge);
	color *= mix(0.78, 1.0, vignette);
	float grain = randomNoise(v_UV * u_Resolution + u_Time * 41.0) - 0.5;
	color += grain * 0.018;

	FragColor = vec4(color, 1.0);
}
