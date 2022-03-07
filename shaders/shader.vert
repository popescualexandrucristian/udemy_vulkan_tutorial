#version 450 // GLSL 4.5

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(set = 0, binding = 0) uniform UboViewProjection
{
   mat4 projection;
   mat4 view;
} uboViewProjection;

layout(set = 0, binding = 1) uniform Model
{
   mat4 model;
} model;

layout(push_constant) uniform PushColor
{
   vec3 color;
} pushColor;

layout(location = 0) out vec3 outColor;

void main()
{
   gl_Position = uboViewProjection.projection * uboViewProjection.view * model.model * vec4(position, 1.0);
   outColor = color * pushColor.color;
}