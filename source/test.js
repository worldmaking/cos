// hello.js
const an = require('./build/Release/an');

an.init();

function step() {
    let buf = an.update(1/60, new Float32Array(10));
    console.log(buf, buf.byteLength/1024/1024);
}

//setInterval(step, 1000/60);
step();

// Prints: 'world'
