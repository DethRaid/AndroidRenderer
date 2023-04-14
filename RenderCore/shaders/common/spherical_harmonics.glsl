#ifndef SPHERICAL_HARMONICS_GLSL
#define SPHERICAL_HARMONICS_GLSL

/**
 * Library of SH functions
 */

#include "shared/prelude.h"

#ifndef PI
#define PI 3.1415927
#endif

// Vector to SH - from https://ericpolman.com/2016/06/28/light-propagation-volumes/

/*Spherical harmonics coefficients – precomputed*/
// 1 / 2sqrt(pi)
#define SH_c0 0.282094792f 
// sqrt(3/pi) / 2
#define SH_c1 0.488602512f 

/*Cosine lobe coeff*/
// sqrt(pi)/2
#define SH_cosLobe_c0 0.886226925f
// sqrt(pi/3) 
#define SH_cosLobe_c1 1.02332671f 

mediump vec4 dir_to_cosine_lobe_ericpolman(mediump vec3 dir) {
    return vec4(SH_cosLobe_c0, -SH_cosLobe_c1 * dir.y, SH_cosLobe_c1 * dir.z, -SH_cosLobe_c1 * dir.x);
}

mediump vec4 dir_to_sh_ericpolman(mediump vec3 dir) {
    return vec4(SH_c0, -SH_c1 * dir.y, SH_c1 * dir.z, -SH_c1 * dir.x);
}

// Vector to SH - from https://www.advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf

mediump vec4 SHRotate(const in mediump vec3 vcDir, const in mediump vec2 vZHCoeffs) {
	// compute sine and cosine of theta angle
	// beware of singularity when both x and y are 0 (no need to rotate at all)
	mediump vec2 theta12_cs = normalize(vcDir.xy);
	if(length(vcDir.xy) == 0) {
		theta12_cs = vcDir.xy;
	}

	// compute sine and cosine of phi angle
	mediump vec2 phi12_cs;
	phi12_cs.x = sqrt(1.0 - vcDir.z * vcDir.z);
	phi12_cs.y = vcDir.z;
	mediump vec4 vResult;
	// The first band is rotation-independent
	vResult.x = vZHCoeffs.x;
	// rotating the second band of SH
	vResult.y = vZHCoeffs.y * phi12_cs.x * theta12_cs.y;
	vResult.z = -vZHCoeffs.y * phi12_cs.y;
	vResult.w = vZHCoeffs.y * phi12_cs.x * theta12_cs.x;
	return vResult;
}

mediump vec4 SHProjectCone(const in mediump vec3 vcDir, const mediump float angle) {
	const mediump vec2 vZHCoeffs = vec2(
			0.5 * (1.0 - cos(angle)), // 1/2 (1 - Cos[\[Alpha]])
			0.75 * sin(angle) * sin(angle)); // 3/4 Sin[\[Alpha]]^2
	return SHRotate(vcDir, vZHCoeffs);
}

mediump vec4 SHProjectCone(const in mediump vec3 vcDir) {
	const mediump vec2 vZHCoeffs = vec2(0.25, // 1/4
								0.5); // 1/2
	return SHRotate(vcDir, vZHCoeffs);
}

mediump vec4 dir_to_cosine_lobe(mediump vec3 dir) {
	// return SHProjectCone(dir * vec3(1, -1, 1));
	return dir_to_cosine_lobe_ericpolman(dir);
}

mediump vec4 dir_to_sh(mediump vec3 dir) {
	// 45 degrees to radians
	// return SHProjectCone(dir * vec3(1, -1, 1), 0.7853982);
	return dir_to_sh_ericpolman(dir);
}

// SH to vector - from https://zvxryb.github.io/blog/2015/08/20/sh-lighting-part1/

// real-valued spherical harmonics borrowed from Wikipedia

mediump float sh_project_band0 = 1.0/2.0 * sqrt(1.0/PI);

void sh_project_band1(mediump vec3 n, out mediump float Y1[3]) {
	Y1[0] = sqrt(3.0/4.0/PI) * n.y;
	Y1[1] = sqrt(3.0/4.0/PI) * n.z;
	Y1[2] = sqrt(3.0/4.0/PI) * n.x;
}

void sh_project_band2(mediump vec3 n, out mediump float Y2[5]) {
	mediump vec3 n2 = n * n;
	
	Y2[0] = 1.0/2.0 * sqrt(15.0/PI) * n.x * n.y;
	Y2[1] = 1.0/2.0 * sqrt(15.0/PI) * n.y * n.z;
	Y2[2] = 1.0/4.0 * sqrt( 5.0/PI) * (2.0*n2.z - n2.x - n2.y);
	Y2[3] = 1.0/2.0 * sqrt(15.0/PI) * n.z * n.x;
	Y2[4] = 1.0/4.0 * sqrt(15.0/PI) * (n2.x - n2.y);
}

mediump float sh_lookup_band2(mediump float x[9], mediump vec3 n) {
	mediump float Y0 = sh_project_band0;
	mediump float Y1[3];
	mediump float Y2[5];
	sh_project_band1(n, Y1);
	sh_project_band2(n, Y2);
	
	mediump float value = float(0.0);
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

mediump float sh_lookup_band1(mediump vec4 coefficients, mediump vec3 direction) {
	mediump vec4 projected_direction = dir_to_sh(direction);

	return dot(projected_direction, coefficients);
}

#endif 
