#ifndef BRDF_GLSL
#define BRDF_GLSL

#ifndef PI
#define PI 3.1415927
#endif

struct SurfaceInfo {
    vec3 location;

    vec4 base_color;

    vec3 normal;

    float metalness;

    float roughness;

    vec3 emission;
};

float D_GGX(float NoH, float roughness) {
    float k = roughness / (1.0 - NoH * NoH + roughness * roughness);
    return k * k * (1.0 / PI);
}

vec3 F_Schlick(float u, vec3 f0, float f90) { return f0 + (f90 - f0) * pow(1.0 - u, 5.0); }

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);

    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() { return 1.0 / PI; }

vec3 Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    vec3 lightScatter = F_Schlick(NoL, vec3(1.0), f90);
    vec3 viewScatter = F_Schlick(NoV, vec3(1.0), f90);

    return lightScatter * viewScatter * (1.0 / PI);
}

float PDF_GGX(const in float roughness, const in vec3 n, const in vec3 l, const in vec3 v) {
    vec3 h = normalize(v + l);

    const float VoH = clamp(dot(v, h), 0, 1);
    const float NoH = clamp(dot(n, h), 0, 1);

    // D_GGX(NoH, roughness) * NoH / (4.0 + VoH);

    return 1 / (4 * VoH);
}

vec3 Fd(in SurfaceInfo surface, vec3 l, const vec3 v) {
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const float dielectric_f0 = 0.04; // TODO: Get this from a texture
    const vec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const vec3 diffuse_color = surface.base_color.rgb * (1 - dielectric_f0) * (1 - surface.metalness);

    const vec3 h = normalize(v + l);

    float NoV = dot(surface.normal, v) + 1e-5;
    float NoL = dot(surface.normal, l);
    const float NoH = clamp(dot(surface.normal, h), 0, 1);
    const float VoH = clamp(dot(v, h), 0, 1);

    if(NoL <= 0) {
        return vec3(0);
    }

    NoV = abs(NoV);
    NoL = clamp(NoL, 0, 1);

    // diffuse BRDF
    const float LoH = clamp(dot(l, h), 0, 1);
    return diffuse_color * Fd_Burley(NoV, NoL, LoH, surface.roughness);
}

vec3 Fr(in SurfaceInfo surface, vec3 l, const vec3 v) {
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const float dielectric_f0 = 0.04; // TODO: Get this from a texture
    const vec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const vec3 h = normalize(v + l);

    float NoV = dot(surface.normal, v) + 1e-5;
    float NoL = dot(surface.normal, l);
    const float NoH = clamp(dot(surface.normal, h), 0, 1);
    const float VoH = clamp(dot(v, h), 0, 1);

    if(NoL <= 0) {
        return vec3(0);
    }

    NoV = abs(NoV);
    NoL = clamp(NoL, 0, 1);

    const float D = D_GGX(NoH, surface.roughness);
    const vec3 F = F_Schlick(VoH, f0, 1.f);
    const float V = V_SmithGGXCorrelated(NoV, NoL, surface.roughness);

    // specular BRDF
    return (D * V) * F;
}

vec3 brdf(in SurfaceInfo surface, vec3 l, const vec3 v) {
    // Rely on the optimizer to remove all the redundant instructions
    return Fd(surface, l, v) + Fr(surface, l, v);
}

#endif
