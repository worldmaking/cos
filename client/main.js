
/* global: mat4, vec3, VRCubeIsland, WGLUDebugGeometry, WGLUStats, WGLUTextureLoader, VRSamplesUtil */

let isProjector = webutils.getQueryStringParamterByName("projector") == 1

let projector_calibration = {"vive_translate":[-0.3, -0.1, 0.1],"vive_euler": [0.0, 185.0, 0.0],"ground_position":[0.001474501914345,-0.116043269634247,-3.399029493331909],"ground_quat":[0.019847802817822,0.018058635294437,-0.011425343342125,0.999571561813354],"position":[-0.20009072124958,0.12004879117012,0.11839796602726],"frustum":[-0.540263652801514,0.686421573162079,-0.429053455591202,0.335920542478561,1,10],"rotatexyz":[-1.062064528465271,0.801017045974731,-1.60685408115387],"quat":[-0.009365003556013,0.007119102869183,-0.013956263661385,0.999833405017853],"ground_quat_fixed":[0.7208383332669634,0.004690445640796409,-0.020848320873468468,0.6927693018190737],"quat_ground":[-0.7208427585109026,-0.004690474435555491,0.020848448861813883,0.6927735547465508],"position_ground":[-0.001474501914345,0.116043269634247,3.399029493331909],"camera_position":[-0.33518560060247854,3.495735864008822,-0.3691928870850422],"camera_quat":[-0.7272938198015632,-0.010013291787240237,0.006000773148561227,0.686231795217466]}

window.addEventListener("keyup", function(event) {
	//print(event.key);
	if (event.key == 'f') {
		if (screenfull.enabled) {
      screenfull.toggle(document.body);

      if (vr.Display.isPresenting) {
        onVRExitPresent();
      } else if (vr.Display.capabilities.canPresent) {
        onVRRequestPresent();
      }
		}
	} else if (event.key == 'e') {

      if (vr.Display.isPresenting) {
        onVRExitPresent();
      } else if (vr.Display.capabilities.canPresent) {
        onVRRequestPresent();
      }
	} else if (event.key == " ") {
	//	running = !running;
	} else if (event.key == "r") {
	//	sock.send({cmd: "reset"});
	// } else if (event.key == "m") {
	// 	showmap = !showmap; //agents.sort((a, b) => b.reward - a.reward);
	// } else if (event.key == "l") {
	// 	showlines = !showlines;
	// } else if (event.key == "i") {
	// 	slab_composite_invert = (slab_composite_invert) ? 0 : 1;
	// } else if (event.key == "s") {
	// 	// `frame${frame.toString().padStart(5, '0')}.png`;
	// 	saveCanvasToPNG(canvas, "result");
	}
}, false);

let isReceivingData = false;

// ===================================================
// WebGL scene setup. This code is not WebVR specific.
// ===================================================
// WebGL setup.
let canvas = document.getElementById("webgl-canvas");
let gl = canvas.getContext("webgl2", { alpha: false, preserveDrawingBuffer: true, });;
vr.init(canvas, gl, !isProjector);
vr.showStats = false;

/////////////////////////

let cube = makeCube();

/*
  For VR want to measure in meters for greatest convenience
  Voxel field needs to cover whole space
  Typically origin will be centre of floor
  
  A good room-scale would be maybe 6x6m
  For a 32^3 field, if we make voxels every 20cm, it covers 6.4m. 
*/

const HDIM = 256;
const HDIM2 = HDIM*HDIM;

let hmap = {
  modelmatrix: mat4.create(),

  locations: new Float32Array(HDIM2 * 3),
  heights: new Float32Array(HDIM2 * 1),

  // how many indices?
  //indices: new Uint16Array(),
};

let indices = [];

for (let y=0; y<HDIM; y++) { 
  for (let x=0; x<HDIM; x++) {

    let i00 = x + y*HDIM;
    let i01 = (x-1) + y*HDIM;
    let i10 = x + (y-1)*HDIM;

    if (x > 0) {
      indices.push(i01);
      indices.push(i00);
    }
    if (y > 0) {
      indices.push(i10);
      indices.push(i00);
    }

    let zoom = 2;
    let val = [((x/HDIM) * 2 - 1) * zoom, 0, ((y/HDIM) * 2 -1) * zoom];
    hmap.locations.set(val, i00 * 3);
  } 
} 
hmap.indices = new Uint16Array(indices)

hmap.program = makeProgramFromCode(
  gl,
  `#version 300 es
uniform mat4 u_viewmatrix, u_perspectivematrix, u_modelmatrix;
uniform float u_setting;
in vec3 a_location;
in float a_height;
out vec3 vertex;
out float brightness;

${vertex_shader_lib}

void main() {
  vertex = a_location;
  vertex.y = a_height;
  vec4 view_vertex = u_viewmatrix * u_modelmatrix * vec4(vertex, 1);
  gl_Position = u_perspectivematrix * view_vertex;
  brightness = clamp(vertex.y*4., 0., 1.);
  brightness *= 0.3;
  brightness *= clamp(length(view_vertex.xyz)*3. - 0.1, 0., 1.);
}`,
  `#version 300 es
precision mediump float;
in vec3 vertex;
in float brightness;
out vec4 outColor;

${fragment_shader_lib}

void main() {
  outColor = vec4(brightness);

  //float home = 1. - min(1., length(vertex.xz));
  //vec2 xz = mod(vertex, 1.).xz;
  //outColor = vec4(xz.x, home, xz.y, 1.);
}
`);


hmap.u_viewmatrix_loc = gl.getUniformLocation(hmap.program, "u_viewmatrix");
hmap.u_modelmatrix_loc = gl.getUniformLocation(hmap.program, "u_modelmatrix");
hmap.u_perspectivematrix_loc = gl.getUniformLocation(hmap.program, "u_perspectivematrix");
hmap.u_setting_loc = gl.getUniformLocation(hmap.program, "u_setting");

hmap.vao = gl.createVertexArray();
hmap.location_buf = gl.createBuffer();
hmap.height_buf = gl.createBuffer();
hmap.index_buf = gl.createBuffer();
gl.bindVertexArray(hmap.vao);
{
  gl.bindBuffer(gl.ARRAY_BUFFER, hmap.location_buf);
  gl.bufferData(gl.ARRAY_BUFFER, hmap.locations, gl.STATIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(hmap.program, "a_location");
    gl.enableVertexAttribArray(attrLoc);
    gl.vertexAttribPointer(attrLoc, 3, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }
  
  gl.bindBuffer(gl.ARRAY_BUFFER, hmap.height_buf);
  gl.bufferData(gl.ARRAY_BUFFER, hmap.heights, gl.DYNAMIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(hmap.program, "a_height");
    gl.enableVertexAttribArray(attrLoc);
    gl.vertexAttribPointer(attrLoc, 1, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }

  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, hmap.index_buf);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, hmap.indices, gl.STATIC_DRAW);
}
gl.bindVertexArray(null);

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

${vertex_shader_lib}

void main() {
  vec3 vertex = a_position;
  world_vertex = (u_modelmatrix * vec4((vertex*0.5+0.5) + a_location, 1.)).xyz;
  vec4 view_vertex = u_viewmatrix * vec4(world_vertex, 1);
  gl_Position = u_perspectivematrix * view_vertex;
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

${fragment_shader_lib}

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
    gl.enableVertexAttribArray(attrLoc);
    gl.bindBuffer(gl.ARRAY_BUFFER, field.vertex_buf);
    gl.vertexAttribPointer(attrLoc, 3, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }
  
  gl.bindBuffer(gl.ARRAY_BUFFER, field.location_buf);
  gl.bufferData(gl.ARRAY_BUFFER, field.locations, gl.STATIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(field.program, "a_location");
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


let population_size = 1024;
let agents = [];

let agent_attrib_count = 12; // 3x vec4
let agent_attribs = new Float32Array(population_size * agent_attrib_count);

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

    let props = [a.phase, 0, 0, a.size];
    agent_attribs.set(a.pos, i*agent_attrib_count);
    agent_attribs.set(a.orient, i*agent_attrib_count + 4);
    agent_attribs.set(props, i*agent_attrib_count + 8);
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
uniform float u_time;
in vec3 a_position;
in vec3 a_normal;
in vec4 a_location;
in vec4 a_orientation;
in vec4 a_properties;
out vec3 normal, eyepos, ray_origin, ray_direction;
out float time, scale;
out vec3 world_vertex;
out vec4 world_orientation;
out vec4 properties;
out mat4 viewprojectionmatrix;

${vertex_shader_lib}

void main() {

  scale = a_properties.w;

	vec3 vertex = a_position;
  
  world_vertex = quat_rotate(a_orientation, vertex * scale) + a_location.xyz;
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

  time = u_time + a_properties.y*3.;
  properties = a_properties;
}
`,
  `#version 300 es
precision mediump float;

${fragment_shader_lib}

uniform float u_isvr;
in vec3 normal, eyepos, ray_origin, ray_direction;
in float time, scale;
in vec3 world_vertex;
in vec4 world_orientation;
in mat4 viewprojectionmatrix;
in vec4 properties;
out vec4 outColor;

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

float sdCapsuleRipple(vec3 p, vec3 a, vec3 b, float ra, float rb, float timephase) {
  vec3 pa = p-a, ba = b-a;
  float t = dot(pa, ba) / dot(ba, ba);
  float h = clamp(t, 0., 1.); // limit line
  // dist
  vec3 rel = pa - ba*h;
  float d = length(rel);
  // ripple:
  //float h1 = h + 0.2*sin(PI * 4. * (t*t + timephase));
  //return d - mix(ra, rb, h1);
  return d - ra * (1. + 0.2*sin(TWOPI * (t + timephase)));
}

float map(vec3 p) {
  float t1 = (cos(time * -TWOPI)*0.2+0.8);
  float t2 = (sin(time * -TWOPI)*0.3+0.7);
  //float s = fSphere(p, t1);
  float s1 = fSphere(p+vec3(0, 0, 0.3), 0.7*t1);
  float s2 = fSphere(p+vec3(0, 0, 0.3), 0.6*t2);
  float sh = max(s1, -s2);
  //float c = fCylinder(p.xzy, 0.2, 1.);
  // float b = fBox(p, vec3(0.5, 1., 0.1));
  // float sc = smin(s, c, 0.2);
  // float cb = smin(c, b, 0.2);
  // float bs = smin(b, s, 0.2);

  vec3 A = vec3(0, 0, 0.7);
  vec3 B = vec3(0, 0, -0.7);
  float C1 = sdCapsule1(p, A, B, 0.3);
  //float C1 = sdCapsuleRipple(p, A, B, 0.3, 0.3, time);
  float C2 = sdCapsule1(p, A, B, 0.1);
  float C = max(C1, -C2);

  //float ss = smin(s1, s2, 0.3);
  float ssc = smin(sh, C, .1);

  float e0 = fSphere(p+vec3(+0.4, 0., 0.7), 0.3);
  float e1 = fSphere(p+vec3(-0.4, 0., 0.7), 0.3);
  float es = min(e0, e1);
  float final = min(ssc, es);
  //final = max(final, -c);

 //final = s;

  return final + 0.02 * (1. + sin(TWOPI * (p.z*2. + abs(p.x) + time)));; //min(ssc, es);
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
  vec3 lightdir = vec3(0, 4, 0) - world_vertex;
  vec3 color = vec3(1.);
  

  float distance_vr = length(world_vertex - eyepos);
  float distance_floor = world_vertex.y;
  float distance = u_isvr != 0. ? distance_vr : distance_floor;

  vec3 rd = normalize(ray_direction);

  //rd.y += 0.1*sin(time);

  vec3 ro = (ray_origin);
  vec3 nn = normal; // normalize(normal) not necessary for a cube face

  #define STEPS 32

  // 0.001 at dist=0
  // 0.05 at dist = 1
  float EPS = 0.0001 + 0.05*(distance*distance);// * distance;
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
  
  if (contact > 0 && (u_isvr != 0. || world_vertex.y > 0.)) {
   // 

    // also write to depth buffer, for detailed occlusion:
    vec3 world_pos = (world_vertex + quat_rotate(world_orientation, p));
    //gl_FragDepth = computeDepth(world_pos.xyz, viewprojectionmatrix);
    // texcoord from naive normal:
    vec3 tnn = normalize(p)*0.5+0.5;
    // in world space
    vec3 wnn = quat_rotate(world_orientation, nn);

    color = vec3(0.8, wnn.yz*0.25+0.75);
    color = mix (vec3(0.9), color, u_isvr);
    //color *= properties.xyz;

    // reflect the light from above:
    color *= dot(wnn, lightdir)*0.4+0.6;
    // a simple kind of fresnel:
    color *= dot(nn, rd)*0.4+0.6;

    alpha = clamp(float(contact) * 0.1, 0., 1.);
    //alpha = clamp(s * 0.05, 0., 1.);
    alpha = pow(alpha, 1.2);
    
    // TODO: projector fade if u_isvr == 0.;
    alpha = mix(1. - (alpha / (1. + world_vertex.y*0.5)), alpha, u_isvr);

    
    //color *= alpha;
    
    //color = vec3(-properties.x);
    outColor = vec4(color, alpha);



  } else {
    discard;
    //gl_FragDepth = 0.99999;
  }


}
`
);
let u_viewmatrix_loc = gl.getUniformLocation(program, "u_viewmatrix");
let u_perspectivematrix_loc = gl.getUniformLocation(
  program,
  "u_perspectivematrix"
);
let u_time_loc = gl.getUniformLocation(program, "u_time");
let u_isvr_loc = gl.getUniformLocation(program, "u_isvr");

let vao = gl.createVertexArray();
let vertex_buf = gl.createBuffer();
let normal_buf = gl.createBuffer();
let index_buf = gl.createBuffer();

let location_buf = gl.createBuffer();
let orientation_buf = gl.createBuffer();
let property_buf = gl.createBuffer();

let attribs_buf = gl.createBuffer();
gl.bindVertexArray(vao);
{
  gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buf);
  gl.bufferData(gl.ARRAY_BUFFER, cube.vertices, gl.DYNAMIC_DRAW);
  let positionAttributeLocation = gl.getAttribLocation(program, "a_position");
  gl.enableVertexAttribArray(positionAttributeLocation);
  gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buf);
  gl.vertexAttribPointer(positionAttributeLocation, 3, gl.FLOAT, false, 0, 0);
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

  // instance data:
  gl.bindBuffer(gl.ARRAY_BUFFER, attribs_buf);
  gl.bufferData(gl.ARRAY_BUFFER, agent_attribs, gl.DYNAMIC_DRAW);
  {
    let attrLoc = gl.getAttribLocation(program, "a_location");
    gl.enableVertexAttribArray(attrLoc);
    gl.vertexAttribPointer(attrLoc, 4, gl.FLOAT, false, agent_attrib_count*4, 0*4);
    gl.vertexAttribDivisor(attrLoc, 1);
  }
  {
    let attrLoc = gl.getAttribLocation(program, "a_orientation");
    gl.enableVertexAttribArray(attrLoc);
    gl.vertexAttribPointer(attrLoc, 4, gl.FLOAT, false, agent_attrib_count*4, 4*4);
    gl.vertexAttribDivisor(attrLoc, 1);
  }
  {
    let attrLoc = gl.getAttribLocation(program, "a_properties");
    gl.enableVertexAttribArray(attrLoc);
    gl.vertexAttribPointer(attrLoc, 4, gl.FLOAT, false, agent_attrib_count*4, 8*4);
    gl.vertexAttribDivisor(attrLoc, 1);
  }
  gl.bindBuffer(gl.ARRAY_BUFFER, null);
}
gl.bindVertexArray(null);

let once = 1
function updateBuffers() {
  if (!isReceivingData) {
    for (let i in agents) {
      let a = agents[i];
      let props = [a.phase, 0, 0, a.size];
      agent_attribs.set(a.pos, i*agent_attrib_count);
      agent_attribs.set(a.orient, i*agent_attrib_count + 4);
      agent_attribs.set(props, i*agent_attrib_count + 8);
    }
  }

  //gl.bindBuffer(gl.ARRAY_BUFFER, field.intensity_buf);
  //gl.bufferData(gl.ARRAY_BUFFER, field.intensities, gl.DYNAMIC_DRAW);

  gl.bindBuffer(gl.ARRAY_BUFFER, attribs_buf);
  gl.bufferData(gl.ARRAY_BUFFER, agent_attribs, gl.DYNAMIC_DRAW);

  gl.bindBuffer(gl.ARRAY_BUFFER, hmap.height_buf);
  gl.bufferData(gl.ARRAY_BUFFER, hmap.heights, gl.DYNAMIC_DRAW);
}
updateBuffers();

let t0 = performance.now() * 0.001;
let t = t0;
let fpsAvg = 60;
function update() {
  t = performance.now() * 0.001;
  let dt = t - t0;
  t0 = t;
  let fps = 1/Math.max(0.001, dt);
  fpsAvg += 0.1*(fps-fpsAvg);
  //document.getElementById('log').textContent = `${Math.round(fpsAvg)}fps  ${canvas.width}x${canvas.height} @${Math.floor(t)} avg ${rxAvg/(1024*1024)} mb at ${(received/t)/(1024*1024)} mbps`;

  if (!isReceivingData) updateAgents(dt);
  updateBuffers();
}

if (isProjector) {
  
  gl.clearColor(1., 1., 1., 1.0);
}

function draw(vr, isInVR) {
  let perspective_matrix = vr.projectionMat;
  let view_matrix = vr.viewMat;

  if (isProjector) {

    vr.canvas.width = 1920;
    vr.canvas.height = 1200;
    let pview = mat4.fromQuat(mat4.create(), projector_calibration.camera_quat);
    let ppos = mat4.fromTranslation(mat4.create(), projector_calibration.camera_position);

    mat4.identity(view_matrix);
    mat4.multiply(view_matrix, ppos, pview)

    mat4.invert(view_matrix, view_matrix)

    let up = quat_uy([], projector_calibration.camera_quat);

    let near = 0.1;
    mat4.frustum(perspective_matrix, 
      near*projector_calibration.frustum[0], 
      near*projector_calibration.frustum[1], 
      near*projector_calibration.frustum[2], 
      near*projector_calibration.frustum[3], 
      near, 10.);

  } else {
    // adjust vr viewmat to match the kinect world space:
    let vive_mat = mat4.fromRotationTranslation(
      mat4.create(), 
      quat.fromEuler(quat.create(), 
        projector_calibration.vive_euler[0], 
        projector_calibration.vive_euler[1], 
        projector_calibration.vive_euler[2]
      ), 
      vec3.fromValues(
        projector_calibration.vive_translate[0], 
        projector_calibration.vive_translate[1], 
        projector_calibration.vive_translate[2]
      )
    );
    mat4.multiply(view_matrix, view_matrix, vive_mat);

    
    
  }

  //vr.cubeIsland.render(vr.projectionMat, vr.viewMat, vr.stats);
  if (0) {
    // To ensure that the FPS counter is visible in VR mode we have to
    // render it as part of the scene.
    mat4.fromTranslation(vr.cubeIsland.statsMat, [0, 1.5, -vr.cubeIsland.depth * 0.5]);
    mat4.scale(vr.cubeIsland.statsMat, vr.cubeIsland.statsMat, [0.5, 0.5, 0.5]);
    mat4.rotateX(vr.cubeIsland.statsMat, vr.cubeIsland.statsMat, -0.75);
    mat4.multiply(vr.cubeIsland.statsMat, view_matrix, vr.cubeIsland.statsMat);
    vr.stats.render(perspective_matrix, vr.cubeIsland.statsMat);
  }

  drawscene(perspective_matrix, view_matrix, isInVR);

  if (!isProjector) {
    mat4.translate(view_matrix, view_matrix, vec3.fromValues(0, -4, 0));
    mat4.rotateY(view_matrix, view_matrix, Math.PI);
    drawscene(perspective_matrix, view_matrix, isInVR);
  }
}

function drawscene(perspective_matrix, view_matrix, isInVR) {

  // if (0) {
  //   gl.useProgram(field.program);
  //   gl.uniformMatrix4fv(field.u_modelmatrix_loc, false, field_matrix);
  //   gl.uniformMatrix4fv(field.u_viewmatrix_loc, false, view_matrix);
  //   gl.uniformMatrix4fv(field.u_perspectivematrix_loc, false, perspective_matrix);
  //   gl.bindVertexArray(field.vao);
  //   //gl.drawArrays(gl.LINES, 0, DIM3);
  //   //gl.drawElements(gl.POINTS, field.indices.length, gl.UNSIGNED_SHORT, 0);
  //   gl.drawElementsInstanced(gl.TRIANGLES, cube.indices.length, gl.UNSIGNED_SHORT, 0, DIM3);
  //   gl.bindVertexArray(null);
  //   gl.useProgram(null);
  // }

  // // enable blending:
  gl.disable(gl.CULL_FACE);
  //gl.disable(gl.DEPTH_TEST)
  gl.enable(gl.BLEND);
  gl.depthMask(false);
  if (isProjector) {
      
  // multiplicative (for white background):
  // 2nd arg can by any of: SRC_COLOR, SRC_ALPHA, ONE_MINUS_SRC_COLOR, or ONE_MINUS_SRC_ALPHA
    gl.blendFunc(gl.ZERO, gl.SRC_ALPHA);
  } else {
    
    // additive (for black background):
    // first arg can be any of: ONE, SRC_COLOR, SRC_ALPHA, or ONE_MINUS_SRC_ALPHA
    gl.blendFunc(gl.ONE, gl.ONE);
    //gl.blendFunc(gl.SRC_COLOR, gl.ONE);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
  }

  if (!isProjector) {
    gl.useProgram(hmap.program);
    gl.uniformMatrix4fv(hmap.u_modelmatrix_loc, false, hmap.modelmatrix);
    gl.uniformMatrix4fv(hmap.u_viewmatrix_loc, false, view_matrix);
    gl.uniformMatrix4fv(hmap.u_perspectivematrix_loc, false, perspective_matrix);
    gl.uniform1f(hmap.u_setting_loc, isProjector ? 1. : 0.);
    gl.bindVertexArray(hmap.vao);
    //gl.drawArrays(gl.POINTS, 0, HDIM2);
    gl.drawElements(gl.LINES, hmap.indices.length, gl.UNSIGNED_SHORT, 0);
    //gl.drawElementsInstanced(gl.POINTS, hmap.locations.length, gl.UNSIGNED_SHORT, 0, DIM3);
    gl.bindVertexArray(null);
    gl.useProgram(null);
  }

  if (1) {
    gl.useProgram(program);
    gl.uniform1f(u_time_loc, t);
    gl.uniform1f(u_isvr_loc, isInVR ? 1. : 0.);
    gl.uniformMatrix4fv(u_viewmatrix_loc, false, view_matrix);
    gl.uniformMatrix4fv(u_perspectivematrix_loc, false, perspective_matrix);
    gl.bindVertexArray(vao);
    let instanceCount = agent_attribs.length / agent_attrib_count; 
    gl.drawElementsInstanced(gl.TRIANGLES, cube.indices.length, gl.UNSIGNED_SHORT, 0, instanceCount);
    gl.bindVertexArray(null);
    gl.useProgram(null);
  }
  
  
  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.CULL_FACE);
  gl.disable(gl.BLEND);
  gl.depthMask(true);
}

/////////////////////

let received = 0;
let rxAvg = 0;

/// socket stuff

let onetime = 1

let sock
let max_sock
try {
	if (window.location.hostname == "localhost") {
		sock = new Socket({
			reload_on_disconnect: true,
      reconnect_period: 10000,
			onopen: function() {
        //this.send({ cmd: "getdata", date: Date.now() });
			},
			onmessage: function(msg) { 
        console.log("received", msg);
        if (msg.message == "config") {
          projector_calibration = msg.projector_calibration;
        }
			},
			onbuffer(data, byteLength) {
        //console.log (byteLength);
        isReceivingData = true;

				received += byteLength;
				rxAvg += 0.1*(byteLength - rxAvg);
        agent_attribs.set(new Float32Array(data));
			},
    });
    
    maxsock = new Socket({
      port: 7474,
      reload_on_disconnect: false,
      reconnect_period: 10000,
			onopen: function() {
				//this.send({ cmd: "getdata", date: Date.now() });
			},
			onmessage: function(msg) { 
        console.log("received", msg);
        if (msg.message == "config") {
          projector_calibration = msg.projector_calibration;
        }
			},
			onbuffer(data, byteLength) {
        //console.log (byteLength , hmap.heights.byteLength);
        hmap.heights.set(new Float32Array(data));
        
			},
		});
	}
} catch (e) {
	console.error(e);
}
