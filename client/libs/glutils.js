
// utility to help turn shader code into a shader object:
function createShader(gl, type, source) {
    let shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    let success = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
    if (success) {
        return shader;
    }
    console.error("shader compile error", gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
    return undefined;
}
  
// utility to turn shader objects into a GPU-loaded shader program
// uses the most common case a program of 1 vertex and 1 fragment shader:
function createProgram(gl, vertexShader, fragmentShader) {
    let program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    let success = gl.getProgramParameter(program, gl.LINK_STATUS);
    if (success) {
        return program;
    }
    console.error("shader program error", gl.getProgramInfoLog(program));  
    gl.deleteProgram(program);
    return undefined;
}

// combine above functions to create a program from GLSL code:
function makeProgramFromCode(gl, vertexCode, fragmentCode) {
    // create GLSL shaders, upload the GLSL source, compile the shaders
    let vertexShader = createShader(gl, gl.VERTEX_SHADER, vertexCode);
    let fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, fragmentCode);
    // Link the two shaders into a program
    return createProgram(gl, vertexShader, fragmentShader);
}

function uniformsFromCode(gl, program, code) {
    let uniforms = {};
    let matches = code.match(/uniform\s+((\w+)\s+(\w+))/g);
    for (let e of matches) {
        let terms = e.split(/\s+/)
        let type = terms[1];
        let name = terms[2];
        let location = gl.getUniformLocation(program, name);
        let setter;
        switch (type) {
            case "float": setter = (f) => gl.uniform1f(location, f); break;
            case "vec2": setter = (x, y, z, w) => gl.uniform2f(location, x, y); break;
            case "vec3": setter = (x, y, z, w) => gl.uniform3f(location, x, y, z); break;
            case "vec4": setter = (x, y, z, w) => gl.uniform4f(location, x, y, z, w); break;
            case "ivec2": setter = (x, y, z, w) => gl.uniform2i(location, x, y); break;
            case "ivec3": setter = (x, y, z, w) => gl.uniform3i(location, x, y, z); break;
            case "ivec4": setter = (x, y, z, w) => gl.uniform4i(location, x, y, z, w); break;
            case "mat2": setter = (m, transpose=false) => gl.uniformMatrix2fv(location, transpose, m); break;
            case "mat3": setter = (m, transpose=false) => gl.uniformMatrix3fv(location, transpose, m); break;
            case "mat4": setter = (m, transpose=false) => gl.uniformMatrix4fv(location, transpose, m); break;
            default: setter = (i) => gl.uniform1i(location, i);
        }
        uniforms[name] = { 
            set: setter,
            name: name,
            type: type,
            location: location,
        };
    };
    return uniforms;
}


// create a GPU buffer to hold some vertex data:
function makeBuffer(gl, vertices) {
    let positionBuffer = gl.createBuffer();
    // Bind it to ARRAY_BUFFER (think of it as ARRAY_BUFFER = positionBuffer)
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, null); // done.
    return positionBuffer;
}

function loadTexture(gl, url, flipY=false, premultiply=false) {

    let tex = {
        id: gl.createTexture(),
        data: null,
        width: 1,
        height: 1,
        channels: 4,
        format: gl.RGBA,
        dataType: gl.UNSIGNED_BYTE,

        bind(unit = 0) {
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, this.id);
            return this;
        },
        unbind(unit = 0) {
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, null);
            return this;
        },
    };
    
    gl.bindTexture(gl.TEXTURE_2D, tex.id);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  
    // Because images have to be download over the internet
    // they might take a moment until they are ready.
    // Until then put a single pixel in the texture so we can
    // use it immediately. When the image has finished downloading
    // we'll update the texture with the contents of the image.
    gl.texImage2D(gl.TEXTURE_2D, 0, tex.format, tex.width, tex.height, 0, tex.format, tex.dataType, new Uint8Array([0, 0, 0, 255]));
  
    const image = new Image();
    image.onload = function() {
        gl.bindTexture(gl.TEXTURE_2D, tex.id);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flipY);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, premultiply);
        gl.texImage2D(gl.TEXTURE_2D, 0, tex.format, tex.format, tex.dataType, image);

        // WebGL1 has different requirements for power of 2 images
        // vs non power of 2 images so check if the image is a
        // power of 2 in both dimensions.
        if (utils.isPowerOf2(tex.width) && utils.isPowerOf2(tex.height)) {
            // Yes, it's a power of 2. Generate mips.
            gl.generateMipmap(gl.TEXTURE_2D);
        } else {
            // No, it's not a power of 2. Turn of mips and set
            // wrapping to clamp to edge
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        }
    };
    image.src = url;
  
    return tex;
}

function createPixelTexture(gl, width, height, floatingpoint=false) {

    floatingpoint =  floatingpoint && (!!gl.getExtension("EXT_color_buffer_float"));
    const channels = 4; // RGBA

    let tex = {
        id: gl.createTexture(),
        data: null,
        width: width,
        height: height,
        channels: channels,
        format: gl.RGBA,
        dataType: floatingpoint ? gl.FLOAT : gl.UNSIGNED_BYTE,  // type of data we are supplying,
        
        load(url) {
            if (!this.data) this.allocate();

            let self = this;
            const img = new Image();   // Create new img element
            const canvas = new OffscreenCanvas(this.width, this.height);
            img.onload = function() {

                // TODO: assert width/height match?
                let ctx = canvas.getContext("2d");
                ctx.drawImage(img, 0, 0);
                let imgdata = ctx.getImageData(0, 0, self.width, self.height);
                let binary = new Uint8ClampedArray(imgdata.data.buffer);
                let length = imgdata.data.length;
                for (let i=0; i<length; i++) {
                    self.data[i*4+0] = (binary[i*4+0] / 255);
                    self.data[i*4+1] = (binary[i*4+1] / 255);
                    self.data[i*4+2] = (binary[i*4+2] / 255);
                    self.data[i*4+3] = (binary[i*4+3] / 255);
                }
                self.bind().submit();
                // self.width = this.width;
                // self.height = this.height;
                // self.canvas.width = self.width;
                // self.canvas.height = self.height;
                // let length = self.width * self.height;
                // let ctx = self.canvas.getContext("2d");
                // ctx.drawImage(img, 0, 0);
                // self.imgdata = ctx.getImageData(0, 0, self.width, self.height);
                // let binary = new Uint8ClampedArray(self.imgdata.data.buffer);
                // let data = new Float32Array(length*4);
                // for (let i=0; i<length; i++) {
                //     data[i*4+0] = (binary[i*4+0] / 255);
                //     data[i*4+1] = (binary[i*4+1] / 255);
                //     data[i*4+2] = (binary[i*4+2] / 255);
                //     data[i*4+3] = (binary[i*4+3] / 255);
                // }
                // self.data = data;

                // if (callback) callback.apply(self);
            }
            img.src = url; // Set source path
            return this;
        },

        // allocate local data
        allocate() {
            if (!this.data) {
                let elements = width * height * channels;
                if (floatingpoint) {
                    this.data = new Float32Array(elements);
                } else {
                    this.data = new Uint8Array(elements);
                }
            }
            return this;
        },
        
        // bind() first
        submit() {
            let mipLevel = 0;
            let internalFormat = floatingpoint ? gl.RGBA32F : gl.RGBA;   // format we want in the texture
            let border = 0;                 // must be 0
            gl.texImage2D(gl.TEXTURE_2D, mipLevel, internalFormat, this.width, this.height, border, this.format, this.dataType, this.data);
            //gl.generateMipmap(gl.TEXTURE_2D);
            return this;
        },
        
        bind(unit = 0) {
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, this.id);
            return this;
        },
        unbind(unit = 0) {
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, null);
            return this;
        },

        read(x, y) {
            if (!this.data) return 0;
    
            let idx = 4*(Math.floor(x) + Math.floor(y) * this.width);
            return this.data[idx+1];
        },
    
        readInto(x, y, v) {
            if (this.data) {
                let idx = 4*(Math.floor(x) + Math.floor(y) * this.width);
                v[0] = this.data[idx];
                v[1] = this.data[idx+1];
                v[2] = this.data[idx+2];
                v[3] = this.data[idx+3];
            }
            return v;
        },
    
        readDot(x, y, xyz) {
            if (!this.data) return 0;
            let idx = 4*(Math.floor(x) + Math.floor(y) * this.width);
            return this.data[idx] * xyz[0]
                 + this.data[idx+1] * xyz[1]
                 + this.data[idx+2] * xyz[2];
        },
    };

    tex.allocate().bind().submit();

    // unless we get `OES_texture_float_linear` we can not filter floating point
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    

    return tex.unbind();
}

function createFBO(gl, width, height, floatingpoint=false) {
    let id = gl.createFramebuffer();

    let colorRenderbuffer = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, colorRenderbuffer);
    gl.renderbufferStorageMultisample(gl.RENDERBUFFER, 4, gl.RGBA8, width, height);

    gl.bindFramebuffer(gl.FRAMEBUFFER, id);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.RENDERBUFFER, colorRenderbuffer);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);

    let fbo = {
        id: id,
        // what we currently read from:
        front: createPixelTexture(gl, width, height, floatingpoint),
        // what we currently draw to:
        back: createPixelTexture(gl, width, height, floatingpoint),
        
        bind() { 
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.id); 

            //gl.renderbufferStorageMultisample(gl.RENDERBUFFER, 4, gl.RBGA4, 256, 256);
            return this; 
        },
        clear(r=0, g=0, b=0, a=1) {
            gl.clearBufferfv(gl.COLOR, 0, [r, g, b, a]);
            return this; 
        },
        unbind() { 
            gl.bindFramebuffer(gl.FRAMEBUFFER, null); 
            return this; 
        },
        swap() {
            [this.back, this.front] = [this.front, this.back];
            return this;
        },
        begin() {
            // make this the framebuffer we are rendering to.
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.id);
            let attachmentPoint = gl.COLOR_ATTACHMENT0;
            let mipLevel = 0;               // the largest mip
            gl.framebufferTexture2D(gl.FRAMEBUFFER, attachmentPoint, gl.TEXTURE_2D, this.back.id, mipLevel);
            gl.viewport(0, 0, width, height);
            return this; 
        },
        
        end() {
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            this.swap();
            return this; 
        },

        blit(dstid) {
            // Blit framebuffers, no Multisample texture 2d in WebGL 2
            gl.bindFramebuffer(gl.READ_FRAMEBUFFER, this.id);
            gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, dstid);
            //gl.clearBufferfv(gl.COLOR, 0, [0.0, 0.0, 0.0, 1.0]);
            gl.blitFramebuffer(
                0, 0, this.front.width, this.front.height,
                0, 0, this.front.width, this.front.height,
                gl.COLOR_BUFFER_BIT, gl.NEAREST // NEAREST is the only valid option at the moment
            );
            return this;
        },

        // reads the GPU memory back into this.data
        // must bind() first!
        // warning: can be slow
        readPixels(attachment = gl.COLOR_ATTACHMENT0) {
            if (!this.front.data) this.front.allocate();
            gl.readBuffer(attachment);
            gl.readPixels(0, 0, this.front.width, this.front.height, this.front.format, this.front.dataType, this.front.data);
            return this;
        },
    };

    fbo.bind().swap().unbind();
    return fbo;
}

function createQuadVao(gl, program) {
    let self = {
        id: gl.createVertexArray(),
        init(program) {
            this.bind();
            {
                let positionBuffer = makeBuffer(gl, [
                    -1,  1,  -1, -1,   1, -1,
                    -1,  1,   1, -1,   1,  1
                ]);
                // look up in the shader program where the vertex attributes need to go.
                let positionAttributeLocation = gl.getAttribLocation(program, "a_position");
                // Turn on the attribute
                gl.enableVertexAttribArray(positionAttributeLocation);
                // Tell the attribute which buffer to use
                gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
                // Tell the attribute how to get data out of positionBuffer (ARRAY_BUFFER)
                let size = 2;          // 2 components per iteration
                let type = gl.FLOAT;   // the data is 32bit floats
                let normalize = false; // don't normalize the data
                let stride = 0;        // 0 = move forward size * sizeof(type) each iteration to get the next position
                let offset = 0;        // start at the beginning of the buffer
                gl.vertexAttribPointer(positionAttributeLocation, size, type, normalize, stride, offset);
                // done with buffer:
                gl.bindBuffer(gl.ARRAY_BUFFER, null);
            }
            {
                let texcoordBuffer = makeBuffer(gl, [
                    0, 1,  0, 0,   1, 0,
                    0, 1,  1, 0,   1, 1
                ]);
                // look up in the shader program where the vertex attributes need to go.
                let positionAttributeLocation = gl.getAttribLocation(program, "a_texCoord");
                // Turn on the attribute
                gl.enableVertexAttribArray(positionAttributeLocation);
                // Tell the attribute which buffer to use
                gl.bindBuffer(gl.ARRAY_BUFFER, texcoordBuffer);
                // Tell the attribute how to get data out of positionBuffer (ARRAY_BUFFER)
                let size = 2;          // 2 components per iteration
                let type = gl.FLOAT;   // the data is 32bit floats
                let normalize = false; // don't normalize the data
                let stride = 0;        // 0 = move forward size * sizeof(type) each iteration to get the next position
                let offset = 0;        // start at the beginning of the buffer
                gl.vertexAttribPointer(positionAttributeLocation, size, type, normalize, stride, offset);
                // done with buffer:
                gl.bindBuffer(gl.ARRAY_BUFFER, null);
            }
            this.unbind();
        },

        bind() {
            gl.bindVertexArray(this.id);
            return this;
        },
        unbind() {
            gl.bindVertexArray(this.id, null);
            return this;
        },
        draw() {
            // draw
            let primitiveType = gl.TRIANGLES;
            let offset = 0;
            let count = 6;
            gl.drawArrays(primitiveType, offset, count);
            return this;
        }
    }
    if (program) self.init(program);

    return self;
}

function createSlab(gl, fragCode, uniforms) {
    let vertCode = `#version 300 es
in vec4 a_position;
in vec2 a_texCoord;
uniform vec2 u_scale;
out vec2 v_texCoord;
void main() {
    gl_Position = a_position;
    vec2 adj = vec2(1, -1);
    gl_Position.xy = (gl_Position.xy + adj)*u_scale.xy - adj;
    v_texCoord = a_texCoord;
}`
    let program = makeProgramFromCode(gl, vertCode, fragCode);
    let self = {
        program: program,
        quad: createQuadVao(gl, program),
        uniforms: uniformsFromCode(gl, program, vertCode + fragCode),

        uniform(name, ...args) {
            this.uniforms[name].set.apply(this, args);
            return this;
        },

        setuniforms(dict) {
            this.use();
            for (let k in dict) {
                this.uniforms[k].set.apply(this, dict[k]);
            }
            return this;
        },

        use() {
            gl.useProgram(this.program);
            return this;
        },

        draw() {
            this.quad.bind().draw();
            return this;
        },
    };
    self.use();
    self.uniform("u_scale", 1, 1);
    if (uniforms) self.setuniforms(uniforms);
    return self;
}