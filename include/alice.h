#ifndef ALICE_H
#define ALICE_H

#include "al/al_glfw.h"
#include "al/al_signals.h"
#include "al/al_hmd.h"
#include "al/al_leap.h"

struct AL_ALICE_EXPORT Alice {
	
	vdk::signal<void()> onReset;
	vdk::signal<void(uint32_t, uint32_t)> onFrame;
	vdk::signal<void()> onReloadGPU;
	vdk::signal<void(std::string filename)> onFileChange;
	vdk::signal<void(int keycode, int scancode, int downup, bool shift, bool ctrl, bool alt, bool cmd)> onKeyEvent;
	
	double t = 0.;	  		// clock time
	double simTime = 0.; 	// simulation time
	double dt = 1/60.; 		// updated continually while running
	double desiredFrameRate = 60.;
	double fpsAvg = 30;	
	int framecount = 0;

	int isSimulating = true;
	int isRendering = true;

	// services:
	Window::Manager windowManager;
	CloudDeviceManager cloudDeviceManager;

	Window window;
	Hmd * hmd;
	LeapMotion * leap;


	
	static Alice& Instance();	
	
};

#endif //ALICE_H 