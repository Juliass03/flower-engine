#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_framedata.glsl"

layout (set = 1, binding = 0) uniform sampler2D scenecolorTex;

layout (location = 0) in  vec2 inUV0;
layout (location = 0) out vec4 outColor;

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float aces(float x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(scenecolorTex, inUV0).xyz;
    vec3 mapped = aces(hdrColor); 
    outColor = vec4(mapped,1.0f);
}