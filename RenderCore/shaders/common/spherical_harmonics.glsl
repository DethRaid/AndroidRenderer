/**
 * Library of SH functions
 */

#include "shared/prelude.h"

#ifndef PI
#define PI 3.1415927
#endif

// Vector to SH - from Crytek's LPV paper

vec4 sh_rotate(const in vec3 vec, const in vec2 zh_coeffs) {
    // compute sine and cosine of thetta angle
    // beware of singularity when both x and y are 0 (no need to rotate at all)
    vec2 theta12_cs = normalize(vec.xy);

	if(length(vec.xy) == 0.0) {
		theta12_cs = vec.xy;
	}

    // compute sine and cosine of phi angle
    vec2 phi12_cs;
    phi12_cs.x = sqrt(1.0 - vec.z * vec.z);
    phi12_cs.y = vec.z;

    vec4 result;
    // The first band is rotation-independent
    result.x = zh_coeffs.x;
    // rotating the second band of SH
    result.y = zh_coeffs.y * phi12_cs.x * theta12_cs.y;
    result.z = -zh_coeffs.y * phi12_cs.y;
    result.w = zh_coeffs.y * phi12_cs.x * theta12_cs.x;
    return result;
}

vec4 sh_project_cone(const in vec3 vcDir, float angle)
{
	const vec2 vZHCoeffs = vec2(0.5 * (1.0 - cos(angle)), // 1/2 (1 - Cos[\[Alpha]])
								0.75 * sin(angle) * sin(angle)); // 3/4 Sin[\[Alpha]]^2
	return sh_rotate(vcDir, vZHCoeffs);
}

vec4 sh_project_cone(const in vec3 vector) {
    const vec2 zh_coeffs = vec2(0.25, 0.5);

    return sh_rotate(vector, zh_coeffs);
}

// SH to vector - from https://zvxryb.github.io/blog/2015/08/20/sh-lighting-part1/

// real-valued spherical harmonics borrowed from Wikipedia

float sh_project_band0 = 1.0/2.0 * sqrt(1.0/PI);

void sh_project_band1(vec3 n, out float Y1[3]) {
	Y1[0] = sqrt(3.0/4.0/PI) * n.y;
	Y1[1] = sqrt(3.0/4.0/PI) * n.z;
	Y1[2] = sqrt(3.0/4.0/PI) * n.x;
}

void sh_project_band2(vec3 n, out float Y2[5]) {
	vec3 n2 = n * n;
	
	Y2[0] = 1.0/2.0 * sqrt(15.0/PI) * n.x * n.y;
	Y2[1] = 1.0/2.0 * sqrt(15.0/PI) * n.y * n.z;
	Y2[2] = 1.0/4.0 * sqrt( 5.0/PI) * (2.0*n2.z - n2.x - n2.y);
	Y2[3] = 1.0/2.0 * sqrt(15.0/PI) * n.z * n.x;
	Y2[4] = 1.0/4.0 * sqrt(15.0/PI) * (n2.x - n2.y);
}

float sh_lookup_band2(float x[9], vec3 n) {
	float Y0 = sh_project_band0;
	float Y1[3];
	float Y2[5];
	sh_project_band1(n, Y1);
	sh_project_band2(n, Y2);
	
	float value = float(0.0);
	value += x[0] * Y0;
	value += x[1] * Y1[0];
	value += x[2] * Y1[1];
	value += x[3] * Y1[2];
	value += x[4] * Y2[0];
	value += x[5] * Y2[1];
	value += x[6] * Y2[2];
	value += x[7] * Y2[3];
	value += x[8] * Y2[4];
	
	return value;
}

// Adapted from the above

float sh_lookup_band1(vec4 coefficients, vec3 direction) {
	vec4 projected_direction = sh_project_cone(direction);

	return dot(projected_direction, coefficients);
}
