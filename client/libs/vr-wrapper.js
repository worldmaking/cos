

let vr = {
    canvas: null,
    Display: null,
    frameData: null,
    
    PresentButton: null,
    stats: null,
    cubeIsland: null,
    
    projectionMat: mat4.create(),
    viewMat: mat4.create(),
    PLAYER_HEIGHT: 1.65,
    depthNear: 0.01,
    depthFar: 16,
    
    // Get a matrix for the pose that takes into account the stageParameters
    // if we have them, and otherwise adjusts the position to ensure we're
    // not stuck in the floor.
    getStandingViewMatrix: function (out, view) {
      if (this.Display.stageParameters) {
        // If the headset provides stageParameters use the
        // sittingToStandingTransform to transform the view matrix into a
        // space where the floor in the center of the users play space is the
        // origin.
        mat4.invert(out, this.Display.stageParameters.sittingToStandingTransform);
        mat4.multiply(out, view, out);
      } else {
        // Otherwise you'll want to translate the view to compensate for the
        // scene floor being at Y=0. Ideally this should match the user's
        // height (you may want to make it configurable). For this demo we'll
        // just assume all human beings are 1.65 meters (~5.4ft) tall.
        mat4.identity(out);
        mat4.translate(out, out, [0, this.PLAYER_HEIGHT, 0]);
        mat4.invert(out, out);
        mat4.multiply(out, view, out);
      }
    },
    
    init: function(canvas, gl) {
      let vr = this;
      this.canvas = canvas;
      this.frameData = new VRFrameData();
      
      let initWebGL = (preserveDrawingBuffer) => {
        if (!gl) {
          VRSamplesUtil.addError("Your browser does not support webgl");
          return;
        }
        gl.clearColor(0., 0., 0., 1.0);
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        let texture = createCheckerTexture(gl, 6);
  
        // If the VRDisplay doesn't have stageParameters we won't know
        // how big the users play space. Construct a scene around a
        // default space size like 2 meters by 2 meters as a placeholder.
        vr.cubeIsland = new VRCubeIsland(gl, texture, 6, 6);
        let enablePerformanceMonitoring = false;
        console.log("ok")
        vr.stats = new WGLUStats(gl, enablePerformanceMonitoring);
        // Wait until we have a WebGL context to resize and start rendering.
        window.addEventListener("resize", onResize, false);
        onResize();
        window.requestAnimationFrame(onAnimationFrame);
      }
      
      if (!navigator.getVRDisplays) {
        initWebGL(false);
        VRSamplesUtil.addError("Your browser does not sufficiently support WebVR.");
        return;
      }
      
      navigator.getVRDisplays().then(
        (displays) => {
          if (displays.length < 1) {
            initWebGL(false);
            VRSamplesUtil.addInfo(
              "WebVR supported, but no VRDisplays found.",
              3000
            );
            return;
          }
          
          vr.Display = displays[displays.length - 1];
          vr.Display.depthNear = this.depthNear;
          vr.Display.depthFar = this.depthFar;
          initWebGL(vr.Display.capabilities.hasExternalDisplay);
  
          console.log(vr.Display);
  
          if (
            vr.Display.stageParameters &&
            vr.Display.stageParameters.sizeX > 0 &&
            vr.Display.stageParameters.sizeZ > 0
          ) {
            // If we have stageParameters with a valid size use that to resize
            // our scene to match the users available space more closely. The
            // check for size > 0 is necessary because some devices, like the
            // Oculus Rift, can give you a standing space coordinate but don't
            // have a configured play area. These devices will return a stage
            // size of 0.
            vr.cubeIsland.resize(vr.Display.stageParameters.sizeX, vr.Display.stageParameters.sizeZ);
          }
  
          if (vr.Display.capabilities.canPresent)
            vr.PresentButton = VRSamplesUtil.addButton(
              "Enter VR",
              "E",
              "media/icons/cardboard64.png",
              onVRRequestPresent
            );
  
          window.addEventListener(
            "vrdisplaypresentchange",
            onVRPresentChange,
            false
          );
          window.addEventListener(
            "vrdisplayactivate",
            onVRRequestPresent,
            false
          );
          window.addEventListener(
            "vrdisplaydeactivate",
            onVRExitPresent,
            false
          );
        },
        function() {
          VRSamplesUtil.addError("Your browser does not support WebVR.");
        }
      );
      
      canvas.addEventListener("webglcontextlost", (event) => {
        event.preventDefault();
        console.log("WebGL Context Lost.");
        gl = null;
        vr.cubeIsland = null;
        vr.stats = null;
      }, false);
      canvas.addEventListener(
        "webglcontextrestored",
        (event) => {
          console.log("WebGL Context Restored.");
          initWebGL(vr.Display ? vr.Display.capabilities.hasExternalDisplay : false);
        },
        false
      );
    },
    
    renderSceneView: function() {
      if (draw) draw(this);
    }
  };
    
    function onVRRequestPresent() {
    vr.Display.requestPresent([{ source: canvas }]).then(
      function() {},
      function(err) {
        let errMsg = "requestPresent failed.";
        if (err && err.message) {
          errMsg += "<br/>" + err.message;
        }
        VRSamplesUtil.addError(errMsg, 2000);
      }
    );
  }
  function onVRExitPresent() {
    if (!vr.Display.isPresenting) return;
    vr.Display.exitPresent().then(
      function() {},
      function() {
        VRSamplesUtil.addError("exitPresent failed.", 2000);
      }
    );
  }
  function onVRPresentChange() {
    onResize();
    console.log("onVRPresentChange", vr.Display.isPresenting)
    if (vr.Display.isPresenting) {
      if (vr.Display.capabilities.hasExternalDisplay) {
        VRSamplesUtil.removeButton(vr.PresentButton);
        vr.PresentButton = VRSamplesUtil.addButton(
          "Exit VR",
          "E",
          "media/icons/cardboard64.png",
          onVRExitPresent
        );
      }
    } else {
      if (vr.Display.capabilities.hasExternalDisplay) {
        VRSamplesUtil.removeButton(vr.PresentButton);
        vr.PresentButton = VRSamplesUtil.addButton(
          "Enter VR",
          "E",
          "media/icons/cardboard64.png",
          onVRRequestPresent
        );
      }
    }
  }
    
    function onResize() {
    if (vr.Display && vr.Display.isPresenting) {
      let leftEye = vr.Display.getEyeParameters("left");
      let rightEye = vr.Display.getEyeParameters("right");
      vr.canvas.width =
        Math.max(leftEye.renderWidth, rightEye.renderWidth) * 2;
      vr.canvas.height = Math.max(
        leftEye.renderHeight,
        rightEye.renderHeight
      );
    } else {
      vr.canvas.width = vr.canvas.offsetWidth * window.devicePixelRatio;
      vr.canvas.height = vr.canvas.offsetHeight * window.devicePixelRatio;
    }
  }
  
  
  
  function onAnimationFrame(t) {
    // do not attempt to render if there is no available WebGL context
    if (!gl) return;
    vr.stats.begin();
  
    if (update) update();
  
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    if (vr.Display) {
      vr.Display.requestAnimationFrame(onAnimationFrame);
      vr.Display.getFrameData(vr.frameData);
      if (vr.Display.isPresenting) {
        gl.viewport(0, 0, vr.canvas.width * 0.5, vr.canvas.height);
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        vr.getStandingViewMatrix(vr.viewMat, vr.frameData.leftViewMatrix);
        mat4.copy(vr.projectionMat, vr.frameData.leftProjectionMatrix);
        vr.renderSceneView();
        gl.viewport(vr.canvas.width * 0.5, 0, vr.canvas.width * 0.5, vr.canvas.height);
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        vr.getStandingViewMatrix(vr.viewMat, vr.frameData.rightViewMatrix);
        mat4.copy(vr.projectionMat, vr.frameData.rightProjectionMatrix);
        vr.renderSceneView();
        vr.Display.submitFrame();
      } else {
        gl.viewport(0, 0, vr.canvas.width, vr.canvas.height);
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        mat4.perspective(
          vr.projectionMat,
          Math.PI * 0.4,
          vr.canvas.width / vr.canvas.height,
          vr.depthNear, vr.depthFar
        );
        vr.getStandingViewMatrix(vr.viewMat, vr.frameData.leftViewMatrix);
        vr.renderSceneView();
        vr.stats.renderOrtho();
      }
    } else {
      window.requestAnimationFrame(onAnimationFrame);

      // No VRDisplay found.
      gl.viewport(0, 0, vr.canvas.width, vr.canvas.height);
      gl.enable(gl.DEPTH_TEST);
      gl.enable(gl.CULL_FACE);
      mat4.perspective(
        vr.projectionMat,
        Math.PI * 0.4,
        vr.canvas.width / vr.canvas.height,
        vr.depthNear, vr.depthFar
      );
      mat4.identity(vr.viewMat);
      mat4.translate(vr.viewMat, vr.viewMat, [0, -vr.PLAYER_HEIGHT, 0]);
      //vr.cubeIsland.render(vr.projectionMat, vr.viewMat, vr.stats);
      vr.renderSceneView();
      vr.stats.renderOrtho();
    }
    vr.stats.end();
  }