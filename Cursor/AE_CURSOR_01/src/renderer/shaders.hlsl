struct VSInput {
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

cbuffer PushConstants : register(b0) {
    float4x4 MVP;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.pos = mul(MVP, float4(input.pos, 1.0f));
    output.col = input.col;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.col;
}
