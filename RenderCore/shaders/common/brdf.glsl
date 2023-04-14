#ifndef BRDF_GLSL
#define BRDF_GLSL

#ifndef PI
#define PI 3.1415927
#endif

#ifndef medfloat
#define medfloat mediump float
#define medvec2 mediump vec2
#define medvec3 mediump vec3
#define medvec4 mediump vec4
#endif

struct SurfaceInfo {
    medvec3 location;

    medvec3 base_color;

    medvec3 normal;

    medfloat metalness;

    medfloat roughness;

    medvec3 emission;
};

medfloat D_GGX(medfloat NoH, medfloat roughness) {
    medfloat k = roughness / (1.0 - NoH * NoH + roughness * roughness);
    return k * k * (1.0 / PI);
}

medvec3 F_Schlick(medfloat u, medvec3 f0, medfloat f90) { return f0 + (f90 - f0) * pow(clamp(1.0 - u, 0.0, 1.0), 5.0); }

medfloat V_SmithGGXCorrelated(medfloat NoV, medfloat NoL, medfloat a) {
    medfloat a2 = a * a;
    medfloat GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    medfloat GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);

    return 0.5 / (GGXV + GGXL);
}

medfloat Fd_Lambert() { return 1.0 / PI; }

medvec3 Fd_Burley(medfloat NoV, medfloat NoL, medfloat LoH, medfloat roughness) {
    medfloat f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    medvec3 lightScatter = F_Schlick(NoL, vec3(1.0), f90);
    medvec3 viewScatter = F_Schlick(NoV, vec3(1.0), f90);

    return lightScatter * viewScatter * (1.0 / PI);
}

medfloat PDF_GGX(const in medfloat roughness, const in medvec3 n, const in medvec3 l, const in medvec3 v) {
    medvec3 h = normalize(v + l);

    const medfloat VoH = clamp(dot(v, h), 0, 1);
    const medfloat NoH = clamp(dot(n, h), 0, 1);

    // D_GGX(NoH, roughness) * NoH / (4.0 + VoH);

    return 1 / (4 * VoH);
}

medvec3 Fd(in SurfaceInfo surface, medvec3 l, const medvec3 v) {
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const medfloat dielectric_f0 = 0.04; // TODO: Get this from a texture
    const medvec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const medvec3 diffuse_color = surface.base_color.rgb * (1 - dielectric_f0) * (1 - surface.metalness);

    const medvec3 h = normalize(v + l);

    medfloat NoV = dot(surface.normal, v) + 1e-5;
    medfloat NoL = dot(surface.normal, l);
    const medfloat NoH = clamp(dot(surface.normal, h), 0, 1);
    const medfloat VoH = clamp(dot(v, h), 0, 1);

    if(NoL <= 0) {
        return vec3(0);
    }

    NoV = abs(NoV);
    NoL = clamp(NoL, 0, 1);

    // diffuse BRDF
    const medfloat LoH = clamp(dot(l, h), 0, 1);
    return diffuse_color * Fd_Burley(NoV, NoL, LoH, surface.roughness);
}

medvec3 Fr(in SurfaceInfo surface, medvec3 l, const medvec3 v) {
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const medfloat dielectric_f0 = 0.04; // TODO: Get this from a texture
    const medvec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const medvec3 h = normalize(v + l);

    medfloat NoV = dot(surface.normal, v) + 1e-5;
    medfloat NoL = dot(surface.normal, l);
    const medfloat NoH = clamp(dot(surface.normal, h), 0, 1);
    const medfloat VoH = clamp(dot(v, h), 0, 1);

    if(NoL <= 0) {
        return vec3(0);
    }

    NoV = abs(NoV);
    NoL = clamp(NoL, 0, 1);

    const medfloat D = D_GGX(NoH, surface.roughness);
    const medvec3 F = F_Schlick(VoH, f0, 1.f);
    const medfloat V = V_SmithGGXCorrelated(NoV, NoL, surface.roughness);

    // specular BRDF
    return (D * V) * F;
}

medvec3 brdf(in SurfaceInfo surface, medvec3 l, const medvec3 v) {
    // Rely on the optimizer to remove all the redundant instructions
    return Fd(surface, l, v) + Fr(surface, l, v);
}

#endif
