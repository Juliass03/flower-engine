#version 460

struct PushConstantData
{
    vec2 u_dir;
    int u_mipLevel;
};

layout(push_constant) uniform block
{
	PushConstantData myPerFrame;
};

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

layout(set=0, binding = 0) uniform sampler2D inputSampler;

void main()
{
    int s_lenght = 16; 
    float s_coeffs[] = { 
        0.074693, 
        0.073396, 
        0.069638, 
        0.063796, 
        0.056431, 
        0.048197, 
        0.039746, 
        0.031648, 
        0.024332, 
        0.018063, 
        0.012947, 
        0.008961, 
        0.005988,
        0.003864, 
        0.002407, 
        0.001448, 
    }; 

    vec4 accum = s_coeffs[0] * texture( inputSampler, inTexCoord );   
    for(int i=1;i<s_lenght;i++)
    {
        float f= i;
        accum += s_coeffs[i] * texture( inputSampler, inTexCoord + myPerFrame.u_dir*f);
        accum += s_coeffs[i] * texture( inputSampler, inTexCoord - myPerFrame.u_dir*f);
    }

    outColor =  accum;
}
