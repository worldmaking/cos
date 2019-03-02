#!/usr/bin/env node

/*
	# Streaming data node -> browser

	Sending as arraybuffer over websocket seems to be the recommended way at the moment

	## Bandwidth limits

	firefox was accepting around 100mbps over localhost websocket
	so long as it was broken into blobs of 1mb 
	at 90fps, it's ~10x 1mb blobs per frame
	at 30fps, it's ~30x 1mb blobs per frame
	(chrome was accepting about 1/10th that)

	Blob 1: Agents:
	an agent is maybe about 64 bytes (4x vec4); 16k of them in 1mb
	- more than enough

	Blob 2: Kinect height map:
	height only map: 4 bytes per point. 512x512 fits into 1mb. 
	- 1cm in a 5m space.
	height plus normal: 16 bytes per point. 256x256 fits into 1mb. 
	- 2cm in a 5m space. 
	full xyz: 12 bytes per point, too much data

	Blob 3: Voxel field:
	a 32x32x32 voxel field in 1mb can fit 32 bytes per voxel, enough for two vec4's
	- each voxel is about 15cm in a 5m space.
	- (1/4x height+normal or 1/8x height-only resolution) 

	Seems like we should have enough bandwidth.

	# Kinect handling

	We have a calibration routine that can help us to 
	a) derive exact camera properties for the projector
	b) derive relation to floor

	(b) means we can re-orient the cloud points with respect to an origin in the centre of the floor space

	## Conversion to height map

	Assume a 256x256 grid. Not all K points will land in this grid. Some K points are discarded as out of range/bad. This means some cells will have no K points, and some cells will have multiple K points. Also, values can be a bit noisy.
	Missing points: a space average needed. Either, always apply it, or only apply for cells with no raw input. 
	Multiple points: an average of results in a frame. 
	Noise: a time average needed. (Could apply on depth image directly, could apply to final height map, or anywhere in between..)
	Empty points might also need to decay, to avoid leaving things dangling.

	Simple idea:
	- first, decay field
	- second, spatial average field
	- third, blend in new data via a running average routine

	Normal map can easily be derived from height map.

	Q: do we want to distinguish slow-moving and fast-moving areas? To distinguish filler material from humans?

	## Conversion to flow field

	If a point cloud surface intersects a voxel, there could be over 200 points in it. However most voxels will be empty. 
	This can also be quite noisy, and may need some temporal smoothing
	Can use this to construct a density field. 

	Get gradient of density field.
	- just the 
	- direction gives the fluid direction at this point
	- length gives the ?

	Divergence as the change in v per frame. 
	= sum of density derivatives in each axis (dot product)

	Note: incompressible fluids have div = 0 everywhere. This is not what we have here!

	Curl is rotation around point
	= cross product of axis derivatives with field 



	(beware that voxels below surface may be 'shadowed' and thus unkown)


	# Getting Kinect data to client

	Right now we have Kinect data in Max, as jitter matrices.
	To get this to server, either:
	- write jit matrix handlers for [ws] or [ws.client] to send as arraybuffers
		- https://github.com/zaphoyd/websocketpp/issues/572
		- https://github.com/zaphoyd/websocketpp/issues/575
		- it doesn't include any type info, just a raw binary
		- also may need to deal with network byte order etc., packing, etc.
	- Don't use Max: port the Kinect handling code over to a node binary module
		- lose the ability to live patch the configuration
	- Use a mmapfile 



*/

const http = require('http');
const url = require('url');
const fs = require("fs");
const path = require("path");
const os = require("os");
const assert = require("assert");
const performance = require('perf_hooks').performance;
const { exec, execSync, spawn, spawnSync, fork } = require('child_process');


const express = require('express');
const WebSocket = require('ws');
const PNG = require("pngjs").PNG;
const { vec2, vec3, vec4, quat, mat3, mat4 } = require("gl-matrix");

const projector_calibration = JSON.parse(fs.readFileSync("projector_calibration/calib.json", "utf-8"));
{
	/*
		Want to derive proper transformation from this
		- get kinect points relative to groundspace
		- get camera position/orientation relative to groundspace
	*/

	// note: raw kinect data looks into -Z axis
	// the calib data gives the ground and the projector data in this kinect viewspace
	// we'd like to re-position it into the groundspace
	// ground quat/pos gives the pose of our ground plane, the ideal centre of the space.
	// it is originally given in the kinect's viewspace
	// (but groundquat was wrongly rotated by 90 degrees :-()
	let ground_quat = quat.mul([], projector_calibration.ground_quat, quat.fromEuler(quat.create(), 90., 0., 0.));
	projector_calibration.ground_quat_fixed = ground_quat;
	let ground_position = projector_calibration.ground_position;
	let quat_ground = quat.invert([], ground_quat);
	let position_ground = vec3.scale([], ground_position, -1);

	let k2g_rot = mat4.fromQuat([], ground_quat); 
	let k2g_tra = mat4.fromTranslation([], ground_position);
	// k2g_mat will take kinect viewspace and turn it into ground space
	let k2g_mat = mat4.multiply([], k2g_tra, k2g_rot);
	let g2k_mat = mat4.invert([], k2g_mat)
	let g2k_rot = quat.fromMat3([], mat3.fromMat4([], g2k_mat));

	// // let's transform the projector to derive the desired camera:
	projector_calibration.camera_position = vec3.transformMat4([], projector_calibration.position, g2k_mat);
	projector_calibration.camera_quat = quat.mul([], g2k_rot, projector_calibration.quat);
	projector_calibration.position_ground = position_ground;
	projector_calibration.quat_ground = quat_ground;

	if (0) {
		fs.writeFileSync("projector_calibration/calib.json", JSON.stringify(projector_calibration), "utf-8");
	}
}

const HDIM = 256;
const HDIM2 = HDIM*HDIM;
let hmap = {
	heights: new Float32Array(HDIM2),
};

const an = require('./source/build/Release/an');
an.init();

const project_path = process.cwd();
const server_path = __dirname;
const client_path = path.join(server_path, "client");
console.log("project_path", project_path);
console.log("server_path", server_path);
console.log("client_path", client_path);

// let ctrl-c quit:
{
	let stdin = process.stdin;
	// without this, we would only get streams once enter is pressed
	if (process.stdin.setRawMode){
		process.stdin.setRawMode(true)
	}
	// resume stdin in the parent process (node app won't quit all by itself
	// unless an error or process.exit() happens)
	stdin.resume();
	// i don't want binary, do you?
	stdin.setEncoding( 'utf8' );
	// on any data into stdin
	stdin.on( 'data', function( key ){
	// ctrl-c ( end of text )
	if ( key === '\u0003' ) {
		process.exit();
	}
	// write the key to stdout all normal like
	process.stdout.write( key );
	});
}

////////////////////////

// raw data:
const DIM = 32;
const DIM3 = DIM*DIM*DIM;
let field = new Float32Array(DIM3 * 4);

function updateField() {
	for (let z=0, i=0; z<DIM; z++) { 
		for (let y=0; y<DIM; y++) { 
			for (let x=0; x<DIM; x++, i++) {
			let pos = vec3.fromValues(x, y, z);
			
			let p = vec3.random(vec3.create(), 1.);
			let val = vec4.fromValues(p[0], p[1], p[2], Math.random());
			field.set(val, i * 4);
			} 
		} 
	}
}

////////////////////////

const app = express();
app.use(express.static(client_path))
app.get('/', function(req, res) {
	res.sendFile(path.join(client_path, 'index.html'));
});
//app.get('*', function(req, res) { console.log(req); });
const server = http.createServer(app);
// add a websocket service to the http server:
const wss = new WebSocket.Server({ 
	server: server,
	maxPayload: 600 * 1024 * 1024, 
});

// send a (string) message to all connected clients:
function send_all_clients(msg, ignore) {
	wss.clients.forEach(function each(client) {
		if (client == ignore) return;
		try {
			client.send(msg);
		} catch (e) {
			console.error(e);
		};
	});
}

let sessionId = 0;
let sessions = [];
let onetime =1;

// whenever a client connects to this websocket:
wss.on('connection', function(ws, req) {
	console.log("server received a connection");
	console.log("server has "+wss.clients.size+" connected clients");
	
	const location = url.parse(req.url, true);
	// You might use location.query.access_token to authenticate or share sessions
	// or req.headers.cookie (see http://stackoverflow.com/a/16395220/151312)
	
	ws.on('error', function (e) {
		if (e.message === "read ECONNRESET") {
			// ignore this, client will still emit close event
		} else {
			console.error("websocket error: ", e.message);
		}
	});

	// what to do if client disconnects?
	ws.on('close', function(connection) {
		console.log("connection closed");
        console.log("server has "+wss.clients.size+" connected clients");
	});
	
	// respond to any messages from the client:
	ws.on('message', function(e) {
		if (e instanceof Buffer) {
			// get an arraybuffer from the message:
			const ab = e.buffer.slice(e.byteOffset,e.byteOffset+e.byteLength);

			// send to everyone except ourselves
			//send_all_clients(ab, ws)

			//console.log("received arraybuffer", ab);
			// as float32s:
			//console.log(new Float32Array(ab));

			// if this is the heightmap, need to convert it to the world coordinate space?
			hmap.heights.set(new Float32Array(ab));
			// let heights = new Float32Array(ab);
			// for (let i=0; i<heights.length; i++) {
			// 	hmap.locations[i*3+1] = heights[i];
			// }
	  
		} else {
			try {
				handlemessage(JSON.parse(e), ws);
			} catch (e) {
				console.log('bad JSON: ', e);
			}
		}
	});
	
	ws.send(JSON.stringify({
		message: "config",
		projector_calibration: projector_calibration,
	}))
    
	// // Example sending some greetings:
	// ws.send(JSON.stringify({
	// 	type: "greeting",
	// 	value: "hello",
	// 	date: Date.now()
	// }));
	// // Example sending binary:
	// const array = new Float32Array(5);
	// for (var i = 0; i < array.length; ++i) {
	// 	array[i] = i / 2;
	// }
    // ws.send(array);
    
    //send_all_clients("hi")
});

function handlemessage(msg, session) {
	switch (msg.cmd) {
		// case "getagents": {
		// 	try {
		// 		//let data = JSON.stringify(world.agents_meta);
		// 		//console.log(data)
		// 		session.send(JSON.stringify(grid))
		// 	} catch (e) {
		// 		console.error(e);
		// 	}
		// } break;
		// case "reset": {
		// 	process.exit(-1)
		// 	break;
		// }
		default: console.log("received JSON", msg, typeof msg);
	}
}

server.listen(8080, function() {
	console.log(`server listening`);
	console.log(`vr view on http://localhost:${server.address().port}/index.html`);
	console.log(`projector view on http://localhost:${server.address().port}/index.html?projector=1`);
});

let t0=performance.now()*0.001;
let fpsAvg = 60;
console.log(field.byteLength );
let stream = setInterval(function() {
	let t = performance.now()*0.001;
	let dt = t-t0;
	t0 = t;
	let fps = 1/dt;
	fpsAvg += 0.1*(fps-fpsAvg);

	//updateField();
	let buf = an.update(dt, hmap.heights);

	//ws.send(field);
	send_all_clients(buf);

	//if (Math.random() < 0.01) console.log("fps ", fpsAvg, " at ", t);

}, 1000/60);