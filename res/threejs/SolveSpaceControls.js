window.devicePixelRatio = window.devicePixelRatio || 1;

SolvespaceCamera = function(renderWidth, renderHeight, scale, up, right, offset) {
    THREE.Camera.call(this);

    this.type = 'SolvespaceCamera';

    this.renderWidth = renderWidth;
    this.renderHeight = renderHeight;
    this.zoomScale = scale; /* Avoid namespace collision w/ THREE.Object.scale */
    this.up = up;
    this.right = right;
    this.offset = offset;
    this.depthBias = 0;

    this.updateProjectionMatrix();
};

SolvespaceCamera.prototype = Object.create(THREE.Camera.prototype);
SolvespaceCamera.prototype.constructor = SolvespaceCamera;
SolvespaceCamera.prototype.updateProjectionMatrix = function() {
    var temp = new THREE.Matrix4();
    var offset = new THREE.Matrix4().makeTranslation(this.offset.x, this.offset.y, this.offset.z);
    // Convert to right handed- do up cross right instead.
    var n = new THREE.Vector3().crossVectors(this.up, this.right);
    var rotate = new THREE.Matrix4().makeBasis(this.right, this.up, n);
    rotate.transpose();
    /* Transpose of rotation matrix == inverse. Rotating the camera by a
       basis is equivalent to rotating an object by the inverse of the
       basis. To mimic Solvespace's behavior, we pan relative to the camera.
       So we need to be rotated to where the camera is pointing before panning.
       See: https://en.wikipedia.org/wiki/Change_of_basis#Two_dimensions */

    /* TODO: If we want perspective, we need an additional matrix
       here which will modify w for perspective divide. */
    var scale = new THREE.Matrix4().makeScale(2 * this.zoomScale / this.renderWidth,
        2 * this.zoomScale / this.renderHeight, this.zoomScale / 30000.0);

    temp.multiply(scale);
    temp.multiply(rotate);
    temp.multiply(offset);

    this.projectionMatrix.copy(temp);
};

SolvespaceCamera.prototype.NormalizeProjectionVectors = function() {
    /* After rotating, up and right may no longer be orthogonal.
    However, their cross product will produce the correct
    rotated plane, and we can recover an orthogonal basis. */
    var n = new THREE.Vector3().crossVectors(this.right, this.up);
    this.up = new THREE.Vector3().crossVectors(n, this.right);
    this.right.normalize();
    this.up.normalize();
};

SolvespaceCamera.prototype.rotate = function(right, up) {
    var oldRight = new THREE.Vector3().copy(this.right).normalize();
    var oldUp = new THREE.Vector3().copy(this.up).normalize();
    this.up.applyAxisAngle(oldRight, up);
    this.right.applyAxisAngle(oldUp, right);
    this.NormalizeProjectionVectors();
};

SolvespaceCamera.prototype.offsetProj = function(right, up) {
    var shift = new THREE.Vector3(right * this.right.x + up * this.up.x,
        right * this.right.y + up * this.up.y,
        right * this.right.z + up * this.up.z);
    this.offset.add(shift);
};

/* Calculate the offset in terms of up and right projection vectors
that will preserve the world coordinates of the current mouse position after
the zoom. */
SolvespaceCamera.prototype.zoomTo = function(x, y, delta) {
    // Get offset components in world coordinates, in terms of up/right.
    var projOffsetX = this.offset.dot(this.right);
    var projOffsetY = this.offset.dot(this.up);

    /* Remove offset before scaling so, that mouse position changes
    proportionally to the model and independent of current offset. */
    var centerRightI = x/this.zoomScale - projOffsetX;
    var centerUpI = y/this.zoomScale - projOffsetY;
    var zoomFactor;

    /* Zoom 20% every 100 delta. */
    if(delta < 0) {
        zoomFactor = (-delta * 0.002 + 1);
    }
    else if(delta > 0) {
        zoomFactor = (delta * (-1.0/600.0) + 1);
    }
    else {
        return;
    }

    this.zoomScale = this.zoomScale * zoomFactor;
    var centerRightF = x/this.zoomScale - projOffsetX;
    var centerUpF = y/this.zoomScale - projOffsetY;

    this.offset.addScaledVector(this.right, centerRightF - centerRightI);
    this.offset.addScaledVector(this.up, centerUpF - centerUpI);
};

SolvespaceControls = function(object, domElement) {
    var _this = this;
    this.object = object;
    this.domElement = ( domElement !== undefined ) ? domElement : document;

    var threePan = new Hammer.Pan({event : 'threepan', pointers : 3, enable : false});
    var panAfterTap = new Hammer.Pan({event : 'panaftertap', enable : false});

    this.touchControls = new Hammer.Manager(domElement, {
        recognizers: [
            [Hammer.Pinch, { enable: true }],
            [Hammer.Pan],
            [Hammer.Tap],
        ]
    });

    this.touchControls.add(threePan);
    this.touchControls.add(panAfterTap);

    var changeEvent = {
        type: 'change'
    };
    var startEvent = {
        type: 'start'
    };
    var endEvent = {
        type: 'end'
    };

    var _changed = false;
    var _offsetPrev = new THREE.Vector2(0, 0);
    var _offsetCur = new THREE.Vector2(0, 0);
    var _rotatePrev = new THREE.Vector2(0, 0);
    var _rotateCur = new THREE.Vector2(0, 0);

    // Used during touch events.
    var _rotateOrig = new THREE.Vector2(0, 0);
    var _offsetOrig = new THREE.Vector2(0, 0);
    var _prevScale = 1.0;

    this.handleEvent = function(event) {
        if (typeof this[event.type] == 'function') {
            this[event.type](event);
        }
    }

    function mousedown(event) {
        event.preventDefault();
        event.stopPropagation();

        switch (event.button) {
            case 0:
                _rotateCur.set(event.screenX,
                               event.screenY);
                _rotatePrev.copy(_rotateCur);
                document.addEventListener('mousemove', mousemove_rotate, false);
                document.addEventListener('mouseup', mouseup, false);
                break;
            case 2:
                _offsetCur.set(event.screenX / window.devicePixelRatio,
                               event.screenY / window.devicePixelRatio);
                _offsetPrev.copy(_offsetCur);
                document.addEventListener('mousemove', mousemove_pan, false);
                document.addEventListener('mouseup', mouseup, false);
                break;
            default:
                break;
        }
    }

    function wheel( event ) {
        event.preventDefault();
        /* FIXME: Width and height might not be supported universally, but
        can be calculated? */
        var box = _this.domElement.getBoundingClientRect();
        object.zoomTo(event.clientX - box.width/2 - box.left,
             -(event.clientY - box.height/2 - box.top), event.deltaY);
        _changed = true;
    }

    function mousemove_rotate(event) {
        _rotateCur.set(event.screenX,
                       event.screenY);
        var diff = new THREE.Vector2().subVectors(_rotateCur, _rotatePrev)
            .multiplyScalar(1 / object.zoomScale);
        object.rotate(-0.3 * Math.PI / 180 * diff.x * object.zoomScale,
             -0.3 * Math.PI / 180 * diff.y * object.zoomScale);
        _changed = true;
        _rotatePrev.copy(_rotateCur);
    }

    function mousemove_pan(event) {
        _offsetCur.set(event.screenX / window.devicePixelRatio,
                       event.screenY / window.devicePixelRatio);
        var diff = new THREE.Vector2().subVectors(_offsetCur, _offsetPrev)
            .multiplyScalar(window.devicePixelRatio / object.zoomScale);
        object.offsetProj(diff.x, -diff.y);
        _changed = true;
        _offsetPrev.copy(_offsetCur);
    }

    function mouseup(event) {
        /* TODO: Opera mouse gestures will intercept this event, making it
        possible to have multiple mousedown events consecutively without
        a corresponding mouseup (so multiple viewports can be rotated/panned
        simultaneously). Disable mouse gestures for now. */
        event.preventDefault();
        event.stopPropagation();

        switch (event.button) {
            case 0:
                document.removeEventListener('mousemove', mousemove_rotate);
                document.removeEventListener('mouseup', mouseup);
                break;
            case 2:
                document.removeEventListener('mousemove', mousemove_pan);
                document.removeEventListener('mouseup', mouseup);
                break;
        }

        _this.dispatchEvent(endEvent);
    }

    function pan(event) {
        /* neWcur - prev does not necessarily equal (cur + diff) - prev.
        Floating point is not associative. */
        var touchDiff = new THREE.Vector2(event.deltaX, event.deltaY);
        _rotateCur.addVectors(_rotateOrig, touchDiff);
        var incDiff = new THREE.Vector2().subVectors(_rotateCur, _rotatePrev)
            .multiplyScalar(1 / object.zoomScale);
        object.rotate(-0.3 * Math.PI / 180 * incDiff.x * object.zoomScale,
             -0.3 * Math.PI / 180 * incDiff.y * object.zoomScale);
        _changed = true;
        _rotatePrev.copy(_rotateCur);
    }

    function panstart(event) {
        /* TODO: Dynamically enable pan function? */
        _rotateOrig.copy(_rotateCur);
    }

    function pinchstart(event) {
        _prevScale = event.scale;
    }

    function pinch(event) {
        /* FIXME: Width and height might not be supported universally, but
        can be calculated? */
        var box = _this.domElement.getBoundingClientRect();

        /* 16.6... pixels chosen heuristically... matches my touchpad. */
        if (event.scale < _prevScale) {
            object.zoomTo(event.center.x - box.width/2 - box.left,
                 -(event.center.y - box.height/2 - box.top), 100/6.0);
            _changed = true;
        } else if (event.scale > _prevScale) {
            object.zoomTo(event.center.x - box.width/2 - box.left,
                 -(event.center.y - box.height/2 - box.top), -100/6.0);
            _changed = true;
        }

        _prevScale = event.scale;
    }

    /* A tap will enable panning/disable rotate. */
    function tap(event) {
        panAfterTap.set({enable : true});
        _this.touchControls.get('pan').set({enable : false});
    }

    function panaftertap(event) {
        var touchDiff = new THREE.Vector2(event.deltaX, event.deltaY);
        _offsetCur.addVectors(_offsetOrig, touchDiff);
        var incDiff = new THREE.Vector2().subVectors(_offsetCur, _offsetPrev)
            .multiplyScalar(1 / object.zoomScale);
        object.offsetProj(incDiff.x, -incDiff.y);
        _changed = true;
        _offsetPrev.copy(_offsetCur);
    }

    function panaftertapstart(event) {
        _offsetOrig.copy(_offsetCur);
    }

    function panaftertapend(event) {
        panAfterTap.set({enable : false});
        _this.touchControls.get('pan').set({enable : true});
    }

    function contextmenu(event) {
        event.preventDefault();
    }

    this.update = function() {
        if (_changed) {
            _this.dispatchEvent(changeEvent);
            _changed = false;
        }
    };

    this.domElement.addEventListener('mousedown', mousedown, false);
    this.domElement.addEventListener('wheel', wheel, false);
    this.domElement.addEventListener('contextmenu', contextmenu, false);

    /* Hammer.on wraps addEventListener */
    // Rotate
    this.touchControls.on('pan', pan);
    this.touchControls.on('panstart', panstart);

    // Zoom
    this.touchControls.on('pinch', pinch);
    this.touchControls.on('pinchstart', pinchstart);

    //Pan
    this.touchControls.on('tap', tap);
    this.touchControls.on('panaftertapstart', panaftertapstart);
    this.touchControls.on('panaftertap', panaftertap);
    this.touchControls.on('panaftertapend', panaftertapend);
};

SolvespaceControls.prototype = Object.create(THREE.EventDispatcher.prototype);
SolvespaceControls.prototype.constructor = SolvespaceControls;


solvespace = function(obj, params) {
    var scene, edgeScene, camera, edgeCamera, renderer;
    var geometry, controls, material, mesh, edges;
    var width, height, scale, offset, projRight, projUp;
    var directionalLightArray = [];
    var inheritedWidth = false, inheritedHeight = false;

    if (typeof params === "undefined" || !("width" in params)) {
        width = window.innerWidth;
        inheritedWidth = true;
    } else {
        width = params.width;
    }

    if (typeof params === "undefined" || !("height" in params)) {
        height = window.innerHeight;
        inheritedHeight = true;
    } else {
        height = params.height;
    }

    if (typeof params === "undefined" || !("scale" in params)) {
        scale = 5;
    } else {
        scale = params.scale;
    }

    if (typeof params === "undefined" || !("offset" in params)) {
        offset = new THREE.Vector3(0, 0, 0);
    } else {
        offset = params.offset;
    }

    if (typeof params === "undefined" || !("projUp" in params)) {
        projUp = new THREE.Vector3(0, 1, -1);
    } else {
        projUp = params.projUp;
    }

    if (typeof params === "undefined" || !("projRight" in params)) {
        projRight = new THREE.Vector3(1, 0, -1);
    } else {
        projRight = params.projRight;
    }

    var domElement = init();
    lightUpdate();
    render();
    return domElement;

    function init() {
        scene = new THREE.Scene();
        edgeScene = new THREE.Scene();

        camera = new SolvespaceCamera(width, height, scale, projUp, projRight, offset);
        camera.NormalizeProjectionVectors();

        mesh = createMesh(obj);
        scene.add(mesh);
        edges = createEdges(obj);
        edgeScene.add(edges);

        for (var i = 0; i < obj.lights.d.length; i++) {
            var lightColor = new THREE.Color(obj.lights.d[i].intensity,
                obj.lights.d[i].intensity, obj.lights.d[i].intensity);
            var directionalLight = new THREE.DirectionalLight(lightColor, 1);
            directionalLight.position.set(obj.lights.d[i].direction[0],
                obj.lights.d[i].direction[1], obj.lights.d[i].direction[2]);
            directionalLightArray.push(directionalLight);
            scene.add(directionalLight);
        }

        var lightColor = new THREE.Color(obj.lights.a, obj.lights.a, obj.lights.a);
        var ambientLight = new THREE.AmbientLight(lightColor.getHex());
        scene.add(ambientLight);

        renderer = new THREE.WebGLRenderer({ antialias: true});
        renderer.setSize(width * window.devicePixelRatio, height * window.devicePixelRatio);
        renderer.autoClear = false;
        renderer.domElement.style =
            "width:  " + width  + "px;" +
            "height: " + height + "px;";

        controls = new SolvespaceControls(camera, renderer.domElement);
        controls.addEventListener("change", render);
        controls.addEventListener("change", lightUpdate);

        if(inheritedWidth || inheritedHeight) {
            window.addEventListener("resize", resize);
        }

        animate();
        return renderer.domElement;
    }

    function resize() {
        scale = camera.zoomScale;
        if(inheritedWidth) {
            scale *= window.innerWidth / width;
            width = window.innerWidth;
        }
        if(inheritedHeight) {
            scale *= window.innerHeight / height;
            height = window.innerHeight;
        }

        camera.renderWidth = width;
        camera.renderHeight = height;
        camera.zoomScale = scale;

        renderer.setSize(width * window.devicePixelRatio, height * window.devicePixelRatio);
        renderer.domElement.style =
            "width:  " + width  + "px;" +
            "height: " + height + "px;";

        render();
    }

    function animate() {
        requestAnimationFrame(animate);
        controls.update();
    }

    function render() {
        var context = renderer.getContext();
        camera.updateProjectionMatrix();
        renderer.clear();

        context.depthRange(0.1, 1);
        renderer.render(scene, camera);

        context.depthRange(0.1-(2/60000.0), 1-(2/60000.0));
        renderer.render(edgeScene, camera);
    }

    function lightUpdate() {
        var changeBasis = new THREE.Matrix4();

        // The original light positions were in camera space.
        // Project them into standard space using camera's basis
        // vectors (up, target, and their cross product).
        var n = new THREE.Vector3().crossVectors(camera.up, camera.right);
        changeBasis.makeBasis(camera.right, camera.up, n);

        for (var i = 0; i < 2; i++) {
            var newLightPos = changeBasis.applyToVector3Array(
                [obj.lights.d[i].direction[0], obj.lights.d[i].direction[1],
                    obj.lights.d[i].direction[2]]);
            directionalLightArray[i].position.set(newLightPos[0],
                newLightPos[1], newLightPos[2]);
        }
    }

    function createMesh(meshObj) {
        var geometry = new THREE.Geometry();
        var materialIndex = 0;
        var materialList = [];
        var opacitiesSeen = {};

        for (var i = 0; i < meshObj.points.length; i++) {
            geometry.vertices.push(new THREE.Vector3(meshObj.points[i][0],
                meshObj.points[i][1], meshObj.points[i][2]));
        }

        for (var i = 0; i < meshObj.faces.length; i++) {
            var currOpacity = ((meshObj.colors[i] & 0xFF000000) >>> 24) / 255.0;
            if (opacitiesSeen[currOpacity] === undefined) {
                opacitiesSeen[currOpacity] = materialIndex;
                materialIndex++;
                materialList.push(new THREE.MeshLambertMaterial({
                    vertexColors: THREE.FaceColors,
                    opacity: currOpacity,
                    transparent: true,
                    side: THREE.DoubleSide
                }));
            }

            geometry.faces.push(new THREE.Face3(meshObj.faces[i][0],
                meshObj.faces[i][1], meshObj.faces[i][2],
                [new THREE.Vector3(meshObj.normals[i][0][0],
                    meshObj.normals[i][0][1], meshObj.normals[i][0][2]),
                 new THREE.Vector3(meshObj.normals[i][1][0],
                    meshObj.normals[i][1][1], meshObj.normals[i][1][2]),
                 new THREE.Vector3(meshObj.normals[i][2][0],
                    meshObj.normals[i][2][1], meshObj.normals[i][2][2])],
                new THREE.Color(meshObj.colors[i] & 0x00FFFFFF),
                opacitiesSeen[currOpacity]));
        }

        geometry.computeBoundingSphere();
        return new THREE.Mesh(geometry, new THREE.MultiMaterial(materialList));
    }

    function createEdges(meshObj) {
        var geometry = new THREE.Geometry();
        var material = new THREE.LineBasicMaterial();

        for (var i = 0; i < meshObj.edges.length; i++) {
            geometry.vertices.push(new THREE.Vector3(meshObj.edges[i][0][0],
                    meshObj.edges[i][0][1], meshObj.edges[i][0][2]),
                new THREE.Vector3(meshObj.edges[i][1][0],
                    meshObj.edges[i][1][1], meshObj.edges[i][1][2]));
        }

        geometry.computeBoundingSphere();
        return new THREE.LineSegments(geometry, material);
    }
};

