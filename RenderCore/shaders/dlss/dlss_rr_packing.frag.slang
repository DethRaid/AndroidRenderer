/*
 * Packs our data into the format DLSS-RR needs
 */

Texture2D<half4> gbuffer_color;
Texture2D<half4> gbuffer_normals;
Texture2D<half4> gbuffer_data;

struct Output {
    half4 diffuse_albedo;
    half4 specular_albedo;
    half4 combined_normals_roughness;
};

[shader("fragment")]
Output main(float4 sv_position: SV_Position) {
    const int2 pixel = (int2)sv_position.xy;

    const half3 base_color = gbuffer_color[pixel].rgb;
    const half3 normal = normalize(gbuffer_normals[pixel].xyz);
    const half4 data_sample = gbuffer_data[pixel];

    const half roughness = data_sample.g;
    const half metalness = data_sample.b;
    
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const half dielectric_f0 = 0.04h; // TODO: Get this from a texture

    Output output = (Output)0;
    output.diffuse_albedo.rgb = base_color * (1.h - dielectric_f0) * (1.h - metalness);
    output.specular_albedo.rgb = lerp(dielectric_f0.xxx, base_color, metalness);
    output.combined_normals_roughness = half4(normal, roughness);

    return output;
}
