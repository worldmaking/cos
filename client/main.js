let received = 0;

let t0=performance.now()*0.001;
let fpsAvg = 60;

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

				let rx = new Uint8Array(data);
				//console.log(rx[rx.length-1]);

				let t = performance.now()*0.001;
				let dt = t-t0;
				t0 = t;
				let fps = 1/dt;
				if (dt > 0) fpsAvg += 0.1*(fps-fpsAvg);
				
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

setInterval(function() {
	let t = performance.now() * 0.001;
	webutils.print(`avg ${rxAvg/(1024*1024)} mb at ${(received/t)/(1024*1024)} mbps at ${fpsAvg} fps, @${t}`);
}, 1000)