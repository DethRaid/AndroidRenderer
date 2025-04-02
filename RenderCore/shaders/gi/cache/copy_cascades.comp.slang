/*
 * Copies probe information from one texture to another, to account for cascade movement. Any new probes get a special value written to validity (0xFF)
 */

Texture2DArray<half3> rtgi_src;
Texture2DArray<half3> light_cache_src;
Texture2DArray<half2> depth_src;
Texture2DArray<half3> average_srv;
Texture2DArray<half> validity_src;

RWTexture2DArray<half3> rtgi_dest;
RWTexture2DArray<half3> light_cache_dest;
RWTexture2DArray<half2> depth_dest;
RWTexture2DArray<half3> average_dest;
RWTexture2DArray<half> validity_dest;

[vk::push_constant]
cbuffer Constants {
    float3 cascade_movement[4];
};

void init_new_probe(in int3 index) {
    const int2 rtgi_probe_size = int2(7, 8);
    const int3 rtgi_pixel = index * int3(rtgi_probe_size, 1);
    for (int y = 0; y < rtgi_probe_size.y; y++) {
        for (int x = 0; x < rtgi_probe_size.x; x++) {
            rtgi_dest[rtgi_pixel + int3(x, y, 0)] = half3(0);
        }
    }

    const int2 light_cache_probe_size = int2(13, 13);
    const int3 light_cache_pixel = index * int3(light_cache_probe_size, 1);
    for (int y = 0; y < light_cache_probe_size.y; y++) {
        for (int x = 0; x < light_cache_probe_size.x; x++) {
            light_cache_dest[light_cache_pixel + int3(x, y, 0)] = half3(0);
        }
    }

    const int2 depth_probe_size = int2(12, 12);
    const int3 depth_pixel = index * int3(depth_probe_size, 1);
    for (int y = 0; y < depth_probe_size.y; y++) {
        for (int x = 0; x < depth_probe_size.x; x++) {
            depth_dest[light_cache_pixel + int3(x, y, 0)] = half2(0);
        }
    }

    average_dest[index] = half3(0);

    // Super special marker for new probes
    validity_dest[index] = 0xff;
}

void copy_from_cell(in int3 dest_index, in int3 src_index) {
    const int2 rtgi_probe_size = int2(7, 8);
    const int3 rtgi_dest_pixel = dest_index * int3(rtgi_probe_size, 1);
    const int3 rtgi_src_pixel = src_index * int3(rtgi_probe_size, 1);
    for (int y = 0; y < rtgi_probe_size.y; y++) {
        for (int x = 0; x < rtgi_probe_size.x; x++) {
            rtgi_dest[rtgi_dest_pixel + int3(x, y, 0)] = rtgi_src[rtgi_src_pixel + int3(x, y, 0)];
        }
    }

    const int2 light_cache_probe_size = int2(13, 13);
    const int3 light_cache_dest_pixel = dest_index * int3(light_cache_probe_size, 1);
    const int3 light_cache_src_pixel = src_index * int3(light_cache_probe_size, 1);
    for (int y = 0; y < light_cache_probe_size.y; y++) {
        for (int x = 0; x < light_cache_probe_size.x; x++) {
            light_cache_dest[light_cache_dest_pixel + int3(x, y, 0)] = light_cache_src[light_cache_src_pixel + int3(x, y, 0)];
        }
    }

    const int2 depth_probe_size = int2(12, 12);
    const int3 depth_dest_pixel = dest_index * int3(depth_probe_size, 1);
    const int3 depth_src_pixel = src_index * int3(depth_probe_size, 1);
    for (int y = 0; y < depth_probe_size.y; y++) {
        for (int x = 0; x < depth_probe_size.x; x++) {
            depth_dest[depth_dest_pixel + int3(x, y, 0)] = depth_src[depth_src_pixel + int3(x, y, 0)];
        }
    }

    average_dest[dest_index] = average_srv[src_index];

    validity_dest[dest_index] = validity_src[src_index];
}

[numthreads(4, 4, 4)]
[shader("compute")]
void main(uint3 thread_id: SV_DispatchThreadID) {
    const uint cascade_index = thread_id.y / 8;
    const float3 movement = cascade_movement[cascade_index];

    const int3 src_cell = (int3)thread_id - (int3)movement;

    if (all(src_cell >= int3(0, 8 * cascade_index, 0)) && all(src_cell < int3(32, 8 * (cascade_index + 1), 32))) {
        copy_from_cell(thread_id, src_cell);
    } else {
        init_new_probe(thread_id);
    }
}
