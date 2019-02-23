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

	Divergence as the change in density per frame. 

	Note: incompressible fluids have div = 0 everywhere. This is not what we have here!

	Curl is rotation around point



	(beware that voxels below surface may be 'shadowed' and thus unkown)

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
const { vec2, vec3 } = require("gl-matrix");

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
function send_all_clients(msg) {
	wss.clients.forEach(function each(client) {
		try {
			client.send(msg);
		} catch (e) {
			console.error(e);
		};
	});
}

let sessionId = 0;
let sessions = [];

// whenever a client connects to this websocket:
wss.on('connection', function(ws, req) {

    //console.log("ws", ws)
	//console.log("req", req)

	let t0=performance.now()*0.001;
	let fpsAvg = 60;
	let arrays = [];
	for (let i=0; i<3; i++) {
		let array = new Uint8Array(1024*1024); // 1mb
		array[0] = 255;
		array[array.length-1] = i;
		arrays.push(array);
	}
	console.log(arrays[0].length);
		
	let stream = setInterval(function() {
		if(!ws) return;
		for (let a of arrays) {
			ws.send(a);
		}

		let t = performance.now()*0.001;
		let dt = t-t0;
		t0 = t;
		let fps = 1/dt;
		fpsAvg += 0.1*(fps-fpsAvg);
		if (Math.random() < 0.01) console.log("fps ", fpsAvg, " at ", t);

	}, 1000/60);

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
			clearInterval(stream);
		}
	});

	// what to do if client disconnects?
	ws.on('close', function(connection) {
		console.log("connection closed");
        console.log("server has "+wss.clients.size+" connected clients");
		clearInterval(stream);
	});
	
	// respond to any messages from the client:
	ws.on('message', function(e) {
		if (e instanceof Buffer) {
			// get an arraybuffer from the message:
			const ab = e.buffer.slice(e.byteOffset,e.byteOffset+e.byteLength);
			console.log("received arraybuffer", ab);
			// as float32s:
			//console.log(new Float32Array(ab));
		} else {
			try {
				handlemessage(JSON.parse(e), ws);
			} catch (e) {
				console.log('bad JSON: ', e);
			}
		}
    });
    
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
	console.log(`main view on http://localhost:${server.address().port}/index.html`);
});