#version 450 // GLSL 4.5

vec2 positions[] = {
   vec2( 1.0,-1.0),
   vec2(-1.0,-1.0),
   vec2(-1.0, 1.0),

   vec2( 1.0,-1.0),
   vec2(-1.0, 1.0),
   vec2( 1.0, 1.0),
};

void main()
{
   gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}