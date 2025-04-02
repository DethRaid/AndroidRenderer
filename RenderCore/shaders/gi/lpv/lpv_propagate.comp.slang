#include "shared/prelude.h"
#include "common/spherical_harmonics.slangi"

/**
 * This shader propagates lighting through the LPV, using workgroup memory to be clever
 *
 * Each cascade is 32x32x32 cells. That's 32k total, which is unfortunately more than the 1024 threads my phone
 * supports. Thus, we have a smaller workgroup
 *
 * We dispatch 4x4x4 workgroups per cascade - 64 total. Each workgroup loads a 8x8x8 block of cells into groupshared
 * memory. We can store the three color channels in groupshared  memory, it's 24k bytes. After loading, we can perform
 * one propagation step by reading from groupshared memory. This should save some texture reads
 *
 * We run one workgroup per cascade
 *
 * This may help Mali? We can only hope
 */

Texture3D<half4> read_lpv_red;
Texture3D<half4> read_lpv_green;
Texture3D<half4> read_lpv_blue;
RWTexture3D<half4> write_lpv_red;
RWTexture3D<half4> write_lpv_green;
RWTexture3D<half4> write_lpv_blue;

Sampler3D<half4> geo_volume;

[vk::push_constant]
cbuffer Constants {
    uint use_gv;
    uint num_cascades;
    uint num_cells;
};

static const half3x3 neighbour_orientations[6] = half3x3[](
    // Z+
    half3x3(1, 0, 0, 0, 1, 0, 0, 0, 1),
    // Z-
    half3x3(-1, 0, 0, 0, 1, 0, 0, 0, -1),
    // X+
    half3x3(0, 0, 1, 0, 1, 0, -1, 0, 0),
    // X-
    half3x3(0, 0, -1, 0, 1, 0, 1, 0, 0),
    // Y+
    half3x3(1, 0, 0, 0, 0, 1, 0, -1, 0),
    // Y-
    half3x3(1, 0, 0, 0, 0, -1, 0, 1, 0)
    );

static const half3 directions[6] = half3[](
    half3(0, 0, 1),
    half3(0, 0, -1),
    half3(1, 0, 0),
    half3(-1, 0, 0),
    half3(0, 1, 0),
    half3(0, -1, 0));

// right up
static const half2 side[4] = half2[](half2(1, 0), half2(0, 1), half2(-1, 0), half2(0, -1));

// orientation = [ right | up | forward ] = [ x | y | z ]
half3 get_eval_side_direction(uint index, half3x3 orientation) {
    const half smallComponent = 0.4472135h; // 1 / sqrt(5)
    const half bigComponent = 0.894427h;    // 2 / sqrt(5)

    const half2 s = side[index];
    // *either* x = 0 or y = 0
    return mul(orientation, half3(s.x * smallComponent, s.y * smallComponent, bigComponent));
}

half3 get_reproj_side_direction(uint index, half3x3 orientation) {
    const half2 s = side[index];
    return mul(orientation, half3(s.x, s.y, 0));
}

[numthreads(4, 4, 4)]
[shader("compute")]
void main(uint3 thread_index: SV_GroupThreadID, uint3 group_id: SV_GroupID) {
    const uint num_groups_x = num_cells / 4;
    const uint cascade_index = group_id.x / num_groups_x;
    const uint3 cascade_pixel_offset = uint3(cascade_index * 32, 0, 0);

    const int3 local_index = int3(thread_index);
    const int3 group_index_in_cascade = uint3(group_id.x % num_groups_x, group_id.yz);
    const int3 group_offset_in_cascade = group_index_in_cascade * 4;
    const int3 cell_index = local_index + group_offset_in_cascade;

    // Initialize this cell's local cache
    half4 lpv_r = read_lpv_red[cell_index + cascade_pixel_offset];
    half4 lpv_g = read_lpv_green[cell_index + cascade_pixel_offset];
    half4 lpv_b = read_lpv_blue[cell_index + cascade_pixel_offset];

    // Propagate lighting
    for (uint neighbor = 0; neighbor < 6; neighbor++) {
        half3x3 orientation = neighbour_orientations[neighbor];

        half3 main_direction = mul(orientation, half3(0, 0, 1));

        int3 neighbor_cell = cell_index - int3(directions[neighbor]);
        if (any(neighbor_cell < int3(-1)) || any(neighbor_cell > int3(31))) {
            continue;
        }

        half4 gv_sh = half4(0);
        half geo_volume_factor = 1.h;
        if (use_gv != 0) {
            float3 gv_coordinate = float3(neighbor_cell) / 32.f;
            // Add a half-cell offset to the GV
            gv_coordinate += 0.5f / 32.f;

            gv_coordinate.x += cascade_index;
            gv_coordinate.x /= 4.f;

            gv_sh = geo_volume.SampleLevel(gv_coordinate, 0);
            geo_volume_factor = step(0.1f, abs(dot(gv_sh, gv_sh)));
        }

        const int3 cell_to_load = neighbor_cell + cascade_pixel_offset;
        half4 red_coefficients;
        half4 green_coefficients;
        half4 blue_coefficients;

        red_coefficients = read_lpv_red[neighbor_cell + cascade_pixel_offset];
        green_coefficients = read_lpv_green[neighbor_cell + cascade_pixel_offset];
        blue_coefficients = read_lpv_blue[neighbor_cell + cascade_pixel_offset];

        const half direct_face_subtended_solid_angle = 0.4006696846h / PI / 2.h;
        const half side_face_subtended_solid_angle = 0.4234413544h / PI / 3.h;

        for (uint side_face = 0; side_face < 4; side_face++) {
            half3 eval_direction = get_eval_side_direction(side_face, orientation);
            half3 reproj_direction = get_reproj_side_direction(side_face, orientation);

            half4 eval_direction_sh = dir_to_sh(eval_direction);
            half4 reproj_direction_cosine_lobs_sh = dir_to_cosine_lobe(reproj_direction);

            // half geo_volume_factor = 1 - clamp(gv_sh.x * eval_direction_sh.x + abs(dot(gv_sh.yzw, eval_direction_sh.yzw)), 0, 1);            

            lpv_r += side_face_subtended_solid_angle * dot(red_coefficients, eval_direction_sh) * reproj_direction_cosine_lobs_sh * geo_volume_factor;
            lpv_g += side_face_subtended_solid_angle * dot(green_coefficients, eval_direction_sh) * reproj_direction_cosine_lobs_sh * geo_volume_factor;
            lpv_b += side_face_subtended_solid_angle * dot(blue_coefficients, eval_direction_sh) * reproj_direction_cosine_lobs_sh * geo_volume_factor;
        }

        half3 cur_direction = directions[neighbor];
        half4 cur_cosine_lobe = dir_to_cosine_lobe(cur_direction);
        half4 cur_direction_sh = dir_to_sh(cur_direction);

        //half geo_volume_factor = 1 - clamp(gv_sh.x * cur_direction_sh.x + abs(dot(gv_sh.yzw, cur_direction_sh.yzw)), 0, 1);

        lpv_r += direct_face_subtended_solid_angle * dot(red_coefficients, cur_direction_sh) * cur_cosine_lobe * geo_volume_factor;
        lpv_g += direct_face_subtended_solid_angle * dot(green_coefficients, cur_direction_sh) * cur_cosine_lobe * geo_volume_factor;
        lpv_b += direct_face_subtended_solid_angle * dot(blue_coefficients, cur_direction_sh) * cur_cosine_lobe * geo_volume_factor;
    }

    write_lpv_red[cell_index + cascade_pixel_offset] = lpv_r;
    write_lpv_green[cell_index + cascade_pixel_offset] = lpv_g;
    write_lpv_blue[cell_index + cascade_pixel_offset] = lpv_b;
}

