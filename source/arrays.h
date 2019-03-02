#ifndef AL_ARRAYS_H
#define AL_ARRAYS_H

#include "al_math.h"

template <typename C, int COLS, int ROWS, int SLICES>
struct Array3DSized {
	C data[SLICES*COLS*ROWS];
	
	static uint32_t slices() { return SLICES; }
	static uint32_t depth() { return SLICES; }
	static uint32_t columns() { return COLS; }
	static uint32_t width() { return COLS; }
	static uint32_t rows() { return ROWS; }
	static uint32_t height() { return ROWS; }
	static uint32_t elems() { return SLICES*COLS*ROWS; }
	static glm::vec3 dim() { return glm::vec3(COLS, ROWS, SLICES); }
	static glm::ivec3 idim() { return glm::ivec3(COLS, ROWS, SLICES); }
	
	Array3DSized& clear() {
		for (int i = 0; i < elems(); i++) {
			data[i] = C(0);
		}
		return *(this);
	}
	
	Array3DSized& noise() {
		for (int i = 0; i < elems(); i++) {
			data[i] = glm::linearRand(C(0), C(1));
		}
		return *this;
	}
	
	Array3DSized& scale(float v) {
		for (int i = 0; i < elems(); i++) {
			data[i] *= v;
		}
		return *this;
	}
	
	inline int index_raw(int x, int y, int z) {
		return x + y*COLS + z*(ROWS*COLS);
	}
	
	inline int index_clamp(int x, int y, int z) {
		x = x < 0 ? 0 : x >= COLS ? COLS - 1 : x;
		y = y < 0 ? 0 : y >= ROWS ? ROWS - 1 : y;
		z = z < 0 ? 0 : z >= SLICES ? SLICES - 1 : z;
		return index_raw(x, y, z);
	}

	int index_from_texcoord(glm::vec3 texcoord) {
		glm::vec3 t = texcoord*(dim() - 1.f);
		glm::vec3 t0 = glm::vec3(floor(t.x + 0.5f), floor(t.y + 0.5f), floor(t.z + 0.5f));
		return index_clamp(t0.x, t0.y, t0.z);
	}

	inline C read_clamp(int x, int y, int z) {
		return data[index_clamp(x,y,z)];
	}

	inline C read_clamp(glm::vec3 texcoord) {
		return data[index_from_texcoord(texcoord)];
	}

	// clamped version:
	inline C samplepix(glm::vec3 texcoord) {
		// TODO: offet by 0.5f?
		glm::vec3 t0 = glm::vec3(floor(texcoord.x), floor(texcoord.y), floor(texcoord.z));
		glm::vec3 t1 = t0 + 1.f;
		glm::vec3 ta = texcoord - t0;
		glm::vec3 tb = 1.f-ta;
		C v000 = read_clamp(t0.x, t0.y, t0.z);
		C v010 = read_clamp(t1.x, t0.y, t0.z);
		C v100 = read_clamp(t0.x, t1.y, t0.z);
		C v110 = read_clamp(t1.x, t1.y, t0.z);
		C v001 = read_clamp(t0.x, t0.y, t1.z);
		C v011 = read_clamp(t1.x, t0.y, t1.z);
		C v101 = read_clamp(t0.x, t1.y, t1.z);
		C v111 = read_clamp(t1.x, t1.y, t1.z);
		
		return v000 * ta.x * ta.y * ta.z
			 + v100 * tb.x * ta.y * ta.z
			 + v010 * ta.x * tb.y * ta.z
			 + v110 * tb.x * tb.y * ta.z
			 + v001 * ta.x * ta.y * tb.z
			 + v101 * tb.x * ta.y * tb.z
			 + v011 * ta.x * tb.y * tb.z
			 + v111 * tb.x * tb.y * tb.z;
		/*
		return mix(
			mix(mix(v000, v010, ta.x), 
				mix(v100, v110, ta.x), ta.y),
			mix(mix(v001, v011, ta.x), 
				mix(v101, v111, ta.x), ta.y),
			ta.z);*/
	}
	
	inline glm::vec3 norm2pix(glm::vec3 texcoord) {
		return texcoord * glm::vec3(COLS-1, ROWS-1, SLICES-1);
	}
	
	inline glm::vec3 wholepix_floored(glm::vec3 t) {
		return glm::vec3(floor(t.x), floor(t.y), floor(t.z));
	}
	
	inline glm::vec3 wholepix_centered(glm::vec3 t) {
		return glm::vec3(floor(t.x+0.5f), floor(t.y+0.5f), floor(t.z+0.5f));
	}

	C sample(glm::vec3 texcoord) {
		return samplepix(norm2pix(texcoord));
	}

	Array3DSized& blat(C value, glm::vec3 texcoord, float blend) {
		glm::vec3 t = norm2pix(texcoord);
		glm::vec3 t0 = wholepix_centered(t);
		int idx0 = index_clamp(t0.x, t0.y, t0.z);
		// old values
		C v0 = data[idx0];
		// interpolated application:
		data[idx0] = v0 + blend*(value - v0);
		return *this;
	}
	
	// interpolates field toward @value at @texcoord
	// @blend: amount of change to make. @blend 0 has no effect. @blend 1 applies all of value to the field.
	Array3DSized& update(C value, glm::vec3 texcoord, float blend=1.) {
		glm::vec3 t = norm2pix(texcoord);
		glm::vec3 t0 = wholepix_floored(t);
		glm::vec3 t1 = t0 + 1.f;
		glm::vec3 ta = t - t0;
		glm::vec3 tb = 1.f - ta;
		int idx000 = index_clamp(t0.x, t0.y, t0.z);
		int idx010 = index_clamp(t0.x, t1.y, t0.z);
		int idx100 = index_clamp(t1.x, t0.y, t0.z);
		int idx110 = index_clamp(t1.x, t1.y, t0.z);
		int idx001 = index_clamp(t0.x, t0.y, t1.z);
		int idx011 = index_clamp(t0.x, t1.y, t1.z);
		int idx101 = index_clamp(t1.x, t0.y, t1.z);
		int idx111 = index_clamp(t1.x, t1.y, t1.z);
		// old values
		C v000 = data[idx000];
		C v100 = data[idx100];
		C v010 = data[idx010];
		C v110 = data[idx110];
		C v001 = data[idx001];
		C v101 = data[idx101];
		C v011 = data[idx011];
		C v111 = data[idx111];
		// interpolated application:
		data[idx000] = v000 + blend*ta.x*ta.y*ta.z*(value - v000);
		data[idx100] = v100 + blend*tb.x*ta.y*ta.z*(value - v100);
		data[idx010] = v010 + blend*ta.x*tb.y*ta.z*(value - v010);
		data[idx110] = v110 + blend*tb.x*tb.y*ta.z*(value - v110);
		data[idx001] = v001 + blend*ta.x*ta.y*tb.z*(value - v001);
		data[idx101] = v101 + blend*tb.x*ta.y*tb.z*(value - v101);
		data[idx011] = v011 + blend*ta.x*tb.y*tb.z*(value - v011);
		data[idx111] = v111 + blend*tb.x*tb.y*tb.z*(value - v111);
		return *this;
	}
	
	// adds @value at @texcoord
	Array3DSized& splat(C value, glm::vec3 texcoord) {
		glm::vec3 t = norm2pix(texcoord);
		glm::vec3 t0 = wholepix_floored(t);
		glm::vec3 t1 = t0 + 1.f;
		glm::vec3 ta = t - t0;
		glm::vec3 tb = 1.f - ta;
		int idx000 = index_clamp(t0.x, t0.y, t0.z);
		int idx010 = index_clamp(t0.x, t1.y, t0.z);
		int idx100 = index_clamp(t1.x, t0.y, t0.z);
		int idx110 = index_clamp(t1.x, t1.y, t0.z);
		int idx001 = index_clamp(t0.x, t0.y, t1.z);
		int idx011 = index_clamp(t0.x, t1.y, t1.z);
		int idx101 = index_clamp(t1.x, t0.y, t1.z);
		int idx111 = index_clamp(t1.x, t1.y, t1.z);
		// old values
		C v000 = data[idx000];
		C v100 = data[idx100];
		C v010 = data[idx010];
		C v110 = data[idx110];
		C v001 = data[idx001];
		C v101 = data[idx101];
		C v011 = data[idx011];
		C v111 = data[idx111];
		// interpolated application:
		data[idx000] = v000 + ta.x*ta.y*ta.z*(value);
		data[idx100] = v100 + tb.x*ta.y*ta.z*(value);
		data[idx010] = v010 + ta.x*tb.y*ta.z*(value);
		data[idx110] = v110 + tb.x*tb.y*ta.z*(value);
		data[idx001] = v001 + ta.x*ta.y*tb.z*(value);
		data[idx101] = v101 + tb.x*ta.y*tb.z*(value);
		data[idx011] = v011 + ta.x*tb.y*tb.z*(value);
		data[idx111] = v111 + tb.x*tb.y*tb.z*(value);
		return *this;
	}
	
	void diffuse(Array3DSized& sourcefield, float diffusion, int passes=10) {
		
		C * iptr = sourcefield.data;
		float div = 1.f/((1.f+6.f*diffusion));
	
		uint32_t w = width();
		uint32_t h = height();
		uint32_t d = depth();
	
		//-- Gauss-Seidel relaxation scheme:
		for (int n=0; n<passes; n++) {
			for (int z = 0; z<d; z++) {
				for (int y = 0; y<h; y++) {
					for (int x = 0; x<w; x++) {
						int idx = index_raw(x, y, z);
						C pre =	iptr[idx];
						auto va00 =	data[index_clamp(x-1, y  , z  )];
						auto vb00 =	data[index_clamp(x+1, y  , z  )];
						auto v0a0 =	data[index_clamp(x  , y-1, z  )];
						auto v0b0 =	data[index_clamp(x  , y+1, z  )];
						auto v00a =	data[index_clamp(x  , y  , z-1)];
						auto v00b =	data[index_clamp(x  , y  , z+1)];
						auto newval = div*(
							pre +
							diffusion * (va00 + vb00 + v0a0 + v0b0 + v00a + v00b)
						);
						data[idx] = newval;
					}
				}
			}
		}
	}
	
};

template <typename C, int COLS, int ROWS>
struct Array2DSized {
	C data[COLS*ROWS];

	static uint32_t columns() { return COLS; }
	static uint32_t width() { return COLS; }
	static uint32_t rows() { return ROWS; }
	static uint32_t height() { return ROWS; }
	static uint32_t elems() { return COLS*ROWS; }
	static size_t size() { return COLS*ROWS*sizeof(C); }
	static glm::vec2 dim() { return glm::vec2(COLS, ROWS); }

	Array2DSized& clear() {
		for (int i = 0; i < elems(); i++) {
			data[i] = C(0);
		}
		return *(this);
	}

	Array2DSized& noise(float mix=1.) {
		for (int i = 0; i < elems(); i++) {
			data[i] += mix*(glm::linearRand(C(0), C(1)) - data[i]);
		}
		return *this;
	}
	
	Array2DSized& scale(float v) {
		for (int i = 0; i < elems(); i++) {
			data[i] *= v;
		}
		return *this;
	}
	
	int index_raw(int x, int y) {
		return x + y*COLS;
	}

	bool oob(int x, int y) {
		return x < 0 || y < 0 || x >= COLS || y >= ROWS;
	}

	// returns -1 if position is out of bounds:
	int index_oob(int x, int y) {
		return oob(x, y) ? -1 : index_raw(x, y); 
	}
	
	int index_clamp(int x, int y) {
		x = x < 0 ? 0 : x >= COLS ? COLS - 1 : x;
		y = y < 0 ? 0 : y >= ROWS ? ROWS - 1 : y;
		return x + y*COLS;
	}

	int index_from_texcoord(glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(dim() - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x + 0.5f), floor(t.y + 0.5f));
		return index_clamp(t0.x, t0.y);
	}

	C read(int x, int y) {
		x = x < 0 ? 0 : x >= width() ? width() - 1 : x;
		y = y < 0 ? 0 : y >= height() ? height() - 1 : y;
		return data[index_raw(x,y)];
	}

	C read_clamp(int x, int y) {
		x = x < 0 ? 0 : x >= width() ? width() - 1 : x;
		y = y < 0 ? 0 : y >= height() ? height() - 1 : y;
		return data[x + y*int(width())];
	}

	C mix(C a, C b, float f) {
		return a + f*(b - a);
	}

	C samplepix(glm::vec2 texcoord) {
		glm::vec2 t0 = glm::vec2(floor(texcoord.x), floor(texcoord.y));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = texcoord - t0;
		C v00 = read_clamp(t0.x, t0.y);
		C v01 = read_clamp(t1.x, t0.y);
		C v10 = read_clamp(t0.x, t1.y);
		C v11 = read_clamp(t1.x, t1.y);
		return mix(mix(v00, v01, ta.x), mix(v10, v11, ta.x), ta.y);
	}

	C sample(glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(glm::vec2(COLS-1,ROWS-1));
		glm::vec2 t0 = glm::vec2(floor(t.x), floor(t.y));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		C v00 = read_clamp(t0.x, t0.y);
		C v01 = read_clamp(t1.x, t0.y);
		C v10 = read_clamp(t0.x, t1.y);
		C v11 = read_clamp(t1.x, t1.y);
		return mix(mix(v00, v01, ta.x), mix(v10, v11, ta.x), ta.y);
	}

	Array2DSized& blat(C value, glm::vec2 texcoord, float blend=1.) {
		glm::vec2 t = texcoord*(dim() - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x+0.5f), floor(t.y+0.5f));
		int idx00 = index_clamp(t0.x, t0.y);
		// old values
		C v00 = data[idx00];
		// interpolated application:
		data[idx00] = v00 + blend*(value - v00);
		return *this;
	}
	
	Array2DSized& blendto(C value, glm::vec2 texcoord, float blend=1.) {
		glm::vec2 t = texcoord*(dim() - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x+0.5f), floor(t.y+0.5f));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		glm::vec2 tb = 1.f - ta;
		int idx00 = index_clamp(t0.x, t0.y);
		int idx01 = index_clamp(t0.x, t1.y);
		int idx10 = index_clamp(t1.x, t0.y);
		int idx11 = index_clamp(t1.x, t1.y);
		// old values
		C v00 = data[idx00];
		C v10 = data[idx10];
		C v01 = data[idx01];
		C v11 = data[idx11];
		// interpolated application:
		data[idx00] = v00 + blend*ta.x*ta.y*(value - v00);
		data[idx10] = v10 + blend*tb.x*ta.y*(value - v10);
		data[idx01] = v01 + blend*ta.x*tb.y*(value - v01);
		data[idx11] = v11 + blend*tb.x*tb.y*(value - v11);
		return *this;
	}
	
	Array2DSized& update(C value, glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(dim() - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x+0.5f), floor(t.y+0.5f));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		glm::vec2 tb = 1.f - ta;
		int idx00 = index_clamp(t0.x, t0.y);
		int idx01 = index_clamp(t0.x, t1.y);
		int idx10 = index_clamp(t1.x, t0.y);
		int idx11 = index_clamp(t1.x, t1.y);
		// old values
		C v00 = data[idx00];
		C v10 = data[idx10];
		C v01 = data[idx01];
		C v11 = data[idx11];
		// interpolated application:
		data[idx00] = v00 + ta.x*ta.y*(value - v00);
		data[idx10] = v10 + tb.x*ta.y*(value - v10);
		data[idx01] = v01 + ta.x*tb.y*(value - v01);
		data[idx11] = v11 + tb.x*tb.y*(value - v11);
		return *this;
	}
	
	Array2DSized& splat(C value, glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(dim() - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x+0.5f), floor(t.y+0.5f));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		glm::vec2 tb = 1.f - ta;
		int idx00 = index_clamp(t0.x, t0.y);
		int idx01 = index_clamp(t0.x, t1.y);
		int idx10 = index_clamp(t1.x, t0.y);
		int idx11 = index_clamp(t1.x, t1.y);
		// interpolated application:
		data[idx00] += ta.x*ta.y*(value);
		data[idx10] += tb.x*ta.y*(value);
		data[idx01] += ta.x*tb.y*(value);
		data[idx11] += tb.x*tb.y*(value);
		return *this;
	}
	
	void diffuse(Array2DSized& sourcefield, float diffusion=1., int passes=10) {
		
		C * iptr = sourcefield.data;
		float div = 1.f/((1.f+4.f*diffusion));
	
		uint32_t w = width();
		uint32_t h = height();
	
		//-- Gauss-Seidel relaxation scheme:
		for (int n=0; n<passes; n++) {
			for (int y = 0; y<h; y++) {
				for (int x = 0; x<w; x++) {
					int idx = index_raw(x, y);
					C pre =	iptr[idx];
					auto va0 =	data[index_clamp(x-1, y  )];
					auto vb0 =	data[index_clamp(x+1, y  )];
					auto v0a =	data[index_clamp(x  , y-1)];
					auto v0b =	data[index_clamp(x  , y+1)];
					auto newval = div*(
						pre +
						diffusion * (va0 + vb0 + v0a + v0b)
					);
					data[idx] = newval;
				}
			}
		}
	}
};

template <typename C>
struct Array2D {
	C * data = 0;
	glm::vec2 dimensions;
	
	Array2D& init(int width, int height) {
		dimensions.x = width;
		dimensions.y = height;
		data = (C *)malloc(dimensions.x * dimensions.y * sizeof(C));
		return *this;
	}
	
	~Array2D() {
		if (data) free(data);
	}
	
	uint32_t columns() { return dimensions.x; }
	uint32_t width() { return dimensions.x; }
	uint32_t rows() { return dimensions.y; }
	uint32_t height() { return dimensions.y; }
	uint32_t elems() { return dimensions.x*dimensions.y; }
	glm::vec2 dim() { return dimensions; }
	
	Array2D& noise() {
		for (int i=0; i<dimensions.x*dimensions.y; i++) {
			data[i] = glm::linearRand(0.f, 1.f);
		}
		return *this;
	}
	
	int index_clamp(int x, int y) {
		x = x < 0 ? 0 : x >= dimensions.x ? dimensions.x - 1 : x;
		y = y < 0 ? 0 : y >= dimensions.y ? dimensions.y - 1 : y;
		return x + y*int(dimensions.x);
	}
	
	C read_clamp(int x, int y) {
		x = x < 0 ? 0 : x >= dimensions.x ? dimensions.x - 1 : x;
		y = y < 0 ? 0 : y >= dimensions.y ? dimensions.y - 1 : y;
		return data[index_clamp(x,y)];
	}

	C mix(C a, C b, float f) {
		return a + f*(b - a);
	}

	C samplepix(glm::vec2 texcoord) {
		glm::vec2 t0 = glm::vec2(floor(texcoord.x), floor(texcoord.y));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = texcoord - t0;
		C v00 = read_clamp(t0.x, t0.y);
		C v01 = read_clamp(t1.x, t0.y);
		C v10 = read_clamp(t0.x, t1.y);
		C v11 = read_clamp(t1.x, t1.y);
		return mix(mix(v00, v01, ta.x), mix(v10, v11, ta.x), ta.y);
	}

	C sample(glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(dimensions - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x), floor(t.y));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		C v00 = read_clamp(t0.x, t0.y);
		C v01 = read_clamp(t1.x, t0.y);
		C v10 = read_clamp(t0.x, t1.y);
		C v11 = read_clamp(t1.x, t1.y);
		return mix(mix(v00, v01, ta.x), mix(v10, v11, ta.x), ta.y);
	}
	
	Array2D& update(C value, glm::vec2 texcoord) {
		glm::vec2 t = texcoord*(dimensions - 1.f);
		glm::vec2 t0 = glm::vec2(floor(t.x), floor(t.y));
		glm::vec2 t1 = t0 + 1.f;
		glm::vec2 ta = t - t0;
		glm::vec2 tb = 1.f - ta;
		int idx00 = index_clamp(t0.x, t0.y);
		int idx01 = index_clamp(t0.x, t1.y);
		int idx10 = index_clamp(t1.x, t0.y);
		int idx11 = index_clamp(t1.x, t1.y);
		// old values
		C v00 = data[idx00];
		C v10 = data[idx10];
		C v01 = data[idx01];
		C v11 = data[idx11];
		// interpolated application:
		data[idx00] = v00 + ta.x*ta.y*(value - v00);
		data[idx10] = v10 + tb.x*ta.y*(value - v10);
		data[idx01] = v01 + ta.x*tb.y*(value - v01);
		data[idx11] = v11 + tb.x*tb.y*(value - v11);
		return *this;
	}
};


#endif