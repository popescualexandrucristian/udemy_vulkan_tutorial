#version 450 // GLSL 4.5

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(set = 0, binding = 0) uniform MVP
{
   mat4 projection;
   mat4 view;
   mat4 model;
} mvp;

layout(location = 0) out vec3 outColor;

void main()
{
   gl_Position = mvp.projection * mvp.view * mvp.model * vec4(position, 1.0);
   outColor = color;
}