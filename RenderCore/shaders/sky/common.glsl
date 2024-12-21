#ifndef SKY_COMMON_GLSL
#define SKY_COMMON_GLSL

#ifndef PI
    #define PI 3.14159265358
#endif

// Units are in megameters.
const float groundRadiusMM = 6.360;
const float atmosphereRadiusMM = 6.460;

// 200M above the ground.
const vec3 viewPos = vec3(0.0, groundRadiusMM + 0.0002, 0.0);

const vec2 tLUTRes = vec2(256.0, 64.0);
const vec2 msLUTRes = vec2(32.0, 32.0);
// Doubled the vertical skyLUT res from the paper, looks way
// better for sunrise.
const vec2 skyLUTRes = vec2(200.0, 200.0);

const vec3 groundAlbedo = vec3(0.3, 0.3, 0.3);

// These are per megameter.
// static const vec3 rayleighScatteringBase = vec3(5.802, 13.558, 33.1);    // Original Hillaire
const vec3 rayleighScatteringBase = vec3(6.6, 12.3, 29.4);           // From https://github.com/FarukEroglu2048/ARPC
const float rayleighAbsorptionBase = 0.0;

const float mieScatteringBase = 3.996;
const float mieAbsorptionBase = 4.4;

// const vec3 ozoneAbsorptionBase = vec3(0.650, 1.881, .085);        // Original Hillaire
const vec3 ozoneAbsorptionBase = vec3(2.26, 1.54, 0);                // From https://github.com/FarukEroglu2048/ARPC

float getMiePhase(float cosTheta) {
    const float g = 0.8;
    const float scale = 3.0 / (8.0 * PI);
    
    float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
    float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);
    
    return scale * num / denom;
}

float getRayleighPhase(float cosTheta) {
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + cosTheta * cosTheta);
}

void getScatteringValues(vec3 pos,
                         out vec3 rayleighScattering,
                         out float mieScattering,
                         out vec3 extinction) {
    float altitudeKM = max(0, (length(pos) - groundRadiusMM)) * 1000.0;
    // Note: Paper gets these switched up.
    float rayleighDensity = exp(-altitudeKM / 8.0);
    float mieDensity = exp(-altitudeKM / 1.2);
    
    rayleighScattering = rayleighScatteringBase * rayleighDensity;
    float rayleighAbsorption = rayleighAbsorptionBase * rayleighDensity;
    
    mieScattering = mieScatteringBase * mieDensity;
    float mieAbsorption = mieAbsorptionBase * mieDensity;
    
    vec3 ozoneAbsorption = ozoneAbsorptionBase * max(0.0, 1.0 - abs(altitudeKM - 25.0) / 15.0);
    
    extinction = rayleighScattering + rayleighAbsorption + mieScattering + mieAbsorption + ozoneAbsorption;
}

float safeacos(const float x) {
    return acos(clamp(x, -1.0, 1.0));
}

// From https://gamedev.stackexchange.com/questions/96459/fast-ray-sphere-collision-code.
float rayIntersectSphere(vec3 ro, vec3 rd, float rad) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    if (c > 0.0f && b > 0.0) {
        return -1.0;
    }
    float discr = b * b - c;
    if (discr < 0.0) {
        return -1.0;
    }
    // Special case: inside sphere, use far discriminant
    if (discr > b * b) {
        return (-b + sqrt(discr));
    }
    return -b - sqrt(discr);
}

/*
 * Same parameterization here.
 */
vec3 getValFromTLUT(sampler2D tex, vec3 pos, vec3 sunDir) {
    float height = length(pos);
    vec3 up = pos / height;
    float sunCosZenithAngle = dot(sunDir, up);
    vec2 uv = vec2(clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0),
                       max(0.0, min(1.0, (height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM))));
    return textureLod(tex, uv, 0).rgb;
}

vec3 getValFromMultiScattLUT(sampler2D tex, vec3 pos, vec3 sunDir) {
    float height = length(pos);
    vec3 up = pos / height;
    float sunCosZenithAngle = dot(sunDir, up);
    vec2 uv = vec2(clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0),
                       max(0.0, min(1.0, (height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM))));
    return textureLod(tex, uv, 0).rgb;
}

#endif
