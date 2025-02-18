Texture2D source_image;
RWTexture2D dest_image;

[[vk::push_constant]]
cbuffer Constants {
    uint2 resolution;
};

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint2 thread_id: SV_DispatchThreadID) {
    if (any(thread_id >= resolution)) {
        return;
    }

    dest_image[thread_id] = source_image[thread_id];
}
