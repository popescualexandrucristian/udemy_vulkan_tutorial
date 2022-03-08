#version 450 // GLSL 4.5

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

void main()
{
   outColor = max(vec4(inColor, 1.0), vec4(0.5)) * texture(textureSampler, inUV);
}
