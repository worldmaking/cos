#ifndef SHARED_H
#define SHARED_H

#include "al_glm.h"
#include "support.h"


static const int LAND_DIM = 128;
static const int LAND_DIM_1 = LAND_DIM-1;
static const int LAND_NUM_INDICES = LAND_DIM_1*LAND_DIM_1*6;


static const int VOLUME_DIM = 64;
static const int VOLUME_DIM3 = VOLUME_DIM*VOLUME_DIM*VOLUME_DIM;

static const int STALKS_MAX = 36;

static const int DUST_MAX = 1e5;

static const int AGENTS_MAX = 4096;
static const int MORPHOGENS_MAX = 4;
// maximum number of agents a spatial query can return
static const int NEIGHBOURS_MAX = 12;
static const int LINKS_MAX = AGENTS_MAX * NEIGHBOURS_MAX * 4;

glm::vec3 world_min = glm::vec3(-4., 0., -4.);
glm::vec3 world_max = glm::vec3( 4., 8.,  4.);
glm::vec3 world_dim = world_max - world_min;
glm::vec3 world_scale = 1.f/world_dim;
glm::mat4 WorldMatrix = glm::scale(glm::translate(glm::mat4(1.0f), world_min), world_max - world_min);
glm::mat4 WorldMatrixInverse = glm::inverse(WorldMatrix);

float agent_size = 0.05;
float agent_personal_space = 0.005;
float agent_range_of_view = 0.3;

float agent_field_of_view = 0.; // -1 means omni, 0 means forward only, 0.999 is laser-like
float agent_lookahead_frames = 4;

float agent_max_speed = 0.008;
float agent_max_acc   = 0.0008;
float agent_centering_factor = 0.01;
float agent_flow_factor = 0.1;
float agent_avoidance_factor = 1.;
float agent_randomwalk = 0.05;


// could make this different for different morphogens
float agent_morphogen_diffuse = 0.2; 
float agent_morphogen_walk = 0.04;

/*
num_agents = 100;
max_speed = 0.002;
min_agent_size = 0.008;
max_agent_size = 0.018;
agent_field_of_view = Math.PI * 0.9;
agent_lookahead_frames = 4;
personal_space = 0.005;
// strengths of the three flocking forces:
centering_factor = 0.01;
flow_factor = 0.1;
avoidance_factor = 1;
randomwalk_angle = 0.3;

num_obstacles = 1;
min_object_size = 0.025;
max_object_size = 0.075;
*/


STRUCT_ALIGN_BEGIN struct Link {
	int32_t id0, id1;
	
} STRUCT_ALIGN_END;

STRUCT_ALIGN_BEGIN struct Stalk {
	glm::vec3 pos;
	glm::vec3 normalized_pos;
	int ndnode;
	int len;
} STRUCT_ALIGN_END;



STRUCT_ALIGN_BEGIN struct Dust {
	glm::vec3 pos;
} STRUCT_ALIGN_END;


STRUCT_ALIGN_BEGIN struct Agent {
	glm::vec3 pos, vel, acc;
	glm::vec3 attractions, flows, avoidances;
	glm::quat orientations;
	float speeds;
	
	glm::quat quat;
	float state;
	float size;
	float field_of_view;
	glm::vec4 morphogens, morphogen_neighbourhood;
	float antigen;
	int32_t id;
	int32_t neighbours_count;
	
	void reset(int id) {
		this->id = id;
		
		acc = glm::vec3(0.);
		
		attractions = flows = avoidances = glm::vec3(0.);
		orientations = glm::quat(0.f, 0.f, 0.f, 0.f);
		speeds = 0.f;
		field_of_view = agent_field_of_view;
		
		pos = glm::linearRand(world_min, world_max);
		quat = glm::angleAxis(glm::linearRand(0.f, float(M_PI * 2.f)), glm::linearRand(glm::vec3(-1.), glm::vec3(1)));
		vel = quat_uf(quat);
		size = agent_size * glm::linearRand(0.5f, 1.5f);
		for (int i=0; i<MORPHOGENS_MAX; i++) {
			morphogens[i] = glm::linearRand(0.1f, 0.4f);
		}
		
		neighbours_count = 0;
		
		antigen = glm::linearRand(0.1f, 1.f);
	}
	
	static void colorswap(Agent& a, Agent& b, float factor) {
  		for (int i=0; i<4; i++) {
			float d = factor * ( b.morphogens[i] - a.morphogens[i] );
			a.morphogens[i] += d;
			b.morphogens[i] -= d;
		}
	}
	
} STRUCT_ALIGN_END;



STRUCT_ALIGN_BEGIN struct Instance {
	glm::vec3 position;
	float state;
	glm::quat quat;
	glm::vec4 params;
} STRUCT_ALIGN_END;

STRUCT_ALIGN_BEGIN struct LandCell {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
} STRUCT_ALIGN_END;

// this is about 7MB:
STRUCT_ALIGN_BEGIN struct CameraData {
	
	// kinect cloud:
	glm::vec3 k0[640*480];
	glm::vec3 k1[640*480];
	
	// vr tracking
	glm::vec3 pos_head, pos_hand1, pos_hand2;
	float pad;
	glm::quat quat_head, quat_hand1, quat_hand2;
	
	union {
		std::time_t timestamp;
		double unused[2];
	};
	
} STRUCT_ALIGN_END;

// remember: don't use any arch-dependent sized types in here
// no pointers, no int, no size_t, etc.
// or else it will break when sharing between 32 and 64 bit hosts
STRUCT_ALIGN_BEGIN struct Shared {
	
	// rendering data:
	Instance agent_instance_array[AGENTS_MAX];
	Instance stalk_instance_array[STALKS_MAX];
	glm::vec3 agent_links_array[LINKS_MAX*2];
	
	// sim data:
	Agent agents[AGENTS_MAX];
	Link links[LINKS_MAX];
	
	Stalk stalks[STALKS_MAX];
	Dust dust[DUST_MAX];
	
	Array2DSized<glm::vec3, LAND_DIM, LAND_DIM> land;
	unsigned int land_indices[LAND_NUM_INDICES];
	Array2DSized<glm::vec3, LAND_DIM_1, LAND_DIM_1> land_intermediate_normals;
	Array2DSized<LandCell, LAND_DIM, LAND_DIM> land_cells;
	
	Array3DSized<glm::vec4, VOLUME_DIM, VOLUME_DIM, VOLUME_DIM> density;
	Array3DSized<float, VOLUME_DIM, VOLUME_DIM, VOLUME_DIM> shadow;
	Hashspace<AGENTS_MAX> hashspace;

	glm::vec3 attraction_point;
	
	int32_t link_count = 0;
	
	int32_t updating, recording, playback;
	
	CameraData cameradata;
	
	void init() {
		recording = 0;
		playback = 1;
		
		for (int i=0; i<AGENTS_MAX; i++) {
			Agent& a = agents[i];
			a.reset(i);
		}
		
		for (int i=0; i<LINKS_MAX; i++) {
			Link& a = links[i];
			a.id0 = a.id1 = -1;
		}
		link_count = 0;

		hashspace.reset(world_min, world_max);

		attraction_point = glm::vec3(0., 1.8, 0.);
		
		density.noise();
		shadow.clear();
		land.clear();
#ifdef AL_OSX
		for (int i=0; i<10; i++) {
			glm::vec2 tc = glm::diskRand(0.4f) + 0.5f;
			land.update(glm::vec3(0.f, glm::linearRand(0.f, 10.f), 0.f), tc);
		}
		land.noise(0.05);
		land.diffuse(land, 0.1);
		land.diffuse(land, 0.2);
		land.noise(0.04);
		land.diffuse(land, 0.1);
#endif
		for (int y=0; y<land.height(); y++) {
			for (int x=0; x<land.width(); x++) {
				int idx = land.index_raw(x, y);
				glm::vec3& cell = land.data[idx];
				
				cell.x = x / float(land.width());
				cell.z = y / float(land.height());
				
				cell *= world_dim;
				cell += world_min;
			}
		}
		unsigned int * lip = land_indices;
		for (int y=0; y<land.height()-1; y++) {
			for (int x=0; x<land.width()-1; x++) {
				int idx00 = land.index_raw(x, y);
				int idx10 = land.index_raw(x+1, y);
				int idx01 = land.index_raw(x, y+1);
				int idx11 = land.index_raw(x+1, y+1);
				
				*lip++ = idx11;
				*lip++ = idx01;
				*lip++ = idx00;
				
				*lip++ = idx00;
				*lip++ = idx10;
				*lip++ = idx11;
			}
		}

		for (int y = 0; y<land_intermediate_normals.height(); y++) {
			for (int x = 0; x<land_intermediate_normals.width(); x++) {
				glm::vec3 v00 = land.read(x, y);
				glm::vec3 v10 = land.read(x + 1, y);
				glm::vec3 v01 = land.read(x, y + 1);
				glm::vec3 v11 = land.read(x + 1, y + 1);

				// compute normal based on these four points
				glm::vec3 vx0 = v10 - v00;
				glm::vec3 vx1 = v11 - v01;
				glm::vec3 v0y = v01 - v00;
				glm::vec3 v1y = v11 - v10;

				glm::vec3 n0 = glm::cross(v1y, vx1);
				glm::vec3 n1 = glm::cross(v0y, vx0);

				glm::vec3 vx = glm::normalize(n0 + n1); // avg

				int idx = land_intermediate_normals.index_raw(x, y);
				land_intermediate_normals.data[idx] = n1;
			}
		}
		for (int y = 0; y<land_cells.height(); y++) {
			for (int x = 0; x<land_cells.width(); x++) {
				int idx = land_cells.index_raw(x, y);

				glm::vec2 t = glm::vec2(x, y) / glm::vec2(float(LAND_DIM_1));

				land_cells.data[idx].position = land.data[idx];
				land_cells.data[idx].normal = land_intermediate_normals.sample(t);
			}
		}
		

		glm::vec3 stalk_ne = glm::vec3(-4., 0.,  1.25);
		glm::vec3 stalk_sw = glm::vec3( 4., 0., -1.2);
		for (int i=0; i<STALKS_MAX; i++) {

			int row = (i / 6) % 2; // 2 rows
			int col = i % 6; // 6 columns
			int layer = i / 12; // 3 layers

			Stalk& o = stalks[i];
			o.len = 4 - layer;
			switch (col) {
				case 0:
					o.ndnode = row ? 208 : 210;
					break;
				case 1:
					o.ndnode = row ? 207 : 211;
					break;
				case 2:
					o.ndnode = row ? 206 : 209;
					break;
				case 3:
					o.ndnode = row ? 205 : 200;
					break;
				case 4:
					o.ndnode = row ? 204 : 201;
					break;
				case 5:
					o.ndnode = row ? 203 : 202;
					break;
			}

			glm::vec3 pos;
			pos.z = row ? stalk_ne.z : stalk_sw.z;
			pos.x = col / float(5) * (stalk_sw.x - stalk_ne.x) + stalk_ne.x;
			pos.y = 0.5 + layer;

			o.pos = pos;
			o.normalized_pos = mat4_transform(WorldMatrixInverse, o.pos);
			
			//console_log("%f %f %f", o.pos.x, o.pos.y, o.pos.z);
			
			Instance& dst = stalk_instance_array[i];
			dst.position = o.pos;
			dst.state = 1;
			dst.quat = glm::angleAxis(float(M_PI)/2.f, glm::vec3(1.f, 0.f, 0.f));
			dst.params = glm::vec4(0.5, 0.5, 0.5, 0.2);
		}
		
		for (int i=0; i<DUST_MAX; i++) {
			Dust& o = dust[i];
			o.pos = glm::linearRand(world_min, world_max);
		}
		
		updating = 1;
	}

	void main_update(float dt) {
		if (!updating) return;



	
		//density.clear();
		density.scale(0.96);

#ifndef AL_OSX
		for (int y = 0; y<land_cells.height(); y++) {
			for (int x = 0; x<land_cells.width(); x++) {
				int idx = land_cells.index_raw(x, y);
				land_cells.data[idx].position.y *= 0.97;
			}
		}
#endif
		// locomotion:
		for (int i=0; i<AGENTS_MAX; i++) {
			Agent& a = agents[i];
			// increment velocity by acceleration
			// and constrain to limits:
			a.vel = limit(a.vel + a.acc, agent_max_speed);
			a.quat = turn_toward(a.quat, a.vel);
			
			// increment position by velocity
			// and constrain to world
			a.pos = glm::clamp(a.pos + a.vel, world_min, world_max);
			
			// add to density field:
			// convert a.pos to normalized:
			glm::vec3 norm = (a.pos - world_min) * world_scale;
			density.splat(glm::vec4(a.antigen, a.morphogens.y, a.morphogens.z, a.size), norm);
		}
		// copy to rendering state:
		for (int i=0; i<AGENTS_MAX; i++) {
			const Agent& src = agents[i];
			Instance& dst = agent_instance_array[i];
			
			dst.position = src.pos;
			dst.quat = src.quat;
			dst.state = 1.;
			dst.params.x = src.antigen; //src.morphogens[0];
			dst.params.y = src.morphogens[1];
			dst.params.z = src.morphogens[2];
			dst.params.w = src.size;
		}
		
		for (int i=0; i<LINKS_MAX; i++) {
			const Link& src = links[i];
			if (src.id0 >= 0 && src.id1 >= 0) {
				agent_links_array[i * 2] = agents[src.id0].pos;
				agent_links_array[i * 2 + 1] = agents[src.id1].pos;
			}
		}
		

		float v = 0.2 * dt;
		float rise = 0.2*dt;
		glm::vec3 risemin = glm::vec3(0., rise, 0.);
		glm::vec3 riserange = glm::vec3(v, rise + v, v) - risemin;
		for (int i = 0; i < DUST_MAX; i++) {
			Dust& o = dust[i];
			//o.pos += glm::linearRand(risemin, risemax);
			//o.pos += glm::vec3(0., rise, 0.);
			o.pos += risemin + random::vec3_bi()*v;
			//o.pos += random::vec3_bi();
		}
	}
	
	void simulation_update(double dt) {
		
		if (!updating) return;

		

		for (int i = 0; i < DUST_MAX; i++) {
			Dust& o = dust[i];
			if (o.pos.y > world_max.y) {
				//o.pos.y = world_min.y;
				// locate to a random stalk:
				int which = glm::linearRand(0, STALKS_MAX - 1);
				o.pos = stalks[which].pos;
			}
		}
		
		/*
		// randomize color by differentiating from a random partner in world
    // (this keeps overall color qty constant)
    let b = random(agents);
    colorswap(a, b, -colorwalk*colordiffuse);
    
    */
    	// update hashspace:
    	for (int i=0; i<AGENTS_MAX; i++) {
			Agent& a = agents[i];
			hashspace.move(i, a.pos);
		}
		
		// sensing:
		int links_found = 0;
		for (int i=0; i<AGENTS_MAX; i++) {
			Agent& a = agents[i];
			
			
			// reset sensing state:
   		 	a.attractions = a.flows = a.avoidances = glm::vec3(0.);
   		 	a.orientations = glm::quat(0.f, 0.f, 0.f, 0.f);
   		 	a.speeds = 0.f;
   		 	a.morphogen_neighbourhood = glm::vec4(0.f);
   		 	
   		 	/*
   		 	// check each possible obstacle:
			for (var o of obstacles) {
			  // compute relative vector from our future position to obstacle:
			  var lookahead = a.vel.clone().scale(agent_lookahead_frames); // ahead 4 frames
			  var lookahead_point = lookahead.add(a.pos);
			  // get relative vector from lookahead point to obstacle
			  var rel = o.pos.clone().sub(lookahead_point);
			  // wrap in toroidal space
			  rel.relativewrap(1);
			  // compute distance, taking into account object sizes:
			  var distance = Math.max(rel.len() - a.size - o.size, 0);
			  // if collision likely:
			  if (distance < personal_space) {
				// add an avoidance force:
				var mag = 1 - (distance / personal_space);
				var avoid = rel.clone().scale(-mag);
				a.avoidances.add(avoid);
			  }
			}
			*/
			
			
			/*
				At present the flows only captures velocity, not orientation
				How about making flows be a quat, and capture speed separately?
			
			*/
			
			a.neighbours_count = 0;
			std::vector<int32_t> neighbours;
			int nres = hashspace.query(neighbours, NEIGHBOURS_MAX, a.pos, i, agent_range_of_view);
			for (auto j : neighbours) {
				Agent& n = agents[j];
			
				// get relative vector from a to n:
				glm::vec3 rel = n.pos - a.pos;
				float cdistance = glm::length(rel);
			
				// get distance between bodies (never be negative)
				float distance = glm::max(cdistance - a.size - n.size, 0.f);
			
				// skip neighbours that are too far away:
				if (distance > agent_range_of_view) continue;
			
				// rotate into my view:
				//  var viewrel = rel.clone().rotate(-a.vel.angle());
				//  // skip if we can't see
				//  if (Math.abs(viewrel.angle()) > agent_field_of_view) continue;
			
				float similarity = glm::dot(quat_uf(a.quat), glm::normalize(rel));
				if (similarity < a.field_of_view) continue;
			
				// we can sense a neighbour:
				a.neighbours_count++; // add a neighbour to our sensed count
				if (links_found < LINKS_MAX) {
					Link& link = links[links_found];
					link.id0 = a.id;
					link.id1 = n.id;
					links_found++;
				}
				
				// accumulate neighbour (relative) positions to attractions:	
				// TODO: maybe some of these should have a distance-dependent factor?
				a.attractions += (rel);
				// accumulate neighbour velocities:
				a.flows += (n.vel);
			
				a.orientations += n.quat;
				a.speeds += glm::length(n.vel);

			
				//if (cdistance < 0.5) {
			//		lines.push(a.pos);
			//		lines.push(n.pos);
			//	}
				float amt = agent_range_of_view/(distance+agent_range_of_view);
				//Agent::colorswap(a, n, amt*agent_morphogen_diffuse);
				a.morphogen_neighbourhood += n.morphogens;
			
				// accumulate avoidances:
				// base this on where we are going to be next:
				glm::vec3 future_rel = n.pos - a.pos + ((n.vel - a.vel) * agent_lookahead_frames);
				float future_distance = glm::max(glm::length(future_rel) - a.size - n.size, 0.f);
				// if likely to collide:
				if (future_distance < agent_personal_space) {
					// add an avoidance force:
					float mag = 1. - (future_distance / agent_personal_space);
					glm::vec3 avoid = future_rel * -mag;
					a.avoidances += (avoid);
				}
				
			}
			
			for (auto j : neighbours) {
				Agent& n = agents[j];
				float separate_rate = agent_morphogen_diffuse * 0.0001;
				
				if (n.antigen > a.antigen) {
					float amt = glm::min(glm::min(a.antigen, 1.f-n.antigen), separate_rate);
					n.antigen += amt;
					a.antigen -= amt;
				} else {
					float amt = glm::min(glm::min(n.antigen, 1.f-a.antigen), separate_rate);
					a.antigen += amt;
					n.antigen -= amt;
				}
			}
						
			if (a.neighbours_count == 0) {
				
				a.morphogens = glm::mix(a.morphogens * glm::vec4(0.9f, 0.99f, 0.999f, 1.f), glm::vec4(1.f), agent_morphogen_diffuse);


				glm::vec3 rel = attraction_point - a.pos;
				a.attractions += (rel);
				
			} else {
				
				a.morphogens = glm::mix(a.morphogens * glm::vec4(0.9f, 0.99f, 0.999f, 1.f), a.morphogen_neighbourhood / float(a.neighbours_count), agent_morphogen_diffuse);
			}
			
			// morphogen effects:
			//a.size = agent_size * (1.+a.morphogens.x);
			//a.field_of_view = agent_field_of_view * (1.5 - a.morphogens.x);
		}
			
		this->link_count = links_found;
		
		// update_steering
		for (int i=0; i<AGENTS_MAX; i++) {
			Agent& a = agents[i];
			
			glm::vec3 desired_velocity = glm::vec3(a.vel);
			
			// wander (random walk)
			// by slightly changing current velocity
			glm::quat q = glm::angleAxis(glm::linearRand(0.f, agent_randomwalk), glm::linearRand(glm::vec3(-1.), glm::vec3(1)));
			desired_velocity = quat_rotate(q, desired_velocity);
			
			// apply factors due to neighbours:
			if (a.neighbours_count > 0) {
				// cohesion (move to center)
				// compute the average relative vector to all neighbours:
			  	a.attractions *= agent_centering_factor / a.neighbours_count;
			  	
			  	a.flows *= agent_flow_factor / a.neighbours_count;
				
				a.speeds = a.speeds / a.neighbours_count;
				a.orientations = glm::normalize(a.orientations);
				// could slerp to a.orientations by some factor now?
				
				desired_velocity += a.attractions + a.flows;
			}
			
			// apply avoidances (obstacles and neighbours)
			a.avoidances *= agent_avoidance_factor;
			desired_velocity += (a.avoidances);
			
			// finally, use the desired velocity to compute the steering force:
			// (i.e. the acceleration)
			
			a.acc += (desired_velocity - a.vel);
			
			// and also constrain it
			a.acc = limit(a.acc, agent_max_acc);
		}
		
	}
	
} STRUCT_ALIGN_END;

#endif