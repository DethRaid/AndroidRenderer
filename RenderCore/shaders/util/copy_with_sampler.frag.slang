[vk::binding(0, 0)]
Sampler2D image;

[[vk::push_constant]]
cbuffer Constants {
    float2 inv_resolution;
};

[shader("fragment")]
float4 main(float2 position: SV_Position) {
    return image.Sample(position * inv_resolution);
}
