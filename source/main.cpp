#include <iostream>
#include <chrono>
#include <ctime>

// note: GLAD header generated from http://glad.dav1d.de/ with version 3.3 compat
// TODO switch to non _debug.h/.c for production
#include <glad/glad_debug.h>
#define _GLFW_USE_DWM_SWAP_INTERVAL 1
#include <GLFW/glfw3.h>

int console_log(const char *fmt, ...);
int console_error(const char *fmt, ...);

#include "al_glm.h"
#include "al_math.h"
#include "al_gl.h"
#include "al_obj.h"
#include "al_xyzrgb.h"
#include "htcvive.h"
#include "al_kinect1.h"
#include "shared.h"

#include "oscpkt/oscpkt.hh"
#include "oscpkt/udp.hh"

#ifdef USE_SPOUT
#include "SpoutLibrary.h"

SPOUTHANDLE spout;
#endif

// GLAD_DEBUG is only defined if the c-debug generator was used
#ifdef GLAD_DEBUG
// logs every gl call to the console
void pre_gl_call(const char *name, void *funcptr, int len_args, ...) {
	console_log("Calling: %s (%d arguments)", name, len_args);
}
#endif

const int PORT_NUM = 7474;

bool singlethread = 0;

glm::quat hand_camera_quat = glm::quat(glm::vec3(0., 0.f, float(M_PI)));

std::time_t rec_start, rec_end, play_start, play_end;

#ifdef AL_WIN
bool show_shadow = true;
// #define USE_KINECTS 1
#else
bool show_shadow = false;
#endif

Shared shared;
bool threads_done = false;
double simulation_thread_dt = 1. / 60.;
GLFWwindow * window = 0;
int width, height;
int savedWidth, savedHeight;
bool isFullScreen = 0;
glm::vec3 keynav;
glm::vec3 keyrot;
glm::vec3 viewposition = glm::vec3(0.0f, 1.7f, 4.f);
glm::quat viewquat;
Kinect1 kinects[2];
VR vive;
oscpkt::UdpSocket sock;
int cameradata_frame = 0;

glm::vec3 k0_pos = glm::vec3(-0.15, 3.8, -0.15); // W
glm::vec3 k1_pos = glm::vec3(-0., 3.8, -0.0); // E
// jitter uses xyzw format
// glm:: uses wxyz format
// xyzw -> wxyz
//glm::quat k0_quat = glm::quat(0.603785, 0.373893, 0.570518, -0.4125);
glm::quat k0_quat = glm::quat(-0.4125, 0.603785, 0.373893, 0.570518);
//glm::quat k0_quat = glm::quat(0.570518 , -0.4125, 0.603785, 0.373893);
//glm::quat k1_quat = glm::quat( -0.428157, -0.565136, -0.417748, 0.568146);
//glm::quat k1_quat = glm::quat(0.568146, -0.428157, -0.565136, -0.417748);
glm::quat k1_quat = glm::quat(-0.417748, 0.568146, -0.428157, -0.565136);


#ifdef AL_WIN
XYZRGB sema; // ("cloud_data/xyzrgb_all_lo.txt");
#else 
XYZRGB sema;//("cloud_data/xyzrgb_all_hi_clean.txt");
#endif
XYZRGB::Point sema_points[DUST_MAX];

static int dustidx = 0;

void simulation_thread_routine(double dt) {
	
	if (sema.points.size()) {
		for (int i = 0; i < 256; i++) {
			int src_i = random::integer((int)sema.points.size());
			int dst_i = random::integer((int)DUST_MAX);
			sema_points[dst_i] = sema.points[src_i];
		}
	}

	shared.simulation_update(dt);
	
	if (shared.playback) {
		
		// load it:
		char filename[128];
		sprintf(filename, "capture/CameraData-%05d.bin", cameradata_frame++);
		auto myfile = std::fstream(filename, std::fstream::in | std::fstream::binary);
    	if (myfile.good()) {

    		//console_log("reading %s", filename);
    		myfile.read((char*)&shared.cameradata, sizeof(CameraData));

			if (cameradata_frame == 1) {
				console_log("shot began at time %s", std::asctime(std::localtime(&shared.cameradata.timestamp)));
				play_start = shared.cameradata.timestamp;
				rec_start = time(nullptr);
			}

    		
			
    		// copy state:
			if (0) {
				viewposition = glm::mix(viewposition, shared.cameradata.pos_hand1, 0.25f);
				glm::quat q = hand_camera_quat * shared.cameradata.quat_hand1;
				viewquat = glm::normalize(glm::slerp(viewquat, q, 0.25f));
			} else if (0) {
				viewposition = glm::mix(viewposition, shared.cameradata.pos_hand2, 0.25f);
				glm::quat q = hand_camera_quat * shared.cameradata.quat_hand2;
				viewquat = glm::normalize(glm::slerp(viewquat, q, 0.25f));
			} else if (1) {
				viewposition = glm::mix(viewposition, shared.cameradata.pos_head, 0.25f);
				glm::quat q = shared.cameradata.quat_head;
				viewquat = glm::normalize(glm::slerp(viewquat, q, 0.25f));
			}		
			memcpy(kinects[0].cloud_mat.data, shared.cameradata.k0, sizeof(shared.cameradata.k0));
			memcpy(kinects[1].cloud_mat.data, shared.cameradata.k1, sizeof(shared.cameradata.k1));
		
			// TODO: handle timestamps...
    	} else {
    		// loop or rewind?
    		//shared.playback = 0;
			console_log("shot ended at time %s", std::asctime(std::localtime(&shared.cameradata.timestamp)));

    		cameradata_frame = 0;

			play_end = shared.cameradata.timestamp;
			rec_end = time(nullptr);

			console_log("playtime %d rectime %d", std::difftime(play_end, play_start), std::difftime(rec_end, rec_start)) ;
    	}
		myfile.close();
		
	} else if (shared.recording) {
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// copy state data to be saved:
		if (vive.connected) {
			shared.cameradata.pos_head = vive.mTrackedPosition;
			shared.cameradata.pos_hand1 = vive.mHandTrackedPosition[0];
			shared.cameradata.pos_hand2 = vive.mHandTrackedPosition[1];		
			shared.cameradata.quat_head = vive.mTrackedQuat;
			shared.cameradata.quat_hand1 = vive.mHandTrackedQuat[0];
			shared.cameradata.quat_hand2 = vive.mHandTrackedQuat[1];
		} else {
			shared.cameradata.pos_head = viewposition;
			shared.cameradata.pos_hand1 = viewposition;
			shared.cameradata.pos_hand2 = viewposition;		
			shared.cameradata.quat_head = viewquat;
			shared.cameradata.quat_hand1 = viewquat;
			shared.cameradata.quat_hand2 = viewquat;
		}
		memcpy(shared.cameradata.k0, kinects[0].cloud_mat.data, sizeof(shared.cameradata.k0));
		memcpy(shared.cameradata.k1, kinects[1].cloud_mat.data, sizeof(shared.cameradata.k1));
		shared.cameradata.timestamp = time(nullptr);
		
		// save it:
		char filename[128];
		sprintf(filename, "capture/CameraData-%05d.bin", cameradata_frame++);
		auto myfile = std::fstream(filename, std::ios::out | std::ios::binary);
		myfile.write((char*)&shared.cameradata, sizeof(CameraData));
		myfile.close();
		
		auto endTime = std::chrono::high_resolution_clock::now();
		float elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	}
	
	if (sock.isOk()) {
		
		oscpkt::PacketWriter pw;
		pw.startBundle();
		
		for (int i = 0; i < STALKS_MAX; i++) {
			Stalk& o = shared.stalks[i];

			// read density at stalk location
			glm::vec4 d = shared.density.sample(o.normalized_pos);
			float intensity = d.w;

			oscpkt::Message msg("/bell"); 
			msg.pushInt32(o.ndnode);
			msg.pushInt32(o.len);
			msg.pushFloat(intensity);
			pw.addMessage(msg);

			//if (intensity > 0.1) console_log("stalk %d id %d intensity %f", i, o.ndnode, intensity);
		}
		pw.endBundle();
		bool ok = sock.sendPacket(pw.packetData(), pw.packetSize());
	}
}

#ifdef AL_WIN
DWORD simulation_thread_id;
HANDLE simulation_thread_handle;
DWORD WINAPI simulation_thread(LPVOID lpParameter)
{
	sock.connectTo("localhost", PORT_NUM);
	if (!sock.isOk()) {
		console_log("failed to connect to UDP");
	}
	
	kinects[0].cloudTransform = glm::translate(glm::mat4(1.f), -k0_pos) * mat4_cast((k0_quat)); // far away
	kinects[1].cloudTransform = glm::translate(glm::mat4(1.f), -k1_pos) * mat4_cast((k1_quat)); // far away

#ifdef USE_KINECTS
	kinects[0].start();
	kinects[1].start();
#endif

	oscpkt::UdpSocket sock;
	sock.connectTo("localhost", PORT_NUM);
	
	while (!threads_done) {
		double sim_dt = 1. / 60.;
		auto start = std::chrono::high_resolution_clock::now();

#ifdef USE_KINECTS
		kinects[0].step();
		kinects[1].step();
		// copy some kinect points into the dust field:
		int elems = glm::min(DUST_MAX, KINECT_DEPTH_HEIGHT * KINECT_DEPTH_WIDTH);
		for (int i = 0; i < elems; i+=2) {
			shared.dust[i].pos = kinects[0].cloud_mat.data[i*3];
			shared.dust[i+1].pos = kinects[1].cloud_mat.data[i * 3];
		}
		for (int i = 0; i < KINECT_DEPTH_HEIGHT * KINECT_DEPTH_WIDTH; i++) {
			// convert to land cell coordinate:
			for (int k = 0; k < 2; k++) {
				glm::vec3 p = kinects[k].cloud_mat.data[i];
				glm::vec3 q = mat4_transform(WorldMatrixInverse, p);
				if (p.y > 0.4 && p.y < 2. && p.z > -2. && p.z < 2.
					&& q.y >= 0. && q.y < 1. && q.x >= 0. && q.x < 1. && q.z >= 0. && q.z < 1.) {
					int idx = shared.land_cells.index_from_texcoord(glm::vec2(q.x, q.z));
					float y = shared.land_cells.data[idx].position.y;
					//if (p.y > y) {
						//shared.land_cells.data[idx].position.y = p.y;
					//}
					//else {
						shared.land_cells.data[idx].position.y = y + 0.25 * (p.y - y);
					//}
						if (0) {
							if (random::integer(1000) == 0) {
								shared.dust[dustidx].pos = p;
								dustidx = (dustidx + 1) % DUST_MAX;
							}
						}
				}
			}
		}
#endif		
		simulation_thread_routine(sim_dt);


		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		simulation_thread_dt = elapsed_seconds.count();
		//console_log("elapsed %f", simulation_thread_dt);

		double sleep_ms = glm::max(1., 1e3 * (sim_dt - simulation_thread_dt));
		Sleep(sleep_ms);
	}
	return 0;
}
#endif

void glfw_error_callback(int error, const char* description) {
	console_error("GL Error: %s", description);
}

void glfw_framebuffer_size_callback(GLFWwindow *, int w, int h) {
	//console_log("framebuffer size %d %d", w, h); 
	width = w;
	height = h;
}

void glfw_char_callback(GLFWwindow* window,unsigned int c) {
	console_log("char %d", c);
}

void glfw_key_callback(GLFWwindow* window,int keycode, int scancode, int downup, int mods) {
	switch (keycode) {
	case GLFW_KEY_SPACE: {
		if (downup) shared.updating = !shared.updating;
	} break;
	case GLFW_KEY_ESCAPE: {
		threads_done = true;
	} break;
	case 82: { // R
		if (downup) shared.recording = !shared.recording;
		cameradata_frame = 0;
		console_log("RECORDING: %d", shared.recording);
	} break;
	case 80: { // P
		if (downup) shared.playback = !shared.playback;
		cameradata_frame = 0;
		console_log("PLAYBACK: %d", shared.playback);
	} break;
	case GLFW_KEY_LEFT: 
		keyrot.y = downup ?  1.f : 0.f;
		break;
	case GLFW_KEY_RIGHT: 
		keyrot.y = downup ? -1.f : 0.f;
		break;
	case 65: //A
		keynav.x = downup ? -1.f : 0.f;
		break;
	case 68: //D
		keynav.x = downup ?  1.f : 0.f;
		break;
	case 87:
	case GLFW_KEY_UP: 
		keynav.z = downup ? -1.f : 0.f;
		break;
	case 83:
	case GLFW_KEY_DOWN: 
		keynav.z = downup ?  1.f : 0.f;
		break;
	case 90:
	case 47: 
		keynav.y = downup ? -1.f : 0.f;
		break;
	case 81:
	case 39: 
		keynav.y = downup ?  1.f : 0.f;
		break;
	default:
		console_log("key %d %d %d %d", keycode, scancode, downup, mods);
	}
}

int main() {

	console_init();
	console_log("starting");
	sema.flip_xz();
	shared.init();
#ifdef USE_SPOUT
	spout = GetSpout();
	if (!spout) {
		console_error("failed to create Spout library");
	}
#endif

	std::cout << "Starting GLFW context, OpenGL 3.3" << std::endl;
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwSetErrorCallback(glfw_error_callback);

	int monitor_count;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	console_log("found %d monitors", monitor_count);

	const GLFWvidmode * lastMonitorMode = glfwGetVideoMode(monitors[monitor_count -1]);
	
#ifdef AL_WIN
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#endif

#ifdef AL_OSX
	// apple only allows core profile
	// which means lots of gl functions no longer exist 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// Window dimensions
	width = 1200;
	height = 600;
	
	if (show_shadow && monitor_count > 1) {
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_MAXIMIZED, GL_TRUE);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);

		width = lastMonitorMode->width;
		height = lastMonitorMode->height;
		window = glfwCreateWindow(width, height, "Conservation of Shadows", monitors[monitor_count-1], NULL);
	}
	else {
		//glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_MAXIMIZED, GL_TRUE);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);

		window = glfwCreateWindow(width, height, "Conservation of Shadows", NULL, NULL);
	}

	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0); // turn off vsync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize OpenGL context" << std::endl;
		return -1;
	}
	printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
	if (GLVersion.major < 2) {
		printf("Your system doesn't support OpenGL >= 2!\n");
		return -1;
	}
	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));

	glfwSetKeyCallback(window, glfw_key_callback);
	glfwGetFramebufferSize(window, &width, &height);
	glfw_framebuffer_size_callback(window, width, height);
	glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);



	
	if (!vive.connect()) {
		console_error("failed to connect to htcvive");
	} else if (!vive.dest_changed()) {
		console_error("failed to allocate htcvive GL resources");
	}
	console_log("VR initialiezed");


	////////////////////////////////////////////////////////////////////////////////////////////////////

	Shader * dustShader = Shader::createFromFilePaths("dust.vp.glsl", "dust.fp.glsl");
	unsigned int dustVAO;
	unsigned int dustVBO;
	{
		// define the VAO 
		// (a VAO stores attrib & buffer mappings in a re-usable way)
		glGenVertexArrays(1, &dustVAO);
		glBindVertexArray(dustVAO);

		// define the VBO while VAO is bound:
		glGenBuffers(1, &dustVBO);
		glBindBuffer(GL_ARRAY_BUFFER, dustVBO);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, sizeof(shared.dust), shared.dust, GL_STATIC_DRAW);

		// attr location 
		glEnableVertexAttribArray(0);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Dust), (void*)(offsetof(Dust, pos)));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}


	Shader * cloudShader = Shader::createFromFilePaths("cloud.vp.glsl", "cloud.fp.glsl");
	unsigned int cloudVAO;
	unsigned int cloudVBO;
	{
		// define the VAO 
		// (a VAO stores attrib & buffer mappings in a re-usable way)
		glGenVertexArrays(1, &cloudVAO);
		glBindVertexArray(cloudVAO);

		// define the VBO while VAO is bound:
		glGenBuffers(1, &cloudVBO);
		glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, sizeof(XYZRGB::Point) * DUST_MAX, sema_points, GL_STATIC_DRAW);

		// attr location 
		glEnableVertexAttribArray(0);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(XYZRGB::Point), (void*)(offsetof(XYZRGB::Point, pos)));


		glEnableVertexAttribArray(1);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(XYZRGB::Point), (void*)(offsetof(XYZRGB::Point, color)));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}


	//SimpleOBJ snake_obj("sp_long.obj");
	//SimpleOBJ snake_obj("sp_long01.obj");
	//SimpleOBJ snake_obj("snake.obj");
	//SimpleOBJ snake_obj("tex_n.obj");
	SimpleOBJ snake_obj("l_m.obj");
	//SimpleOBJ snake_obj("w_m.obj");
	
	Shader * agentShader;
	Shader * agentShadowShader;
	unsigned int agentVAO;
	unsigned int agentVBO;
	unsigned int agentEBO;
	unsigned int agentInstanceVBO;

	unsigned int stalkVAO;
	unsigned int stalkInstanceVBO;
	{
		agentShader = Shader::createFromFilePaths("agent.vp.glsl", "agent.fp.glsl");
		agentShadowShader = Shader::createFromFilePaths("agent.shadow.vp.glsl", "agent.shadow.fp.glsl");
		
		// define the VAO 
		// (a VAO stores attrib & buffer mappings in a re-usable way)
		glGenVertexArrays(1, &agentVAO);
		glBindVertexArray(agentVAO);
		// define the VBO while VAO is bound:
		glGenBuffers(1, &agentVBO);
		glBindBuffer(GL_ARRAY_BUFFER, agentVBO);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * snake_obj.vertices.size(), &snake_obj.vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &agentEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, agentEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(SimpleOBJ::Element) * snake_obj.indices.size(), &snake_obj.indices[0], GL_STATIC_DRAW);

		// attr location 
		glEnableVertexAttribArray(0);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

		// attr location 
		glEnableVertexAttribArray(1);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	
		// attr location 
		glEnableVertexAttribArray(2); 
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
		
		// instance VBO:
		glGenBuffers(1, &agentInstanceVBO);
		glBindBuffer(GL_ARRAY_BUFFER, agentInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Instance) * AGENTS_MAX, &shared.agent_instance_array[0], GL_STATIC_DRAW);

		// instance attrs:
		glEnableVertexAttribArray(4);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, position)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(4, 1);

		glEnableVertexAttribArray(5);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, quat)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(5, 1);

		glEnableVertexAttribArray(6);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, params)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(6, 1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	{
		// define the VAO 
		// (a VAO stores attrib & buffer mappings in a re-usable way)
		glGenVertexArrays(1, &stalkVAO);
		glBindVertexArray(stalkVAO);

		glBindBuffer(GL_ARRAY_BUFFER, agentVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, agentEBO);

		// attr location 
		glEnableVertexAttribArray(0);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0); 

		// attr location 
		glEnableVertexAttribArray(1);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

		// instance VBO:
		glGenBuffers(1, &stalkInstanceVBO);
		glBindBuffer(GL_ARRAY_BUFFER, stalkInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Instance) * STALKS_MAX, &shared.stalk_instance_array[0], GL_STATIC_DRAW);

		// instance attrs:
		glEnableVertexAttribArray(4);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, position)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(4, 1);

		glEnableVertexAttribArray(5);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, quat)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(5, 1);

		glEnableVertexAttribArray(6);
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(offsetof(Instance, params)));
		// mark this attrib as being per-instance	
		glVertexAttribDivisor(6, 1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	Shader * landShader = Shader::createFromFilePaths("land.vp.glsl", "land.fp.glsl");
	unsigned int landVAO;
	unsigned int landVBO;
	unsigned int landEBO;
	{
		// define the VAO 
		// (a VAO stores attrib & buffer mappings in a re-usable way)
		glGenVertexArrays(1, &landVAO);
		glBindVertexArray(landVAO);

		// define the VBO while VAO is bound:
		glGenBuffers(1, &landVBO);
		glBindBuffer(GL_ARRAY_BUFFER, landVBO);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, shared.land_cells.size(), shared.land_cells.data, GL_STATIC_DRAW);

		glGenBuffers(1, &landEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, landEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(shared.land_indices), shared.land_indices, GL_STATIC_DRAW);

		// attr location 
		glEnableVertexAttribArray(0);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LandCell), (void*)0);
		//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

		// attr location 
		glEnableVertexAttribArray(1);
		// set the data layout
		// attr location, element size & type, normalize?, source stride & offset
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LandCell), (void*)(offsetof(LandCell, normal)));
		//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0); 

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	Shader * bgShadowShader = Shader::createFromFilePaths("bg.shadow.vp.glsl", "bg.shadow.fp.glsl");
	

	SimpleFBO fbo;
	fbo.dim = glm::ivec2(4096);
	if (!fbo.dest_changed()) {
		console_error("failed to allocate fbo");
	}
	
	SimpleFBO shadowfbo;
	if (!shadowfbo.dest_changed()) {
		console_error("failed to allocate fbo");
	}
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_SPOUT
	if (spout) {
		spout->CreateSender("COS", fbo.dim.x, fbo.dim.y);
	}
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////////////



#ifdef AL_WIN
	kinects[0].usecolor = 0;
	kinects[1].usecolor = 0;
#ifdef USE_KINECTS
	// 0: "USB\VID_045E&PID_02C2\6&213947De&2&4"
	// 1: "USB\VID_045E&PID_02C2\6&E7F9739&0&4"
	kinects[1].open("USB\\VID_045E&PID_02C2\\6&213947DE&2&4", 0);
	kinects[0].open("USB\\VID_045E&PID_02C2\\6&E7F9739&0&4", 1);
	//getchar();
#endif
	if (!singlethread) {
		simulation_thread_handle = CreateThread(0, 0, simulation_thread, 0, 0, &simulation_thread_id);
	}

#else

#endif


	//////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// Game loop
	double t0 = glfwGetTime(); // frame start time
	double t = t0; // current time
	double fps_avg = 90.;
	double debug_timer = t0;
	double dt = 1. / 90.;
	while (!glfwWindowShouldClose(window) && !threads_done)
	{

		glm::mat4 ModelMatrix = glm::mat4(1.); 
		if (!shared.playback) {

			viewposition += quat_rotate(viewquat, keynav) * float(dt);
			viewquat = glm::quat(keyrot * float(dt)) * viewquat;
		}

		if (1) {
			shared.main_update(dt);
			
			if (singlethread) simulation_thread_routine(dt);
		}
		
		const double t_post_simulate = glfwGetTime();
		const double dur_simulate = t_post_simulate - t;
		t = t_post_simulate;

		////////////////////////////////////////////////////////////////////////////////////////////////////

		glfwMakeContextCurrent(window);

		glBindBuffer(GL_ARRAY_BUFFER, dustVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(shared.dust), shared.dust, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(XYZRGB::Point) * DUST_MAX, sema_points, GL_STATIC_DRAW);


		glBindBuffer(GL_ARRAY_BUFFER, landVBO);
		glBufferData(GL_ARRAY_BUFFER, shared.land_cells.size(), shared.land_cells.data, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, agentInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Instance) * AGENTS_MAX, &shared.agent_instance_array[0], GL_STATIC_DRAW);
		
   		const double t_post_uploadgpu = glfwGetTime();
		const double dur_uploadgpu = t_post_uploadgpu - t;
		t = t_post_uploadgpu;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		
		// Check if any events have been activated (key pressed, mouse moved etc.) and call corresponding response functions
		glfwPollEvents();

		vive.bang();

		shared.attraction_point = vive.mTrackedPosition;

		////////////////////////////////////////////////////////////////////////////////////////////////////

		if (shared.updating) {
			shadowfbo.begin();
			{
				glViewport(0, 0, shadowfbo.dim.x, shadowfbo.dim.y);
				glEnable(GL_DEPTH_TEST);
				glClearColor(1.f, 1.f, 1.f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glEnable(GL_BLEND);
				glBlendFunc(GL_DST_COLOR, GL_ZERO);
				glBlendEquation(GL_MIN);
				//glBlendFunc(GL_DST_COLOR, GL_DST_COLOR);
				//glBlendFunc(GL_DST_ALPHA, GL_DST_ALPHA);
				glDepthMask(GL_FALSE);

				glm::mat4 ProjectionMatrix = glm::perspective(float(M_PI * 0.5), width / (float)height, 0.1f, 100.f);
				glm::mat4 ViewMatrix = glm::lookAt(glm::vec3(0., 4., 0.), glm::vec3(0., 0., 0.), glm::vec3(0., 0., -1.));
				glm::mat4 ModelViewMatrix = ViewMatrix * ModelMatrix;
				glm::mat4 ModelViewMatrixInverse = glm::inverse(ModelViewMatrix);
				glm::mat4 ProjectionMatrixInverse = glm::inverse(ProjectionMatrix);

				if (1) {
					agentShadowShader->use();
					agentShadowShader->uniform("time", t0);
					agentShadowShader->uniform("ModelViewMatrix", ModelViewMatrix);
					agentShadowShader->uniform("ProjectionMatrix", ProjectionMatrix);
					if (1) {
						glBindVertexArray(agentVAO);
						glDrawElementsInstanced(GL_TRIANGLES, snake_obj.indices.size(), GL_UNSIGNED_INT, 0, AGENTS_MAX);
						glBindVertexArray(0);
					}
					agentShadowShader->unuse();
				}

				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);
			}
			shadowfbo.end();

			fbo.begin();
			glEnable(GL_SCISSOR_TEST);
			for (int eye = 0; eye < 2; eye++) {
				{
					glScissor(eye * fbo.dim.x / 2, 0, fbo.dim.x / 2, fbo.dim.y);
					glViewport(eye * fbo.dim.x / 2, 0, fbo.dim.x / 2, fbo.dim.y);

					glEnable(GL_DEPTH_TEST);
					glClearColor(0.f, 0.f, 0.f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

					glm::mat4 ViewMatrix = glm::mat4_cast(glm::inverse(viewquat)) * glm::translate(glm::mat4(1.0f), -viewposition);
					glm::mat4 ProjectionMatrix = glm::perspective(float(M_PI * 0.5), 0.5f*width / (float)height, 0.1f, 100.f);
					if (vive.connected) {
						// override these
						//ViewMatrix = vive.mTrackedPosition;//vive.mHMDPose * vive.m_mat4viewEye[eye];
						//ViewMatrix = glm::inverse(vive.mHMDPose); 
						ViewMatrix = glm::inverse(vive.m_mat4viewEye[eye]) * glm::mat4_cast(glm::inverse(vive.mTrackedQuat)) * glm::translate(glm::mat4(1.f), -vive.mTrackedPosition);
						ProjectionMatrix = glm::frustum(vive.frustum[eye].l, vive.frustum[eye].r, vive.frustum[eye].b, vive.frustum[eye].t, vive.frustum[eye].n, vive.frustum[eye].f);
					}
					glm::mat4 ModelViewMatrix = ViewMatrix;// *ModelMatrix;
					glm::mat4 ModelViewMatrixInverse = glm::inverse(ModelViewMatrix);
					glm::mat4 ProjectionMatrixInverse = glm::inverse(ProjectionMatrix);

					if (1) {
						//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						glDisable(GL_CULL_FACE);

						// use the VAO & shader:
						glBindVertexArray(landVAO);
						landShader->use();
						landShader->uniform("ModelViewMatrix", ModelViewMatrix);
						landShader->uniform("ProjectionMatrix", ProjectionMatrix);

						// offset, vertex count, instance count
						glDrawElements(GL_TRIANGLES, LAND_NUM_INDICES, GL_UNSIGNED_INT, 0);

						landShader->unuse();
						glBindVertexArray(0);

						glEnable(GL_CULL_FACE);
						//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					}

					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					glBlendEquation(GL_FUNC_ADD);
					glDepthMask(GL_FALSE);

					if (1) {
						glDisable(GL_CULL_FACE);

						// use the VAO & shader:
						agentShader->use();
						agentShader->uniform("time", t0);
						agentShader->uniform("ModelViewMatrix", ModelViewMatrix);
						agentShader->uniform("ProjectionMatrix", ProjectionMatrix);

						if (1) {
							glBindVertexArray(agentVAO);

							// offset, vertex count, instance count
							glDrawElementsInstanced(GL_TRIANGLES, snake_obj.indices.size(), GL_UNSIGNED_INT, 0, AGENTS_MAX);

							glBindVertexArray(0);
						}
						if (0) {
							glBindVertexArray(stalkVAO);
							glDrawElementsInstanced(GL_TRIANGLES, snake_obj.indices.size(), GL_UNSIGNED_INT, 0, STALKS_MAX);
							glBindVertexArray(0);
						}

						agentShader->unuse();

						glEnable(GL_CULL_FACE);
					}


#ifdef AL_WIN
					glEnable(GL_POINT_SPRITE);
#endif
					glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
					/*
					if (0) {
						glEnable(GL_CULL_FACE);
						glCullFace(GL_FRONT);

						volumeShader->use();
						volumeShader->uniform("ModelViewMatrix", ModelViewMatrix);
						volumeShader->uniform("ProjectionMatrix", ProjectionMatrix);
						volumeShader->uniform("WorldMatrix", WorldMatrix);

						volumeShader->uniform("ModelViewMatrixInverse", ModelViewMatrixInverse);
						volumeShader->uniform("ProjectionMatrixInverse", ProjectionMatrixInverse);
						volumeShader->uniform("WorldMatrixInverse", WorldMatrixInverse);
						//volumeShader->uniform("fieldtex", 0);
						//glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_3D, densityTex.tex);
						cube_mesh.draw();
						glBindTexture(GL_TEXTURE_3D, 0);

						volumeShader->unuse();
											//
						glCullFace(GL_BACK);
						glDisable(GL_CULL_FACE);
					}
					*/

					if (1) {
						// use the VAO & shader:
						dustShader->use();
						dustShader->uniform("time", t0);
						dustShader->uniform("ModelViewMatrix", ModelViewMatrix);
						dustShader->uniform("ProjectionMatrix", ProjectionMatrix);
						dustShader->uniform("PointSize", fbo.dim.y * 0.002f);

						glBindVertexArray(dustVAO);
						glDrawArrays(GL_POINTS, 0, DUST_MAX);
						glBindVertexArray(0);

						dustShader->unuse();
					}

					if (1) {
						// use the VAO & shader:
						cloudShader->use();
						cloudShader->uniform("time", t0);
						cloudShader->uniform("ModelViewMatrix", ModelViewMatrix);
						cloudShader->uniform("ProjectionMatrix", ProjectionMatrix);
						cloudShader->uniform("PointSize", fbo.dim.y * 0.003f);


						glBindVertexArray(cloudVAO);

						glDrawArrays(GL_POINTS, 0, DUST_MAX);
						glBindVertexArray(0);

						cloudShader->unuse();
					}
					glDepthMask(GL_TRUE);
					glDisable(GL_BLEND);
				}
			}
			glDisable(GL_SCISSOR_TEST);
			fbo.end();

			if (vive.connected) {
				vive.submit(fbo);
			}

			glViewport(0, 0, width, height);
			glEnable(GL_DEPTH_TEST);
			glClearColor(0.f, 0.f, 0.f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			if (show_shadow) {
				bgShadowShader->use();
				shadowfbo.draw_no_shader();
				bgShadowShader->unuse();
			}
			else {
				fbo.draw();
			}
		}

#ifdef USE_SPOUT
		if (spout) {
			spout->SendTexture(fbo.tex, GL_TEXTURE_2D, fbo.dim.x, fbo.dim.y);
		}
#endif
		const double t_post_render = glfwGetTime();
		const double dur_render = t_post_render - t;
		t = t_post_render;

		// Swap the screen buffers
		glfwSwapBuffers(window);

		const double t_post_swapbuffer = glfwGetTime();
		const double dur_swap = t_post_swapbuffer - t;
		t = t_post_swapbuffer;

		// measure:
		const double dur_frame = t - t0;
		dt += 0.1 * (dur_frame - dt);
		fps_avg = 1. / dt;
		t0 = t;
		debug_timer += dur_frame;
		if (debug_timer > 1.) {
			debug_timer -= 1.;
			console_log("fps %03d  t %04ds", int(fps_avg), int(t));
		}
	}

	threads_done = true;
#ifdef AL_WIN
	if (!singlethread) CloseHandle(simulation_thread_handle);

#ifdef USE_KINECTS
	kinects[0].close();
	kinects[1].close();
#endif
#endif

#ifdef USE_SPOUT
	if (spout) {
		spout->ReleaseSender();
		spout->Release();
	}
#endif
	glfwDestroyWindow(window);

	// Terminates GLFW, clearing any resources allocated by GLFW.
	glfwTerminate();
	
	const std::size_t kB = 1024;
    const std::size_t MB = 1024 * kB;
    const std::size_t GB = 1024 * MB;
	console_log("time %f MB", sizeof(CameraData) / float(MB));

	return 0;
}