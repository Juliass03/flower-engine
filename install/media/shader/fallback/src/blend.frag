#version 460

struct PushConstantData
{
    float u_weight;
    float u_intensity;
};

layout(push_constant) uniform block
{
	PushConstantData myPerFrame;
};

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outColor;

layout(set=0, binding=0) uniform sampler2D inputSampler;

void main()
{
    outColor = myPerFrame.u_intensity * myPerFrame.u_weight * texture( inputSampler, inTexCoord).rgba;
}
