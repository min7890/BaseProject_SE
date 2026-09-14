#version 330

in vec3 a_Position;
uniform vec2 u_Position;
uniform vec2 u_Size;

void main()
{
	vec2 position = a_Position.xy * u_Size + u_Position;
	gl_Position = vec4(position, 0.0, 1.0);
}
