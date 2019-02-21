#define AL_IMPLEMENTATION 1
#include "al/al_glfw.h"

#include "al_max.h"
#include "al_math.h"

#include "world.h"

struct Instance {
	t_object * host = 0;

	World world;

	int callcount=0;

    Window::Manager windowManager;
    Window window;
	
	Instance(t_object * host) : host(host) {
		object_post(host, "hello from dynamic instance %p", this);
        
        window.open();

		world.reset();
		world.dest_changed();
	}
	
	~Instance() {
		object_post(host, "destroying dynamic instance %p", this);
        window.close();
	}

    void bang() {
        glfwPollEvents();
        glfwMakeContextCurrent(window.pointer); // maybe we want to have a onSim() event before doing this?
        glClearColor(0.f, 0.f, 0.f, 1.0f);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		world.update();

        glfwSwapBuffers(window.pointer);
    }

	void anything(t_symbol * s, long argc, t_atom * argv) {
        if (s == gensym("bang")) return bang();


		callcount++;


        double t = glfwGetTime();

		object_post(host, "%s (%d arguments), calls: %d at %f\n", s->s_name, argc, callcount, t);
	}
};

extern "C" {

	C74_EXPORT void * init(t_object * host) {
		return new Instance(host);
	}
	
	C74_EXPORT void quit(t_object * host, Instance * I) {
		delete I;
	}

	C74_EXPORT t_atom_long test(void * instance, t_atom_long) { return 0; }

	C74_EXPORT void anything(Instance * I, t_symbol * s, long argc, t_atom * argv) {
		I->anything(s, argc, argv);
	}
}

