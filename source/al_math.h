#ifndef AL_MATH_H
#define AL_MATH_H

#include "al_glm.h"

#ifndef AL_MIN
#define AL_MIN(x,y) (x<y?x:y)
#endif

#ifndef AL_MAX
#define AL_MAX(x,y) (x>y?x:y)
#endif

#define AL_IS_NAN_FLOAT(v)		((v)!=(v))
#define AL_FIX_NAN_FLOAT(v)		((v)=AL_IS_NAN_FLOAT(v)?0.f:(v))

inline bool al_isnan(float v) { return v!=v; }
inline float al_fixnan(float v) { return al_isnan(v)?0.f:v; }


inline bool al_isnan(glm::vec2 v) { 
	return al_isnan(v.x) || al_isnan(v.y); 
}
inline bool al_isnan(glm::vec3 v) { 
	return al_isnan(v.x) || al_isnan(v.y) || al_isnan(v.z); 
}
inline bool al_isnan(glm::vec4 v) { 
	return al_isnan(v.x) || al_isnan(v.y) || al_isnan(v.z) || al_isnan(v.w); 
}
inline bool al_isnan(glm::quat v) { 
	return al_isnan(v.x) || al_isnan(v.y) || al_isnan(v.z) || al_isnan(v.w); 
}


inline glm::vec2 al_fixnan(glm::vec2 v) { 
	return glm::vec2( al_fixnan(v.x), al_fixnan(v.y));
}
inline glm::vec3 al_fixnan(glm::vec3 v) { 
	return glm::vec3( al_fixnan(v.x), al_fixnan(v.y), al_fixnan(v.z) );
}
inline glm::vec4 al_fixnan(glm::vec4 v) { 
	return glm::vec4( al_fixnan(v.x), al_fixnan(v.y), al_fixnan(v.z), al_fixnan(v.w) );
}
inline glm::quat al_fixnan(glm::quat v) { 
	return glm::quat( al_fixnan(v.x), al_fixnan(v.y), al_fixnan(v.z), al_fixnan(v.w) );
}


float radians(float degrees) {
	return degrees*0.01745329251994f;
}

float degrees(float radians) {
	return radians*57.29577951308233f;
}

float clip(float in, float min, float max) {
	float v = AL_MIN(in, max);
	return AL_MAX(v, min);
}

template<typename T>
inline T wrap(T x, T mod) {
	if (mod) {
		if (x > mod) {
			// shift down
			if (x > (mod*T(2))) {
				// multiple wraps:
				T div = x / mod;
				// get fract:
				T divl = (long)div;
				T fract = div - (T)divl;
				return fract * mod;
			} else {
				// single wrap:
				return x - mod;
			}
		} else if (x < T(0)) {
			// negative x, shift up
			if (x < -mod) {
				// multiple wraps:
				T div = x / mod;
				// get fract:
				T divl = (long)div;
				T fract = div - (T)divl;
				T x1 = fract * mod;
				return (x1 < T(0)) ? x1 + mod : x1;	
			} else {
				// single wrap:
				return x + mod;
			}
		} else {
			return x;
		}
	} else {
		return T(0);	// avoid divide by zero
	}
}

template<typename T>
inline T wrap(T x, T lo, T hi) {
	return lo + wrap(x-lo, hi-lo);
}

glm::vec2 wrap(glm::vec2 v, float dim=1.f) {
	return glm::vec2(
		wrap(v.x, dim), 
		wrap(v.y, dim)
	);
}

glm::vec3 wrap(glm::vec3 v, float dim=1.f) {
	return glm::vec3(
		wrap(v.x, dim), 
		wrap(v.y, dim), 
		wrap(v.z, dim)
	);
}


class random {
public: 

	static float as_float(uint32_t i) {
		union {
			uint32_t i;
			float f;
		} pun = { i };
		return pun.f;
	}

	static double as_double(uint64_t i) {
		union {
			uint64_t i;
			double f;
		} pun = { i };
		return pun.f;
	}

	static float uni() {
		//uint32_t r = rand();
		//return as_float(0x3F800000U | (r >> 9)) - 1.0f;

		return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
	}

	static float bi() {
		//uint32_t r = rand();
		//return as_float(0x3F800000U | (r >> 9))*2.f - 1.0f;
		return -1.f + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (1.f - -1.f)));
	}

	static glm::vec3 vec3() {
		return glm::vec3(uni(), uni(), uni());
	}

	static glm::vec3 vec3_bi() {
		return glm::vec3(bi(), bi(), bi());
	}

	/*
	// (0, 1]
	double rand_double_co() {
		uint32_t r64;
		return as_double(0x3FF0000000000000ULL | (r >> 12)) - 1.0;
	}*/

	static void seed() {
		srand(time(NULL));
	}

	static int integer(int lim=2) {
		return floorf(glm::linearRand(0.f, lim-0.0000001f));
	}

	static float uni_glm(float lim=1.f) {
		return glm::linearRand(0.f, lim);
	}	

	static float bi_glm(float lim=1.f) {
		return glm::linearRand(-lim, lim);
	}	

};

#endif //AL_MATH_H