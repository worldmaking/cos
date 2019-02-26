#ifndef SUPPORT_H
#define SUPPORT_H

#include "al_math.h"
#include "arrays.h"

#include <vector>

#ifdef  _MSC_VER
#define AL_WIN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX 
#include <Windows.h>

#define STRUCT_ALIGN_BEGIN 
//__declspec(align(8))
#define STRUCT_ALIGN_END
HANDLE console_mutex;
#endif 

#ifdef __APPLE__
#define AL_OSX
#define STRUCT_ALIGN_BEGIN
#define STRUCT_ALIGN_END __attribute__((packed, aligned(8)))
#endif

void console_init() {
#ifdef AL_WIN
	console_mutex = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
#endif
}

int console_log(const char *fmt, ...) {
	int ret;
#ifdef AL_WIN
	if (WAIT_OBJECT_0 == WaitForSingleObject(console_mutex, 10)) {  // 10ms timeout
#endif
		va_list myargs;
		va_start(myargs, fmt);
		ret = vfprintf(stdout, fmt, myargs);
		va_end(myargs);
		fputc('\n', stdout);
#ifdef AL_WIN
		ReleaseMutex(console_mutex);
	}
#endif
	return ret;
}

int console_error(const char *fmt, ...) {
	int ret;
#ifdef AL_WIN
	if (WAIT_OBJECT_0 == WaitForSingleObject(console_mutex, 10)) {  // 10ms timeout
#endif
		va_list myargs;
		va_start(myargs, fmt);
		ret = vfprintf(stderr, fmt, myargs);
		va_end(myargs);
		fputc('\n', stderr);
#ifdef AL_WIN
	ReleaseMutex(console_mutex);
	}
#endif
	return ret;
}

glm::quat turn_toward(glm::quat q, glm::vec3 dir) {
	glm::vec3 desired = glm::normalize(dir);
	glm::vec3 fwd = quat_uf(q);
	
	glm::vec3 axis = (glm::cross(fwd, desired));
	float axis_len = glm::length(axis);
	if (axis_len > 0.001f) {
		float ang = acos(glm::dot(fwd, desired)); 
		axis = axis / axis_len;
		q = safe_normalize(glm::angleAxis(ang, axis) * q);
		//object_post(0, "new q %f %f %f %f", q.x, q.y, q.z, q.w);
	}
	return q;
}

// max should be >> 0.
glm::vec2 limit(glm::vec2 v, float max) {
	float len = glm::length(v);
	if (len > max) {
		return v * max/len;
	}
	return v;
}

// max should be >> 0.
glm::vec3 limit(glm::vec3 v, float max) {
	float len = glm::length(v);
	if (len > max) {
		return v * max/len;
	}
	return v;
}

STRUCT_ALIGN_BEGIN
template<int MAX_OBJECTS = 1024, int RESOLUTION = 5>
struct Hashspace {
	
	struct Object {
		int32_t id;		///< which object ID this is
		int32_t next, prev;
		uint32_t hash;	///< which voxel ID it belongs to (or invalidHash())
		glm::vec3 pos;
	};
	
	struct Voxel {
		/// the linked list of objects in this voxel
		int32_t first;
	};
	
	struct Shell {
		uint32_t start;
		uint32_t end;
	};
	
	Object mObjects[MAX_OBJECTS];
	Voxel mVoxels[(1<<RESOLUTION) * (1<<RESOLUTION) * (1<<RESOLUTION)];
	Shell mShells[(1<<RESOLUTION) * (1<<RESOLUTION)]; // indices into mVoxelsByDistance
	uint32_t mVoxelsByDistance[(1<<RESOLUTION) * (1<<RESOLUTION) * (1<<RESOLUTION)];
	
	uint32_t mShift, mShift2, mDim, mDim2, mDim3, mDimHalf, mWrap, mWrap3;
	
	glm::mat4 world2voxels;
	glm::mat4 voxels2world;
	float world2voxels_scale;
	
	Hashspace& reset(glm::vec3 world_min, glm::vec3 world_max) {
		
		mShift = RESOLUTION;
		mShift2 = RESOLUTION+RESOLUTION;
		mDim = (1<<RESOLUTION);
		mDim2 = (mDim*mDim);
		mDim3 = (mDim2*mDim);
		mDimHalf = (mDim/2);
		mWrap = (mDim-1);
		mWrap3 = (mDim3-1);
		
		struct ShellData {
			int32_t hash;
			double distance;
			
			ShellData(int32_t hash, double distance) 
			: hash(hash), distance(distance) {}
		};
		
		// create a map of shell radii to voxel hashes:
		std::vector<std::vector<ShellData> > shells;
		shells.resize(mDim2);
		int32_t lim = mDimHalf;
		for (int32_t x= -lim; x < lim; x++) {
			for(int32_t y=-lim; y < lim; y++) {
				for(int32_t z=-lim; z < lim; z++) {
					// each voxel lives at a given distance from the origin:
					double d = x*x+y*y+z*z;
					if (d < mDim2) {
						uint32_t h = hash(x, y, z);
						// add to list: 
						shells[d].push_back(ShellData(h, 0));
					}
				}
			}
		}
		// now pack the shell indices into a sorted list
		// and store in a secondary list the offsets per distance
		int vi = 0;
		for (unsigned d=0; d<mDim2; d++) {
			Shell& shell = mShells[d];
			shell.start = vi; 
			
			std::vector<ShellData>& list = shells[d];
			
			// the order of the elements within a shell is biased
			// earlier 
			//std::reverse(std::begin(list), std::end(list));
			
			//std::sort(list.begin(), list.end(), [](ShellData a, ShellData b) { return b.distance > a.distance; });
			
			if (!list.empty()) {
				for (unsigned j=0; j<list.size(); j++) {
					mVoxelsByDistance[vi++] = list[j].hash;
				}
			}
			shell.end = vi;

			// shells run 0..1024 (mdim2)
			// ranges up to 32768 (mdim3)
			//object_post(0, "shell %d: %d..%d", d, shell.start, shell.end);
		}
		
		
		//object_post(0, "mDim %d mDim2 %d mDim3 %d mDimHalf %d %d", mDim, mDim2, mDim3, mDimHalf, lim);
		
	
		// zero out the voxels:
		for (int i=0; i<mDim3; i++) {
			Voxel& o = mVoxels[i];
			o.first = -1;
		}
		
		// zero out the objects:
		for (int i=0; i<MAX_OBJECTS; i++) {
			Object& o = mObjects[i];
			o.id = i;
			o.hash = invalidHash();
			o.next = o.prev = -1;
		}
		
		// define the transforms:
		voxels2world = glm::translate(world_min) * glm::scale((world_max - world_min) / float(mDim));
		// world to normalized:
		world2voxels = glm::inverse(voxels2world);
		
		world2voxels_scale = mDim/((world_max.z - world_min.z));
		
		return *this;
	}
	
	
	int query(std::vector<int32_t>& result, int maxResults, glm::vec3 center, int32_t selfId=-1, float maxRadius=1000.f, float minRadius=0) {
		int nres = 0;
		
		// convert distance in term of voxels:
		minRadius = minRadius * world2voxels_scale;
		maxRadius = maxRadius * world2voxels_scale;
		
		// convert pos:
		auto ctr = world2voxels * glm::vec4(center, 1);
		const uint32_t x = ctr.x+0.5;
		const uint32_t y = ctr.y+0.5;
		const uint32_t z = ctr.z+0.5;
		
		// get shell radii:
		const uint32_t iminr2 = glm::max(uint32_t(0), uint32_t(minRadius*minRadius));
		const uint32_t imaxr2 = glm::min(mDim2, uint32_t(1 + maxRadius*maxRadius));
		
		// move out shell by shell until we have enough results
		for (int s = iminr2; s <= imaxr2 && nres < maxResults; s++) {
			const Shell& shell = mShells[s];
			const uint32_t cellstart = shell.start;
			const uint32_t cellend = shell.end;
			// look at all the voxels in this shell
			// we must check an entire shell, to avoid any spatial bias
			// due to the ordering of voxels within a shell
			for (uint32_t i = cellstart; i < cellend; i++) {
				uint32_t index = hash(x, y, z, mVoxelsByDistance[i]);
				const Voxel& voxel = mVoxels[index];
				// now add any objects in this voxel to the result...
				const int32_t first = voxel.first;
				if (first >= 0) {
					int32_t current = first;
					//int runaway_limit = 100;
					do {
						const Object& o = mObjects[current];
						if (current != o.id) {
							//object_post(0, "corrupt list");
							break;
						}
						if (current != selfId) {
							result.push_back(current);
							nres++;
						}
						current = o.next;
					} while (
							current != first // bail if we looped around the voxel
							//&& nres < maxResults // bail if we have enough hits
							//&& current >= 0  // bail if this isn't a valid object
							//&& --runaway_limit // bail if this has lost control
							); 
				}	
			}
		}
		return nres;
	}
	
	inline void remove(uint32_t objectId) {
		Object& o = mObjects[objectId];
		if (o.hash != invalidHash()) voxel_remove(mVoxels[o.hash], o);
		o.hash = invalidHash();
	}
	
	inline void move(uint32_t objectId, glm::vec3 pos) {
		Object& o = mObjects[objectId];
		o.pos = pos;
		uint32_t newhash = hash(o.pos);
		if (newhash != o.hash) {
			if (o.hash != invalidHash()) voxel_remove(mVoxels[o.hash], o);
			o.hash = newhash;
			voxel_add(mVoxels[newhash], o);
		}
	}
	
	static uint32_t invalidHash() { return UINT_MAX; }
	
	
	inline uint32_t hash(glm::vec3 v) const { 
		glm::vec4 norm = world2voxels * glm::vec4(v, 1.f);
		return hash(norm[0]+0.5, norm[1]+0.5, norm[2]+0.5); 
	}
	
	// convert x,y,z in range [0..DIM) to unsigned hash:
	// this is also the valid mVoxels index for the corresponding voxel:
	inline uint32_t hash(unsigned x, unsigned y, unsigned z) const {
		return hashx(x)+hashy(y)+hashz(z);
	}
	inline uint32_t hashx(uint32_t v) const { return v & mWrap; }
	inline uint32_t hashy(uint32_t v) const { return (v & mWrap)<<mShift; }
	inline uint32_t hashz(uint32_t v) const { return (v & mWrap)<<mShift2; }
	
	inline uint32_t unhashx(uint32_t h) const { return (h) & mWrap; }
	inline uint32_t unhashy(uint32_t h) const { return (h>>mShift) & mWrap; }
	inline uint32_t unhashz(uint32_t h) const { return (h>>mShift2) & mWrap; }
	
	// generate hash offset by an already generated hash:
	inline uint32_t hash(uint32_t x, uint32_t y, uint32_t z, uint32_t offset) const {
		return	hashx(unhashx(offset) + x) +
		hashy(unhashy(offset) + y) +
		hashz(unhashz(offset) + z);
	}
	
	// this is definitely not thread-safe.
	inline Hashspace& voxel_add(Voxel& v, Object& o) {
		if (v.first >= 0) {
			Object& first = mObjects[v.first];
			Object& last = mObjects[first.prev];
			// add to tail:
			o.prev = last.id;
			o.next = first.id;
			last.next = first.prev = o.id;
		} else {
			// unique:
			v.first = o.prev = o.next = o.id;
		}
		return *this;
	}
	
	// this is definitely not thread-safe.
	inline Hashspace& voxel_remove(Voxel& v, Object& o) {
		if (o.id == o.prev) {	// voxel only has 1 item
			v.first = -1;
		} else {
			Object& prev = mObjects[o.prev];
			Object& next = mObjects[o.next];
			prev.next = next.id;
			next.prev = prev.id;
			// update head pointer?
			if (v.first == o.id) { v.first = next.id; }
		}
		// leave the object clean:
		o.prev = o.next = -1;
		return *this;
	}

} STRUCT_ALIGN_END;

#endif