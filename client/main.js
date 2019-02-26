/* global: mat4, vec3, VRCubeIsland, WGLUDebugGeometry, WGLUStats, WGLUTextureLoader, VRSamplesUtil */




// ===================================================
// WebGL scene setup. This code is not WebVR specific.
// ===================================================
// WebGL setup.
let canvas = document.getElementById("webgl-canvas");
let gl = canvas.getContext("webgl2", { alpha: false, preserveDrawingBuffer: true, });;
vr.init(canvas, gl);

/////////////////////////

let cube = makeCube();

/*
  For VR want to measure in meters for greatest convenience
  Voxel field needs to cover whole space
  Typically origin will be centre of floor
  
  A good room-scale would be maybe 6x6m
  For a 32^3 field, if we make voxels every 20cm, it covers 6.4m. 
*/

const DIM = 32;
const DIM3 = DIM*DIM*DIM;
let field = {
  locations: new Float32Array(DIM3 * 3),
  intensities: new Float32Array(DIM3 * 4),
};

let field_matrix = mat4.create();  
let s = 0.2;
mat4.scale(field_matrix, field_matrix, vec3.fromValues(s, s, s))
mat4.translate(field_matrix, field_matrix, vec3.fromValues(-DIM/2, 0, -DIM/2))

for (let z=0, i=0; z<DIM; z++) { 
  for (let y=0; y<DIM; y++) { 
    for (let x=0; x<DIM; x++, i++) {
      let pos = vec3.fromValues(x, y, z);
      field.locations.set(pos, i * 3);
      
      let p = vec3.fromValues(0, 1, 0); //vec3.random(vec3.create(), 1.);
      let val = vec4.fromValues(p[0], p[1], p[2], Math.random());
      field.intensities.set(val, i * 4);
    } 
  } 
}


field.program = makeProgramFromCode(
  gl,
  `#version 300 es
uniform mat4 u_modelmatrix;
uniform mat4 u_viewmatrix, u_perspectivematrix;
in vec3 a_position;
in vec3 a_location;
in vec4 a_intensity;
out vec4 intensity;
out vec3 eyepos, world_vertex, ray_origin, ray_direction;

void main() {
  vec3 vertex = a_position;
  world_vertex = (u_modelmatrix * vec4((vertex*0.5+0.5) + a_location, 1.)).xyz;
  gl_Position = u_perspectivematrix * u_viewmatrix * vec4(world_vertex, 1);
  gl_PointSize = 3.0;
  intensity = a_intensity;

  // derive eye location in world space from current view matrix:
	// (could pass this in as a uniform instead...)
	eyepos = -(u_viewmatrix[3].xyz)*mat3(u_viewmatrix);
  mat4 viewprojectionmatrix = u_perspectivematrix * u_viewmatrix;
  // we want the raymarching to operate in object-local space:
	ray_origin = vertex;
	ray_direction = world_vertex - eyepos; 
}`,
  `#version 300 es
precision mediump float;
in vec4 intensity;
in vec3 eyepos, world_vertex, ray_origin, ray_direction;
out vec4 outColor;

vec3 closest_point_on_line_segment(vec3 P, vec3 A, vec3 B) {
	vec3 AB = B-A;
	float l2 = dot(AB, AB);	// length squared
	
	if (l2 < 0.001) {
		// line is too short, just use an endpoint
		return A;
	}
	
	// Consider the line extending the segment,
	// parameterized as A + t (AB).
	// We find projection of point p onto the line.
	// It falls where t = [(AP) . (AB)] / |AB|^2
	
	vec3 AP = P-A;
	float t = dot(AP, AB) / l2;
	
	if (t < 0.0) {
		return A; 	// off A end
	} else if (t > 1.0) {
		return B; 	// off B end
	} else {
		return A + t * AB; // on segment
	}
}

float sdCapsule1(vec3 p, vec3 a, vec3 b, float r) {
	vec3 p1 = closest_point_on_line_segment(p, a, b);
	return distance(p, p1) - r;
}

float map(vec3 p) {
  
  float s = length(p) - 0.05;
  vec3 a = vec3(0.);
  vec3 b = intensity.xyz * 0.5;
  float r = 0.02;
  float c = sdCapsule1(p, a, b, r);
  return min(s, c);
}

// compute normal from a SDF gradient by sampling 4 tetrahedral points around a location "p"
// (cheaper than the usual technique of sampling 6 cardinal points)
// "fScene" should be the SDF evaluator "float distance = fScene(vec3 pos)"  
// "eps" is the distance to compare points around the location "p"
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
  vec3 rd = normalize(ray_direction);
  vec3 ro = ray_origin;

  #define STEPS 8
  #define EPS 0.01
  #define FAR 2.*sqrt(3.)

  vec3 p = ro;
  float t = 0.;
  float d = 0.;
  int steps = 0;
  int contact = 0;
  for (; steps<STEPS; steps++) {
    d = map(p);

    if (abs(d) < EPS) {
      contact = 1;
      break;
    }

    t += d;
    p = ro + t*rd;
    if (t > FAR) break;
  }

  if (contact > 0) {
    vec3 normal = normal4(p, EPS);
    float nde = abs(dot(normal, rd));
    //outColor.rgb = normal*0.5+0.5;
    outColor.rgb = vec3(1.);
    outColor.rgb = world_vertex;
    outColor.rgb *= vec3(nde);
  
    outColor.a = 0.5;
  } else {
    discard;
  }
}
`);

field.u_viewmatrix_loc = gl.getUniformLocation(field.program, "u_viewmatrix");
field.u_perspectivematrix_loc = gl.getUniformLocation(field.program, "u_perspectivematrix");
field.u_modelmatrix_loc = gl.getUniformLocation(field.program, "u_modelmatrix");

field.vao = gl.createVertexArray();
field.vertex_buf = gl.createBuffer();
field.index_buf = gl.createBuffer();
field.location_buf = gl.createBuffer();
field.intensity_buf = gl.createBuffer();
gl.bindVertexArray(field.vao);
{
  gl.bindBuffer(gl.ARRAY_BUFFER, field.vertex_buf);
  gl.bufferData(gl.ARRAY_BUFFER, cube.vertices, gl.STATIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(field.program, "a_position");
    console.log("a_position", attrLoc);
    gl.enableVertexAttribArray(attrLoc);
    gl.bindBuffer(gl.ARRAY_BUFFER, field.vertex_buf);
    gl.vertexAttribPointer(attrLoc, 3, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }
  
  gl.bindBuffer(gl.ARRAY_BUFFER, field.location_buf);
  gl.bufferData(gl.ARRAY_BUFFER, field.locations, gl.STATIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(field.program, "a_location");
    console.log("a_location", attrLoc);
    gl.enableVertexAttribArray(attrLoc);
    gl.bindBuffer(gl.ARRAY_BUFFER, field.location_buf);
    gl.vertexAttribPointer(attrLoc, 3, gl.FLOAT, false, 0, 0);
    gl.vertexAttribDivisor(attrLoc, 1);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }
  
  gl.bindBuffer(gl.ARRAY_BUFFER, field.intensity_buf);
  gl.bufferData(gl.ARRAY_BUFFER, field.intensities, gl.DYNAMIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(field.program, "a_intensity");
    console.log("a_intensity", attrLoc);
    gl.enableVertexAttribArray(attrLoc);
    gl.bindBuffer(gl.ARRAY_BUFFER, field.intensity_buf);
    gl.vertexAttribPointer(attrLoc, 4, gl.FLOAT, false, 0, 0);
    gl.vertexAttribDivisor(attrLoc, 1);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }  
  
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, field.index_buf);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, cube.indices, gl.STATIC_DRAW);
}
gl.bindVertexArray(null);

let population_size = 3000;
let agents = [];

let locations = new Float32Array(population_size * 3);
let orientations = new Float32Array(population_size * 4);
let properties = new Float32Array(population_size * 4);

function reset() {

  for (let i = 0; i < population_size; i++) {
    let a = {
      pos: vec3.create(),
      speed: 5,
      vel: vec3.create(),
      size: 0.05,
      orient: quat.create(),

      rate: 2*(Math.random()+0.5),
      phase: Math.random(),
    };
    vec3.random(a.pos, Math.random()*10);
    vec3.random(a.vel, 0.1);
    quat.random(a.orient);
    quat.normalize(a.orient, a.orient);
    agents[i] = a;
  }
}
reset();

function updateAgents(dt) {
  for (let a of agents) {

    a.phase = (a.phase + a.rate*dt) % 1;

    // do the twist
    let q1 = quat.random(quat.create());
    quat.normalize(a.orient, a.orient);
    quat.slerp(a.orient, a.orient, q1, 0.01);

    // move
    let spd = Math.max(0.1, Math.cos(a.phase * Math.PI * 2.))
    let wv = vec3.fromValues(0, spd * a.size * a.speed * dt, 0);
    quat_rotate(wv, a.orient, wv);
    vec3.add(a.pos, a.pos, wv);
    // bounds:
    if (vec3.length(a.pos) > 10 || a.pos[1] < 0) {
      vec3.set(a.pos, Math.random()*10-5, Math.random()*5, Math.random()*10-5);
    }
  }
}

let program = makeProgramFromCode(
  gl,
  `#version 300 es
uniform mat4 u_viewmatrix, u_perspectivematrix;
uniform mat3 u_viewmatrix_inverse;
uniform float u_time;
in vec3 a_position;
in vec3 a_normal;
in vec3 a_location;
in vec4 a_orientation;
in vec4 a_properties;
out vec3 normal, eyepos, ray_origin, ray_direction;
out float time, scale;
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


  scale = a_properties.w;

	vec3 vertex = a_position;
  //vertex = u_viewmatrix_inverse * vertex; 
  
  world_vertex = quat_rotate(a_orientation, vertex * scale) + a_location;
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
`,
  `#version 300 es
precision mediump float;
in vec3 normal, eyepos, ray_origin, ray_direction;
in float time, scale;
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


vec3 closest_point_on_line_segment(vec3 P, vec3 A, vec3 B) {
	vec3 AB = B-A;
	float l2 = dot(AB, AB);	// length squared
	
	if (l2 < 0.001) {
		// line is too short, just use an endpoint
		return A;
	}
	
	// Consider the line extending the segment,
	// parameterized as A + t (AB).
	// We find projection of point p onto the line.
	// It falls where t = [(AP) . (AB)] / |AB|^2
	
	vec3 AP = P-A;
	float t = dot(AP, AB) / l2;
	
	if (t < 0.0) {
		return A; 	// off A end
	} else if (t > 1.0) {
		return B; 	// off B end
	} else {
		return A + t * AB; // on segment
	}
}

float sdCapsule1(vec3 p, vec3 a, vec3 b, float r) {
	vec3 p1 = closest_point_on_line_segment(p, a, b);
	return distance(p, p1) - r;
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

  vec3 A = vec3(0, 0.7, 0);
  vec3 B = vec3(0, -0.7, 0);
  float C = sdCapsule1(p, A, B, 0.3);

  float ss = smin(s1, s2, 0.3);
  float ssc = smin(ss, C, .1);
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
  

  float distance = pow(length(world_vertex - eyepos), 2.);

  vec3 rd = normalize(ray_direction);
  vec3 ro = (ray_origin);
  vec3 nn = normal; // normalize(normal) not necessary for a cube face

  #define STEPS 32

  // 0.001 at dist=0
  // 0.05 at dist = 1
  float EPS = 0.0001 + 0.1*(distance);// * distance;
  #define FAR 2.*sqrt(3.)

  vec3 p = ro;
  int steps = 0;
  float l = length(ray_origin);
  float precise = 0.01;
  float t = 0.;
  float rsteps = 1./float(STEPS);
  float d = 0.;
  int contact = 0;
  float s = 0.;
  
  for (; steps<STEPS; steps++) {
    d = abs(map(p));

    s += min(1., EPS/(d*d));

    if (d < EPS) {
      contact++;

      if (contact == 1) {
        // first contact determines normal
        // normal in object space:
        nn = normal4(p, .001);
      }

      // move on
      d = EPS;
    }

    //if (abs(d) < precise) break;
    t += d;
    p = ro + t*rd;
    if (t > FAR) break;
  }

  float alpha = 0.;
  
  if (contact > 0) {
   // 

    // also write to depth buffer, for detailed occlusion:
    vec3 world_pos = (world_vertex + quat_rotate(world_orientation, p));
    //gl_FragDepth = computeDepth(world_pos.xyz, viewprojectionmatrix);
    // texcoord from naive normal:
    vec3 tnn = normalize(p)*0.5+0.5;
    // use this for basic marking colors:
    color = vec3(0.5, tnn.yz * 0.5+0.5);

    // in world space
    vec3 wnn = quat_rotate(world_orientation, nn);
    // use this for lighting:
    float ndl = dot(wnn, lightdir);
    color *= ndl*0.4+0.6;

    alpha = clamp(float(contact) * 0.1, 0., 1.);
    //alpha = clamp(s * 0.05, 0., 1.);
    alpha = pow(alpha, 1.2);
    
    color *= alpha;
  } else {
    discard;
    //gl_FragDepth = 0.99999;
  }


	outColor = vec4(color, alpha);
}
`
);
let u_viewmatrix_loc = gl.getUniformLocation(program, "u_viewmatrix");
let u_viewmatrix_inverse_loc = gl.getUniformLocation(
  program,
  "u_viewmatrix_inverse"
);
let u_perspectivematrix_loc = gl.getUniformLocation(
  program,
  "u_perspectivematrix"
);
let u_time_loc = gl.getUniformLocation(program, "u_time");

let vao = gl.createVertexArray();
let vertex_buf = gl.createBuffer();
let normal_buf = gl.createBuffer();
let index_buf = gl.createBuffer();
let location_buf = gl.createBuffer();
let orientation_buf = gl.createBuffer();
let property_buf = gl.createBuffer();
gl.bindVertexArray(vao);
{
  gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buf);
  gl.bufferData(gl.ARRAY_BUFFER, cube.vertices, gl.DYNAMIC_DRAW);
  let positionAttributeLocation = gl.getAttribLocation(program, "a_position");
  gl.enableVertexAttribArray(positionAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buf);
  gl.vertexAttribPointer(positionAttributeLocation, 3, gl.FLOAT, false, 0, 0);
  gl.bindBuffer(gl.ARRAY_BUFFER, null);

  gl.bindBuffer(gl.ARRAY_BUFFER, location_buf);
  gl.bufferData(gl.ARRAY_BUFFER, locations, gl.DYNAMIC_DRAW);
  let locationAttributeLocation = gl.getAttribLocation(program, "a_location");
  gl.enableVertexAttribArray(locationAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, location_buf);
  gl.vertexAttribPointer(locationAttributeLocation, 3, gl.FLOAT, false, 0, 0);
  gl.vertexAttribDivisor(locationAttributeLocation, 1);
  gl.bindBuffer(gl.ARRAY_BUFFER, null);

  gl.bindBuffer(gl.ARRAY_BUFFER, orientation_buf);
  gl.bufferData(gl.ARRAY_BUFFER, orientations, gl.DYNAMIC_DRAW);
  let orientationAttributeLocation = gl.getAttribLocation(program, "a_orientation");
  gl.enableVertexAttribArray(orientationAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, orientation_buf);
  gl.vertexAttribPointer(orientationAttributeLocation, 4, gl.FLOAT, false, 0, 0);
  gl.vertexAttribDivisor(orientationAttributeLocation, 1);
  gl.bindBuffer(gl.ARRAY_BUFFER, null);

  gl.bindBuffer(gl.ARRAY_BUFFER, property_buf);
  gl.bufferData(gl.ARRAY_BUFFER, properties, gl.DYNAMIC_DRAW);
  let propertyAttributeLocation = gl.getAttribLocation(program, "a_properties");
  gl.enableVertexAttribArray(propertyAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, property_buf);
  gl.vertexAttribPointer(propertyAttributeLocation, 4, gl.FLOAT, false, 0, 0);
  gl.vertexAttribDivisor(propertyAttributeLocation, 1);
  gl.bindBuffer(gl.ARRAY_BUFFER, null);

  gl.bindBuffer(gl.ARRAY_BUFFER, normal_buf);
  gl.bufferData(gl.ARRAY_BUFFER, cube.normals, gl.STATIC_DRAW);
  let normalAttributeLocation = gl.getAttribLocation(program, "a_normal");
  gl.enableVertexAttribArray(normalAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, normal_buf);
  gl.vertexAttribPointer(normalAttributeLocation, 3, gl.FLOAT, false, 0, 0);
  gl.bindBuffer(gl.ARRAY_BUFFER, null);

  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, index_buf);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, cube.indices, gl.STATIC_DRAW);
}
gl.bindVertexArray(null);

function updateBuffers() {
  for (let i in agents) {
    let a = agents[i];
    locations.set(a.pos, i * 3);
    orientations.set(a.orient, i * 4);

    let props = [a.phase, 0, 0, a.size];
    properties.set(props, i * 4);

  }
  
  gl.bindBuffer(gl.ARRAY_BUFFER, field.intensity_buf);
  gl.bufferData(gl.ARRAY_BUFFER, field.intensities, gl.DYNAMIC_DRAW);

  gl.bindBuffer(gl.ARRAY_BUFFER, location_buf);
  gl.bufferData(gl.ARRAY_BUFFER, locations, gl.DYNAMIC_DRAW);
  gl.bindBuffer(gl.ARRAY_BUFFER, orientation_buf);
  gl.bufferData(gl.ARRAY_BUFFER, orientations, gl.DYNAMIC_DRAW);
  gl.bindBuffer(gl.ARRAY_BUFFER, property_buf);
  gl.bufferData(gl.ARRAY_BUFFER, properties, gl.DYNAMIC_DRAW);
}
updateBuffers();

let t0 = performance.now() * 0.001;
let t = t0;
let fpsAvg = 60;
function update() {
  let t = performance.now() * 0.001;
  let dt = t - t0;
  t0 = t;
  let fps = 1/Math.max(0.001, dt);
  fpsAvg += 0.1*(fps-fpsAvg);
  document.getElementById('log').textContent = `${Math.round(fpsAvg)}fps  ${canvas.width}x${canvas.height} @${Math.floor(t)} avg ${rxAvg/(1024*1024)} mb at ${(received/t)/(1024*1024)} mbps`;
  

  updateAgents(dt);
  updateBuffers();
}

function draw() {
  let perspective_matrix = vr.projectionMat;
  let view_matrix = vr.viewMat;
  let view3 = mat3.fromMat4(mat3.create(), view_matrix);
  let view_matrix_inverse = mat3.invert(mat3.create(), view3);
  // gl.useProgram(program);
  // gl.uniform1f(u_time_loc, t);
  // gl.uniformMatrix4fv(u_viewmatrix_loc, false, view_matrix);
  // gl.uniformMatrix3fv(u_viewmatrix_inverse_loc, false, view_matrix_inverse);
  // gl.uniformMatrix4fv(u_perspectivematrix_loc, false, perspective_matrix);
  // gl.bindVertexArray(vao);
  // let instanceCount = locations.length / 3;
  // gl.drawElementsInstanced(
  //   gl.TRIANGLES,
  //   cube.indices.length,
  //   gl.UNSIGNED_SHORT,
  //   0,
  //   instanceCount
  // );
  // gl.bindVertexArray(null);
  // gl.useProgram(null);
  
  
  if (0) {
    gl.useProgram(field.program);
    gl.uniformMatrix4fv(field.u_modelmatrix_loc, false, field_matrix);
    gl.uniformMatrix4fv(field.u_viewmatrix_loc, false, view_matrix);
    gl.uniformMatrix4fv(field.u_perspectivematrix_loc, false, perspective_matrix);
    gl.bindVertexArray(field.vao);
    //gl.drawArrays(gl.LINES, 0, DIM3);
    //gl.drawElements(gl.POINTS, field.indices.length, gl.UNSIGNED_SHORT, 0);
    gl.drawElementsInstanced(gl.TRIANGLES, cube.indices.length, gl.UNSIGNED_SHORT, 0, DIM3);
    gl.bindVertexArray(null);
    gl.useProgram(null);
  }
  
  // // enable blending:
  gl.disable(gl.CULL_FACE);
  //gl.disable(gl.DEPTH_TEST)
  gl.enable(gl.BLEND);
  gl.depthMask(false);
  // additive (for black background):
  // first arg can be any of: ONE, SRC_COLOR, SRC_ALPHA, or ONE_MINUS_SRC_ALPHA
  gl.blendFunc(gl.ONE, gl.ONE);
  //gl.blendFunc(gl.SRC_COLOR, gl.ONE);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
  // multiplicative (for white background):
  // 2nd arg can by any of: SRC_COLOR, SRC_ALPHA, ONE_MINUS_SRC_COLOR, or ONE_MINUS_SRC_ALPHA
  //gl.blendFunc(gl.ZERO, gl.SRC_ALPHA);

  if (1) {
    gl.useProgram(program);
    gl.uniform1f(u_time_loc, t);
    gl.uniformMatrix4fv(u_viewmatrix_loc, false, view_matrix);
    gl.uniformMatrix3fv(u_viewmatrix_inverse_loc, false, view_matrix_inverse);
    gl.uniformMatrix4fv(u_perspectivematrix_loc, false, perspective_matrix);
    gl.bindVertexArray(vao);
    let instanceCount = locations.length / 3;
    gl.drawElementsInstanced(
      gl.TRIANGLES,
      cube.indices.length,
      gl.UNSIGNED_SHORT,
      0,
      instanceCount
    );
    gl.bindVertexArray(null);
    gl.useProgram(null);
  }
  
  
  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.CULL_FACE);
  gl.disable(gl.BLEND);
  gl.depthMask(true);
}

/////////////////////


/// socket stuff

let received = 0;
let rxAvg = 0;

let sock
try {
	if (window.location.hostname == "localhost") {
		sock = new Socket({
			reload_on_disconnect: false,
			onopen: function() {
				//this.send({ cmd: "getdata", date: Date.now() });
			},
			onmessage: function(msg) { 
				print("received", msg);
			},
			onbuffer(data, byteLength) {
				received += byteLength;
				rxAvg += 0.1*(byteLength - rxAvg);
				let rx = new Float32Array(data);
				//field.intensities.set(rx);

				if (Math.random() < 0.01) {
					console.log(data.byteLength, field.intensities.byteLength, rx.byteLength)
					
					console.log(rx[rx.length-3])
					console.log(field.intensities[field.intensities.length-3])
				}

				//let rx = new Uint8Array(data);
				//console.log(rx[rx.length-1]);

				
				//console.log("received arraybuffer of " + byteLength + " bytes");
				//console.log(agentsVao.positions.byteLength + agentsVao.colors.byteLength + linesVao.indices.byteLength);
				//console.log(data)
				// copy to agentsVao:
				//let fa = new Float32Array(data);
				//agentsVao.positions = fa.subarray(0, NUM_AGENTS*2);
				//agentsVao.positions.set(fa);

				// agentsVao.positions = new Float32Array(data, 0, NUM_AGENTS*2);
				// agentsVao.colors = new Float32Array(data, agentsVao.positions.byteLength, NUM_AGENTS*4);
				// linesVao.indices = new Uint16Array(data, agentsVao.colors.byteLength + agentsVao.colors.byteOffset, MAX_LINE_POINTS);

				// dirty = true;

				//console.log(utils.pick(linesVao.indices));
			},
		});
	}
} catch (e) {
	console.error(e);
}
