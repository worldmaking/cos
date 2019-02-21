struct Geometry {
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<uint16_t> indices;
};

Geometry makeCube() {
	Geometry g;
	g.vertices = {
		// front
      -1.0, -1.0,  1.0,
       1.0, -1.0,  1.0,
       1.0,  1.0,  1.0,
      -1.0,  1.0,  1.0,

      // back
      -1.0, -1.0, -1.0,
      -1.0,  1.0, -1.0,
       1.0,  1.0, -1.0,
       1.0, -1.0, -1.0,

      // upside
      -1.0,  1.0, -1.0,
      -1.0,  1.0,  1.0,
       1.0,  1.0,  1.0,
       1.0,  1.0, -1.0,

      // downslide
      -1.0, -1.0, -1.0,
       1.0, -1.0, -1.0,
       1.0, -1.0,  1.0,
      -1.0, -1.0,  1.0,

      // right
       1.0, -1.0, -1.0,
       1.0,  1.0, -1.0,
       1.0,  1.0,  1.0,
       1.0, -1.0,  1.0,

      // left
      -1.0, -1.0, -1.0,
      -1.0, -1.0,  1.0,
      -1.0,  1.0,  1.0,
      -1.0,  1.0, -1.0
	};
	g.normals = {
		 // front
       0.0,  0.0,  1.0,
       0.0,  0.0,  1.0,
       0.0,  0.0,  1.0,
       0.0,  0.0,  1.0,

      // back
       0.0,  0.0, -1.0,
       0.0,  0.0, -1.0,
       0.0,  0.0, -1.0,
       0.0,  0.0, -1.0,

      // upside
       0.0,  1.0,  0.0,
       0.0,  1.0,  0.0,
       0.0,  1.0,  0.0,
       0.0,  1.0,  0.0,

      // downside
       0.0, -1.0,  0.0,
       0.0, -1.0,  0.0,
       0.0, -1.0,  0.0,
       0.0, -1.0,  0.0,

      // right
       1.0,  0.0,  0.0,
       1.0,  0.0,  0.0,
       1.0,  0.0,  0.0,
       1.0,  0.0,  0.0,

      // left
      -1.0,  0.0,  0.0,
      -1.0,  0.0,  0.0,
      -1.0,  0.0,  0.0,
      -1.0,  0.0,  0.0
	};
	g.indices = {
		0, 1, 2,
      2, 3, 0,
 
      4, 5, 6,
      6, 7, 4,
      
      8, 9, 10,
      10, 11, 8,
      
      12, 13, 14, 
      14, 15, 12,
      
      16, 17, 18,
      18, 19, 16,
      
      20, 21, 22,
      22, 23, 20
	};
	return g;
}

Geometry cubeGeometry = makeCube();

const int POPULATION_SIZE = 500;

const char * const VERTEX_CODE = R"(
#version 300 es
uniform mat4 u_viewmatrix, u_perspectivematrix;
uniform mat3 u_viewmatrix_inverse;
uniform float u_time;
in vec3 a_position;
in vec3 a_normal;
in vec3 a_location;
in vec4 a_orientation;
in vec4 a_properties;
out vec3 normal, eyepos, ray_origin, ray_direction;
out float time;
out vec3 world_vertex;
out vec4 world_orientation;
out mat4 viewprojectionmatrix;

//	q must be a normalized quaternion
vec3 quat_rotate(vec4 q, vec3 v) {
	vec4 p = vec4(
		q.w*v.x + q.y*v.z - q.z*v.y,	// x
		q.w*v.y + q.z*v.x - q.x*v.z,	// y
		q.w*v.z + q.x*v.y - q.y*v.x,	// z
		-q.x*v.x - q.y*v.y - q.z*v.z	// w
	);
	return vec3(
		p.x*q.w - p.w*q.x + p.z*q.y - p.y*q.z,	// x
		p.y*q.w - p.w*q.y + p.x*q.z - p.z*q.x,	// y
		p.z*q.w - p.w*q.z + p.y*q.x - p.x*q.y	// z
	);
}

// equiv. quat_rotate(quat_conj(q), v):
// q must be a normalized quaternion
vec3 quat_unrotate(in vec4 q, in vec3 v) {
	// return quat_mul(quat_mul(quat_conj(q), vec4(v, 0)), q).xyz;
	// reduced:
	vec4 p = vec4(
				  q.w*v.x - q.y*v.z + q.z*v.y,  // x
				  q.w*v.y - q.z*v.x + q.x*v.z,  // y
				  q.w*v.z - q.x*v.y + q.y*v.x,  // z
				  q.x*v.x + q.y*v.y + q.z*v.z   // w
				  );
	return vec3(
				p.w*q.x + p.x*q.w + p.y*q.z - p.z*q.y,  // x
				p.w*q.y + p.y*q.w + p.z*q.x - p.x*q.z,  // y
				p.w*q.z + p.z*q.w + p.x*q.y - p.y*q.x   // z
				);
}

void main() {
	vec3 vertex = a_position;
  //vertex = u_viewmatrix_inverse * vertex; 
  
  world_vertex = quat_rotate(a_orientation, vertex) + a_location;
  world_orientation = a_orientation;

  // rotate to face camera:
  vec4 world_position = u_viewmatrix * vec4(world_vertex, 1);

  gl_Position = u_perspectivematrix * world_position;
	//gl_PointSize = 13.0;
  normal = a_normal;

  // get normal in world space:
  normal = quat_rotate(a_orientation, normal);

	// derive eye location in world space from current view matrix:
	// (could pass this in as a uniform instead...)
	eyepos = -(u_viewmatrix[3].xyz)*mat3(u_viewmatrix);
  viewprojectionmatrix = u_perspectivematrix * u_viewmatrix;
  // we want the raymarching to operate in object-local space:
	ray_origin = vertex;
	ray_direction = quat_unrotate(a_orientation, world_vertex - eyepos); 

  time = a_properties.x;
}
)";

const char * const FRAGMENT_CODE = R"(
#version 300 es
precision mediump float;
in vec3 normal, eyepos, ray_origin, ray_direction;
in float time;
in vec3 world_vertex;
in vec4 world_orientation;
in mat4 viewprojectionmatrix;
out vec4 outColor;

#define PI 3.141592653589793
#define TWOPI 3.141592653589793*2.0

// Maximum/minumum elements of a vector
float vmax(vec2 v) {
	return max(v.x, v.y);
}

float vmax(vec3 v) {
	return max(max(v.x, v.y), v.z);
}

float vmax(vec4 v) {
	return max(max(v.x, v.y), max(v.z, v.w));
}

float vmin(vec2 v) {
	return min(v.x, v.y);
}

float vmin(vec3 v) {
	return min(min(v.x, v.y), v.z);
}

float vmin(vec4 v) {
	return min(min(v.x, v.y), min(v.z, v.w));
}

float fSphere(vec3 p, float r) {
	return length(p) - r;
}

// Cylinder standing upright on the xz plane
float fCylinder(vec3 p, float r, float height) {
	float d = length(p.xz) - r;
	d = max(d, abs(p.y) - height);
	return d;
}

// Capsule: A Cylinder with round caps on both sides
float fCapsule(vec3 p, float r, float c) {
	return mix(length(p.xz) - r, length(vec3(p.x, abs(p.y) - c, p.z)) - r, step(c, abs(p.y)));
}

// Box: correct distance to corners
float fBox(vec3 p, vec3 b) {
	vec3 d = abs(p) - b;
	return length(max(d, vec3(0))) + vmax(min(d, vec3(0)));
}

// Signed distance function for a cube centered at the origin
// http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/
float sdCube(in vec3 p, in vec3 r){
	vec3 d = abs(p) - r;
	// Assuming p is inside the cube, how far is it from the surface?
    // Result will be negative or zero.
	float inDist = min(max(d.x, max(d.y, d.z)), 0.0);
	// Assuming p is outside the cube, how far is it from the surface?
    // Result will be positive or zero.
	float outDist = length(max(d, 0.0));
	return inDist + outDist;
}

// polynomial smooth min (k = 0.1);
float smin( float a, float b, float k ) {
	float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
	return mix( b, a, h ) - k*h*(1.0-h);
}

float smax( float a, float b, float k ) {
	float k1 = k*k;
	float k2 = 1./k1;
	return log( exp(k2*a) + exp(k2*b) )*k1;
}

// p is the vec3 position of the surface at the fragment.
// viewProjectionMatrix would be typically passed in as a uniform
// assign result to gl_FragDepth:
float computeDepth(vec3 p, mat4 viewProjectionMatrix) {
	float dfar = gl_DepthRange.far;
	float dnear = gl_DepthRange.near;
	vec4 clip_space_pos = viewProjectionMatrix * vec4(p, 1.);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;	
	// standard perspective:
	return (((dfar-dnear) * ndc_depth) + dnear + dfar) / 2.0;
}


//	q must be a normalized quaternion
vec3 quat_rotate(vec4 q, vec3 v) {
	vec4 p = vec4(
		q.w*v.x + q.y*v.z - q.z*v.y,	// x
		q.w*v.y + q.z*v.x - q.x*v.z,	// y
		q.w*v.z + q.x*v.y - q.y*v.x,	// z
		-q.x*v.x - q.y*v.y - q.z*v.z	// w
	);
	return vec3(
		p.x*q.w - p.w*q.x + p.z*q.y - p.y*q.z,	// x
		p.y*q.w - p.w*q.y + p.x*q.z - p.z*q.x,	// y
		p.z*q.w - p.w*q.z + p.y*q.x - p.x*q.y	// z
	);
}

// equiv. quat_rotate(quat_conj(q), v):
// q must be a normalized quaternion
vec3 quat_unrotate(in vec4 q, in vec3 v) {
	// return quat_mul(quat_mul(quat_conj(q), vec4(v, 0)), q).xyz;
	// reduced:
	vec4 p = vec4(
				  q.w*v.x - q.y*v.z + q.z*v.y,  // x
				  q.w*v.y - q.z*v.x + q.x*v.z,  // y
				  q.w*v.z - q.x*v.y + q.y*v.x,  // z
				  q.x*v.x + q.y*v.y + q.z*v.z   // w
				  );
	return vec3(
				p.w*q.x + p.x*q.w + p.y*q.z - p.z*q.y,  // x
				p.w*q.y + p.y*q.w + p.z*q.x - p.x*q.z,  // y
				p.w*q.z + p.z*q.w + p.x*q.y - p.y*q.x   // z
				);
}


float map(vec3 p) {
  float t1 = (cos(time * -TWOPI)*0.5+0.5);
  float t2 = (sin(time * -TWOPI)*0.5+0.5);
  float s = fSphere(p, t1);
  float s1 = fSphere(p+vec3(0, 0.3, 0), t1*0.7);
  float s2 = fSphere(p-+vec3(0, 0.3, 0), t2*0.7);
  float c = fCylinder(p, 0.2, 1.);
  float b = fBox(p, vec3(0.5, 1., 0.1));
  float sc = smin(s, c, 0.2);
  float cb = smin(c, b, 0.2);
  float bs = smin(b, s, 0.2);

  float ss = smin(s1, s2, 0.3);
  float ssc = smin(ss, b, .1);
  return ssc;
}


// compute normal from a SDF gradient by sampling 4 tetrahedral points around a location p
// (cheaper than the usual technique of sampling 6 cardinal points)
// 'fScene' should be the SDF evaluator 'float distance = fScene(vec3 pos)''  
// 'eps' is the distance to compare points around the location 'p' 
// a smaller eps gives sharper edges, but it should be large enough to overcome sampling error
// in theory, the gradient magnitude of an SDF should everywhere = 1, 
// but in practice this isn’t always held, so need to normalize() the result
vec3 normal4(in vec3 p, float eps) {
  vec2 e = vec2(-eps, eps);
  // tetrahedral points
  float t1 = map(p + e.yxx), t2 = map(p + e.xxy), t3 = map(p + e.xyx), t4 = map(p + e.yyy); 
 	vec3 n = (e.yxx*t1 + e.xxy*t2 + e.xyx*t3 + e.yyy*t4);
 	// normalize for a consistent SDF:
 	//return n / (4.*eps*eps);
 	// otherwise:
 	return normalize(n);
}

void main() {
  vec3 lightdir = vec3(1, 1, 0);
  vec3 color = vec3(1.);
  

  vec3 rd = normalize(ray_direction);
  vec3 ro = (ray_origin);
  vec3 nn = normal; // normalize(normal) not necessary for a cube face

  float l = length(ray_origin);
  float maxt = 2.*sqrt(3.);
  float precise = 0.01;
  float t = 0.;
  #define STEPS 32
  vec3 p = ro;
  float d = map(p);
  for (int i=0; i<STEPS; i++) {
    if (abs(d) < precise || t > maxt) break;
    t += d;
    p = ro + t*rd;
    d = map(p);
  }

  if (t < maxt && abs(d) < precise) {
  
    // also write to depth buffer, for detailed occlusion:
    vec3 world_pos = (world_vertex + quat_rotate(world_orientation, p));
    gl_FragDepth = computeDepth(world_pos.xyz, viewprojectionmatrix);
    // texcoord from naive normal:
    vec3 tnn = normalize(p)*0.5+0.5;
    // use this for basic marking colors:
    color = vec3(0.8, tnn.yz * 0.5+0.5);

    // normal in object space:
    nn = normal4(p, .001);
    // in world space
    vec3 wnn = quat_rotate(world_orientation, nn);
    // use this for lighting:
    float ndl = dot(wnn, lightdir);
    color *= ndl*0.4+0.6;
  } else {
    discard;
    gl_FragDepth = 0.99999;
  }


	outColor = vec4(color, 1);
}
)";

struct Agent {
	glm::vec3 pos;
	float speed;
	glm::vec3 vel;
	float rate;
	glm::quat orient;

	float phase;
};

struct World {

	Agent agents[POPULATION_SIZE];

	glm::vec3 locations[POPULATION_SIZE];
	glm::quat orientations[POPULATION_SIZE];
	glm::vec4 properties[POPULATION_SIZE];

	Shader program = Shader(VERTEX_CODE, FRAGMENT_CODE);
	
	GLuint vao;
	GLuint vertex_buf, normal_buf, index_buf;
	GLuint location_buf, orientation_buf, property_buf;

	void reset() {
		for (int i = 0; i < POPULATION_SIZE; i++) {
			Agent& a = agents[i];
			a.speed = 10;
			a.orient = quat_random();
			a.pos = glm::ballRand(6.);
			a.vel = glm::sphericalRand(0.1);

			a.rate = 2*(rnd::uni()+0.5);
			a.phase = rnd::uni();
		}
	}

	void updateAgents(float dt) {
		for (int i = 0; i < POPULATION_SIZE; i++) {
			Agent& a = agents[i];

			a.phase = wrap(a.phase + a.rate*dt, 1.f);

			// do the twist
			glm::quat q1 = quat_random();
			a.orient = glm::slerp(a.orient, q1, 0.04f);

			// move
			float spd = cos(a.phase * PI * 2.f);
			spd = AL_MAX(0, spd);
			glm::vec3 wv = glm::vec3(0, spd * a.speed * dt, 0);
			wv = quat_rotate(a.orient, wv);
			a.pos += wv; 
			// bounds:
			if (glm::length(a.pos) > 20.f) {
				a.pos = glm::vec3(0, 0, 0);
			}
		}
	}

	void updateBuffers() {
		for (int i = 0; i < POPULATION_SIZE; i++) {
			Agent& a = agents[i];
			locations[i] = a.pos;
			orientations[i] = a.orient;
			properties[i] = glm::vec4(a.phase, a.phase, a.phase, a.phase);
		}

		glBindBuffer(GL_ARRAY_BUFFER, location_buf);
		glBufferData(GL_ARRAY_BUFFER, sizeof(locations), locations, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, orientation_buf);
		glBufferData(GL_ARRAY_BUFFER, sizeof(orientations), orientations, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, property_buf);
		glBufferData(GL_ARRAY_BUFFER, sizeof(properties), properties, GL_DYNAMIC_DRAW);
	}

	void dest_changed() {
		program.dest_changed();


		glGenVertexArrays(1, &vao); //vao = glCreateVertexArray();
		glGenBuffers(1, &vertex_buf);  //vertex_buf = glCreateBuffer();
		glGenBuffers(1, &normal_buf);
		glGenBuffers(1, &index_buf);
		glGenBuffers(1, &location_buf);
		glGenBuffers(1, &orientation_buf);
		glGenBuffers(1, &property_buf);
		glBindVertexArray(vao);
		{
			Geometry& geometry = cubeGeometry;
			{
				glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
				glBufferData(GL_ARRAY_BUFFER, geometry.vertices.size() * sizeof(glm::vec3), &geometry.vertices[0], GL_STATIC_DRAW);
				auto attrLoc = glGetAttribLocation(program.program, "a_position");
				glEnableVertexAttribArray(attrLoc);
				glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
				glVertexAttribPointer(attrLoc, 3, GL_FLOAT, false, 0, 0);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			{
				glBindBuffer(GL_ARRAY_BUFFER, normal_buf);
				glBufferData(GL_ARRAY_BUFFER, geometry.normals.size() * sizeof(glm::vec3), &geometry.normals[0], GL_STATIC_DRAW);
				auto attrLoc = glGetAttribLocation(program.program, "a_normal");
				glEnableVertexAttribArray(attrLoc);
				glBindBuffer(GL_ARRAY_BUFFER, normal_buf);
				glVertexAttribPointer(attrLoc, 3, GL_FLOAT, false, 0, 0);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			{
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf);
  				glBufferData(GL_ELEMENT_ARRAY_BUFFER, geometry.indices.size() * sizeof(uint16_t), &geometry.indices[0], GL_STATIC_DRAW);
			}

			{
				glBindBuffer(GL_ARRAY_BUFFER, location_buf);
				glBufferData(GL_ARRAY_BUFFER, sizeof(locations), &locations[0], GL_DYNAMIC_DRAW);
				auto attrLoc = glGetAttribLocation(program.program, "a_location");
				glEnableVertexAttribArray(attrLoc);
				glBindBuffer(GL_ARRAY_BUFFER, location_buf);
				glVertexAttribPointer(attrLoc, 3, GL_FLOAT, false, 0, 0);
				glVertexAttribDivisor(attrLoc, 1);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			{
				glBindBuffer(GL_ARRAY_BUFFER, orientation_buf);
				glBufferData(GL_ARRAY_BUFFER, sizeof(orientations), &orientations[0], GL_DYNAMIC_DRAW);
				auto attrLoc = glGetAttribLocation(program.program, "a_orientation");
				glEnableVertexAttribArray(attrLoc);
				glBindBuffer(GL_ARRAY_BUFFER, orientation_buf);
				glVertexAttribPointer(attrLoc, 4, GL_FLOAT, false, 0, 0);
				glVertexAttribDivisor(attrLoc, 1);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			{
				glBindBuffer(GL_ARRAY_BUFFER, property_buf);
				glBufferData(GL_ARRAY_BUFFER, sizeof(properties), &properties[0], GL_DYNAMIC_DRAW);
				auto attrLoc = glGetAttribLocation(program.program, "a_properties");
				glEnableVertexAttribArray(attrLoc);
				glBindBuffer(GL_ARRAY_BUFFER, property_buf);
				glVertexAttribPointer(attrLoc, 4, GL_FLOAT, false, 0, 0);
				glVertexAttribDivisor(attrLoc, 1);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
		glBindVertexArray(0);

		updateBuffers();
	}

	void update() {
		float t = glfwGetTime() * 0.001;
		float dt = 1/60.f; // TODO
		updateAgents(dt);

		// set up view and perspective matrices:
		float fovy = PI * 0.25;
		float aspect = 1.33f; //canvas.clientWidth / canvas.clientHeight;
		glm::mat4 perspectivematrix = glm::perspective(fovy, aspect, 0.01f, 100.f);
		glm::vec3 target = glm::vec3(0, 0, 0);
		float t1 = t * 0.1;
		float d = 13 * (sin(t1) + 2.5);
		glm::vec3 eye = glm::vec3(d * sin(t1), d * sin(t1 * 0.5), d * cos(t1) );
		glm::vec3 up = glm::vec3(0, 1, 0);
		glm::mat4 viewmatrix = glm::lookAt(eye, target, up);
		glm::mat3 view3 = glm::mat3(viewmatrix);
		glm::mat3 viewmatrix_inverse = glm::inverse(view3);

		updateBuffers();

		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		program.use();
		program.uniform("u_time", t); 
		program.uniform("u_viewmatrix", viewmatrix);
		program.uniform("u_viewmatrix_inverse", viewmatrix_inverse);
		program.uniform("u_perspectivematrix", perspectivematrix);

		glBindVertexArray(vao);
		glDrawElementsInstanced(GL_TRIANGLES, cubeGeometry.indices.size(), GL_UNSIGNED_SHORT, 0,POPULATION_SIZE);
		glBindVertexArray(0);
		program.unuse();
	}
};