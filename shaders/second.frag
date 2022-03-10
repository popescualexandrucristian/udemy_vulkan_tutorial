#version 450 // GLSL 4.5

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inDepth;

layout(location = 0) out vec4 outColor;
layout(push_constant) uniform Data {
   float screenWidth;
} data;

void main()
{
   if(gl_FragCoord.x > data.screenWidth / 2.0)
   {
      float depth = subpassLoad(inDepth).r;
      float depthLowerBound = 0.995;
      float depthUpperBound = 1.0;
      float correctedDepth = 1.0 - ((depth - depthLowerBound) / (depthUpperBound - depthLowerBound));
      outColor = vec4(correctedDepth, correctedDepth, correctedDepth, 1.0);
   }
   else
      outColor = subpassLoad(inColor).rgba;
}
