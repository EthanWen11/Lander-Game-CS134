//--------------------------------------------------------------
//
//  Kevin M. Smith
//
//  Octree Test - startup scene
//
//
//  Student Name:   < Your Name goes Here >
//  Date: <date of last version>


#include "ofApp.h"
#include "Util.h"
#include <glm/gtx/intersect.hpp>
#include <algorithm>

//--------------------------------------------------------------
// setup scene, lighting, state and load geometry
//
//--------------------------------------------------------------
// setup scene, lighting, state and load geometry
//
void ofApp::setup() {
	bWireframe        = false;
	bDisplayPoints    = false;
	bAltKeyDown       = false;
	bCtrlKeyDown      = false;
	bLanderLoaded     = false;
	bTerrainSelected  = true;
	// ofSetWindowShape(1024, 768);

	cam.setDistance(10);
	cam.setNearClip(.1);
	cam.setFov(65.5);   // approx equivalent to 28mm in 35mm format
	ofSetVerticalSync(true);
	cam.disableMouseInput();
	ofEnableSmoothing();
	ofEnableDepthTest();

	// Use GL_TEXTURE_2D for everything (skybox + particles + shaders)
	// and never change this again in setup.
	ofDisableArbTex();

	// setup lighting (Mars-style lights you already defined)
	initLightingAndMaterials();

	// Load terrain
	// mars.load("geo/mars-low-5x-v2.obj");
	mars.load("geo/finalproject_landscape.obj");
	mars.enableTextures();
	mars.enableMaterials();
	mars.enableNormals();
	mars.setScaleNormalization(false);

	// GUI
//	gui.setup();
//	gui.add(numLevels.setup("Number of Octree Levels", 1, 1, 10));
//	gui.add(bTimingInfo.setup("Show Timing", false));
//	gui.setPosition(10, 10);
//	bHide = false;

	// Cameras
	onboard.setFov(100);
	onboard.setNearClip(8);
	onboard.setFarClip(5000);

	tracking.setPosition(104, 62.5, 229);
	tracking.setFov(50);
	tracking.setNearClip(0.1);
	tracking.setFarClip(5000);

	// Create Octree for testing.
	float t1 = ofGetElapsedTimeMillis();
	buildOctreeFromAssimp(mars, octree, 20);
	float t2 = ofGetElapsedTimeMillis();
	cout << "Time to Create Octree: " << (t2 - t1) << " ms" << endl;
	cout << "Number of Verts: " << mars.getMesh(0).getNumVertices() << endl;

	testBox = Box(Vector3(3, 3, 0), Vector3(5, 5, 2));

	// Build landing zones based on terrain
	initLandingZones();

	// Forces for particle systems
	turbForce    = new TurbulenceForce(ofVec3f(-20, -20, -20), ofVec3f(20, 20, 20));
	gravityForce = new GravityForce(ofVec3f(0, -10, 0));
	radialForce  = new ImpulseRadialForce(1000.0);

	emitter.sys->addForce(turbForce);
	emitter.sys->addForce(gravityForce);
	emitter.sys->addForce(radialForce);

	turbForce    = new TurbulenceForce(ofVec3f(particleTurbMin.x, particleTurbMin.y, particleTurbMin.z),
									   ofVec3f(particleTurbMax.x, particleTurbMax.y, particleTurbMax.z));
	gravityForce = new GravityForce(ofVec3f(0, -particleGravity, 0));
	radialForce  = new ImpulseRadialForce(particleRadialForceVal);

	radialEmitter.sys->addForce(turbForce);
	radialEmitter.sys->addForce(gravityForce);
	radialEmitter.sys->addForce(radialForce);

	// Particle emitter setup
	emitter.setVelocity(ofVec3f(0, 0, 0));
	emitter.setOneShot(false);
	emitter.setEmitterType(ConeEmitter);
	emitter.setGroupSize(20);
	emitter.setRate(30);
	emitter.setLifespan(3);

	radialEmitter.setVelocity(ofVec3f(0, 0, 0));
	radialEmitter.setOneShot(true);
	radialEmitter.setEmitterType(RadialEmitter);
	radialEmitter.setGroupSize(5000);
	radialEmitter.setRate(1);
	radialEmitter.setLifespan(5);
	
	numLevels = 5;

	// Load particle texture (uses same non-ARB setup as skybox)
	if (!ofLoadImage(particleTex, "images/dot.png")) {
		cout << "Particle Texture File: images/dot.png not found" << endl;
		ofExit();
	}

	// Load particle shader
#ifdef TARGET_OPENGLES
	shader.load("shaders_gles/shader");
#else
	shader.load("shaders/shader");
#endif

	// Load Mars skybox texture (same image will be used on all 6 faces)
	if (!skyTex.load("images/mars_skybox_gradient_stars.png")) {
		cout << "Could not load images/mars_skybox_gradient_stars.png" << endl;
	}


	// Create a big cube around the camera; same texture on all sides
	skybox.set(5000);                 // size of the cube
	skybox.setResolution(1, 1, 1);    // 1 quad per face is enough
	skybox.mapTexCoordsFromTexture(skyTex.getTexture());
	
	// Load audio assets
	if (!thrustSound.load("sounds/thrust_loop.wav")) {
		cout << "Warning: could not load sounds/thrust_loop.wav" << endl;
	} else {
		thrustSound.setLoop(true);
		thrustSound.setVolume(0.8f);
	}

	if (!explosionSound.load("sounds/explosion.wav")) {
		cout << "Warning: could not load sounds/explosion.wav" << endl;
	} else {
		explosionSound.setLoop(false);
		explosionSound.setMultiPlay(true);
		explosionSound.setVolume(1.0f);
	}

	if (!ambientSound.load("sounds/ambient_wind.wav")) {
		cout << "Warning: could not load sounds/ambient_wind.wav" << endl;
	} else {
		ambientSound.setLoop(true);
		ambientSound.setVolume(0.5f);
		ambientSound.play();
		bAmbientPlaying = true;
	}

	if (!successSound.load("sounds/fanfare.wav")) {
		cout << "Warning: could not load sounds/fanfare.wav" << endl;
	}
	else {
		successSound.setLoop(false);
		successSound.setMultiPlay(true);
		successSound.setVolume(1.0f);
	}
}

void ofApp::loadVbo() {
	// If both systems are empty, clear VBO and bail
	if (emitter.sys->particles.size() < 1 && radialEmitter.sys->particles.size() < 1) {
		vbo.clear();
		vboCount = 0;
		return;
	}

	vector<ofVec3f> sizes;
	vector<ofVec3f> points;

	// thruster particles
	for (int i = 0; i < emitter.sys->particles.size(); i++) {
		points.push_back(emitter.sys->particles[i].position);
		sizes.push_back(ofVec3f(25.0f));
	}

	// explosion particles
	for (int i = 0; i < radialEmitter.sys->particles.size(); i++) {
		points.push_back(radialEmitter.sys->particles[i].position);
		sizes.push_back(ofVec3f(25.0f));
	}

	int total = (int)points.size();

	vbo.clear();
	if (total > 0) {
		vbo.setVertexData(points.data(), total, GL_DYNAMIC_DRAW);
		vbo.setNormalData(sizes.data(), total, GL_DYNAMIC_DRAW);
	}
	vboCount = total;
}

 
//--------------------------------------------------------------
// incrementally update scene (animation)
//
void ofApp::update() {
	// dt = time since last frame (in seconds)
	float dt = ofGetLastFrameTime();

	// run physics step if enabled
	if (bUsePhysics) {
		updatePhysics(dt);
	}

	if (bResolvingCollision) {
		stepCollisionResolve(dt);
	}

	emitter.update();
	radialEmitter.update();

	onboard.setPosition(landerPos);
	tracking.lookAt(landerPos);
	
	float terrainMinY = mars.getSceneMin().y;

	if (currentCam == 2) {

		if (bEasyCamFollowShip && bLanderLoaded) {
			// follow behind and above the ship based on its yaw
			float yawRad = glm::radians(landerAngleY);

			glm::vec3 forward = glm::vec3(sin(yawRad), 0.0f, -cos(yawRad));

			glm::vec3 offset = (-forward * 80.0f) + glm::vec3(0.0f, 40.0f, 0.0f);
			glm::vec3 desired = landerPos + offset;

			ofVec3f camPos(desired.x, desired.y, desired.z);

			// clamp above terrain
			if (camPos.y < terrainMinY + 5.0f) {
				camPos.y = terrainMinY + 5.0f;
			}

			cam.setPosition(camPos);
			cam.lookAt(ofVec3f(landerPos.x, landerPos.y, landerPos.z));
		}
		else {
			// no follow mode: just enforce height clamp
			ofVec3f pos = cam.getPosition();
			if (pos.y < terrainMinY + 5.0f) {
				pos.y = terrainMinY + 5.0f;
				cam.setPosition(pos);
			}
		}
	}
}

// --------------------------------------------------------------
// Update lander physics and game logic each frame.
// dt = time step in seconds
//
void ofApp::updatePhysics(float dt) {
	if (!bLanderLoaded) return;

	// stop physics and input once game is over (win or lose)
	if (bGameOver) {
		return;
	}

	if (gameState == STATE_READY) {
		gameState = STATE_PLAYING;
	}

	accumulatedForces = glm::vec3(0.0f);

	// gravity
	accumulatedForces += glm::vec3(0.0f, gravityAccel * landerMass, 0.0f);

	// turbulence: small lateral noise force in XZ
	if (bEnableTurbulence && !bOnGround && currentAltitude > 3.0f) {
		turbulenceTime += dt;

		float nx = ofSignedNoise(turbulenceTime * turbulenceFreq, 0.0f, 0.0f);
		float nz = ofSignedNoise(0.0f, 0.0f, turbulenceTime * turbulenceFreq);

		float len2 = nx * nx + nz * nz;
		if (len2 > 1e-4f) {
			glm::vec3 dir(nx, 0.0f, nz);
			dir = glm::normalize(dir);

			glm::vec3 turbulenceForce = dir * (turbulenceStrength * landerMass);
			accumulatedForces += turbulenceForce;
		}
	}

	bool anyThrustThisFrame = false;
	glm::vec3 heading;

	if (!bOutOfFuel && fuelRemaining > 0.0f) {

		float yawRad = glm::radians(landerAngleY);

		glm::vec3 forward = glm::vec3(sin(yawRad), 0.0f, -cos(yawRad));
		glm::vec3 right   = glm::vec3(cos(yawRad), 0.0f, sin(yawRad));
		glm::vec3 up      = glm::vec3(0.0f, 1.0f, 0.0f);

		heading = forward;

		if (thrustForward) {
			accumulatedForces += forward * mainThrust;
			anyThrustThisFrame = true;
		}
		if (thrustBackward) {
			accumulatedForces -= forward * mainThrust;
			anyThrustThisFrame = true;
		}
		if (thrustLeft) {
			accumulatedForces -= right * sideThrust;
			anyThrustThisFrame = true;
		}
		if (thrustRight) {
			accumulatedForces += right * sideThrust;
			anyThrustThisFrame = true;
		}
		if (thrustUp) {
			accumulatedForces += up * verticalThrust;
			anyThrustThisFrame = true;
		}
		if (thrustDown) {
			accumulatedForces -= up * verticalThrust;
			anyThrustThisFrame = true;
		}

		// yaw torque (currently unused; rotateLeft/right never set)
		const float yawTorque = 90.0f;
		if (rotateLeft) {
			angularVelY += yawTorque * dt;
		}
		if (rotateRight) {
			angularVelY -= yawTorque * dt;
		}

		// clamp yaw rate so it doesn't spin out of control
		angularVelY = glm::clamp(angularVelY, -maxAngularSpeed, maxAngularSpeed);

		if (anyThrustThisFrame) {
			fuelRemaining -= dt;
			if (fuelRemaining <= 0.0f) {
				fuelRemaining = 0.0f;
				bOutOfFuel    = true;

				// OUT-OF-FUEL terminal condition:
				// if not all zones visited yet, this is a loss.
				if (!bGameOver) {
					if (score >= (int)landingZones.size()) {
						// all zones already visited → still a win
						bPlayerWon      = true;
						gameOverMessage = "All zones visited, but fuel depleted.";
						if (successSound.isLoaded()) successSound.play();
						gameState = STATE_LANDED_SAFE;
					}
					else {
						bPlayerWon      = false;
						gameOverMessage = "Out of fuel before reaching all zones.";
						gameState = STATE_OUT_OF_FUEL;
					}
					bGameOver = true;
				}
			}
		}
	}

	// Thruster exhaust particles + looped thrust audio
	if (anyThrustThisFrame && !bOutOfFuel && gameState != STATE_CRASHED) {
		if (!emitterActive) {
			emitter.start();
			emitterActive = true;
		}
		if (!bThrustLoopPlaying && thrustSound.isLoaded()) {
			thrustSound.play();
			bThrustLoopPlaying = true;
		}
	} else {
		if (emitterActive) {
			emitter.stop();
			emitterActive = false;
		}
		if (bThrustLoopPlaying && thrustSound.isLoaded()) {
			thrustSound.stop();
			bThrustLoopPlaying = false;
		}
	}

	// integrate motion
	glm::vec3 acceleration = accumulatedForces / landerMass;
	landerVel += acceleration * dt;

	// --- SPEED CAPS -------------------------------------------------
	// cap horizontal (XZ) speed
	float speedXZ = glm::length(glm::vec2(landerVel.x, landerVel.z));
	if (speedXZ > maxHorizontalSpeed) {
		float scale = maxHorizontalSpeed / std::max(speedXZ, 0.0001f);
		landerVel.x *= scale;
		landerVel.z *= scale;
	}

	// cap vertical (Y) speed
	landerVel.y = glm::clamp(landerVel.y, -maxVerticalSpeed, maxVerticalSpeed);
	// ----------------------------------------------------------------

	// apply damping (air resistance / friction)
	landerVel *= linearDamping;

	landerPos += landerVel * dt;

	landerAngleY += angularVelY * dt;
	angularVelY *= angularDamping;

	updateAltitudeSensor();
	updateCollisionAndLanding(dt);

	lander.setPosition(landerPos.x, landerPos.y, landerPos.z);
	lander.setRotation(0, landerAngleY, 0, 1, 0);

	emitter.setEmitterPosition(landerPos + ofVec3f(0, -18, 0));
	radialEmitter.setEmitterPosition(landerPos);
	lander.setRotation(0, landerAngleY, 0, 1, 0);
}



// --------------------------------------------------------------
// Cast a ray straight down from the lander and estimate altitude
// above the terrain using the octree.
// This only updates logic values (no drawing).
//
void ofApp::updateAltitudeSensor() {
	// if sensor is off or lander not loaded, do nothing
	if (!bAltitudeSensorOn || !bLanderLoaded) return;

	// put ray origin a bit above the lander, shoot straight down
	glm::vec3 origin = landerPos + glm::vec3(0.0f, 50.0f, 0.0f);
	glm::vec3 dir    = glm::vec3(0.0f, -1.0f, 0.0f);

	Ray ray(
		Vector3(origin.x, origin.y, origin.z),
		Vector3(dir.x,    dir.y,    dir.z)
	);

	TreeNode hitNode;
	bool hit = octree.intersect(ray, octree.root, hitNode);
	if (!hit || hitNode.points.empty()) {
		// if we did not hit any node, leave altitude as-is
		return;
	}

	// estimate ground Y under the lander using the vertices
	// stored in the hit node
	ofMesh &mesh = octree.mesh;

	bool foundAny = false;
	float bestGroundY = -1000000.0f; // very low number

	for (int idx : hitNode.points) {
		ofVec3f v = mesh.getVertex(idx);

		// only consider vertices near the lander in XZ and below it in Y
		float dx = v.x - landerPos.x;
		float dz = v.z - landerPos.z;
		float distXZ = sqrtf(dx * dx + dz * dz);

		if (distXZ < 20.0f && v.y <= landerPos.y) {
			foundAny = true;
			if (v.y > bestGroundY) {
				bestGroundY = v.y;
			}
		}
	}

	if (foundAny) {
		currentAltitude = landerPos.y - bestGroundY;
	}
}


// --------------------------------------------------------------
// Check collisions against terrain using the octree and decide
// if we are landed safely, hard landed, or crashed.
// Also applies a small impulse response to push the lander up.
//
void ofApp::updateCollisionAndLanding(float dt) {
	if (!bLanderLoaded) return;

	// once game is over (crash, out-of-fuel, or win), stop evaluating
	if (bGameOver) {
		return;
	}

	// --------------------------------------------------
	// 1) LANDING ZONE VOLUME TRIGGER
	//    (just being inside the box marks the zone)
	// --------------------------------------------------
	int  zoneIndex = -1;
	bool inZone    = isInsideLandingZone(landerPos, zoneIndex);
	lastLandingZoneIndex = inZone ? zoneIndex : -1;

	if (inZone &&
		zoneIndex >= 0 &&
		zoneIndex < (int)landingZoneVisited.size() &&
		!landingZoneVisited[zoneIndex]) {

		landingZoneVisited[zoneIndex] = true;
		score += 1; // 1 more landing zone completed

		lastLandingSummary =
			"Entered zone " + std::to_string(zoneIndex + 1) +
			" (" + std::to_string(score) + "/" +
			std::to_string((int)landingZones.size()) + ")";

		// Win condition: all zones visited.
		// Crash/out-of-fuel still overrides elsewhere.
		if (score >= (int)landingZones.size() &&
			gameState != STATE_CRASHED &&
			gameState != STATE_OUT_OF_FUEL) {

			bPlayerWon      = true;
			bGameOver       = true;
			gameState       = STATE_LANDED_SAFE;
			gameOverMessage = "All landing zones completed!";

			if (successSound.isLoaded()) {
				successSound.play();
			}
		}
	}

	// --------------------------------------------------
	// 2) TERRAIN COLLISION / CRASH LOGIC
	// --------------------------------------------------
	Box bounds = currentLanderBounds();

	colBoxList.clear();
	bool hit = octree.intersect(bounds, octree.root, colBoxList);

	if (!hit || colBoxList.empty()) {
		bOnGround = false;
		return;
	}

	bOnGround = true;

	float vy = landerVel.y;

	// only treat as impact if moving downward
	if (vy >= 0.0f) {
		return;
	}

	float impactSpeed = -vy;
	lastImpactSpeed   = impactSpeed;

	// thresholds
	float safeSpeed = safeLandingSpeed;
	float hardSpeed = crashLandingSpeed;

	//--------------------------------------------------
	// CRASH: immediate loss (even if all zones were visited)
	//--------------------------------------------------
	if (impactSpeed >= hardSpeed) {
		// explosion setup (same as before)
		radialEmitter.setGroupSize(8000);
		radialEmitter.setLifespan(2.0f);
		radialEmitter.setRate(1);
		radialEmitter.setVelocity(ofVec3f(0, 70, 0));

		if (radialForce) {
			radialForce = new ImpulseRadialForce(6000.0f);

			radialEmitter.sys->forces.clear();
			radialEmitter.sys->addForce(turbForce);
			radialEmitter.sys->addForce(gravityForce);
			radialEmitter.sys->addForce(radialForce);
		}

		radialEmitter.setEmitterPosition(landerPos);
		radialEmitter.sys->reset();
		radialEmitter.start();

		emitter.stop();

		if (explosionSound.isLoaded()) {
			explosionSound.play();
		}
		if (bThrustLoopPlaying && thrustSound.isLoaded()) {
			thrustSound.stop();
			bThrustLoopPlaying = false;
		}

		bLanderLoaded = false;
		lander.clear();

		gameState       = STATE_CRASHED;
		bPlayerWon      = false;
		bGameOver       = true;
		gameOverMessage = "Crashed before reaching all zones.";

		return;
	}

	//--------------------------------------------------
	// SAFE / HARD LANDING (no crash) – just bounce/settle
	//--------------------------------------------------
	bHardLanding = (impactSpeed >= safeSpeed);

	// collision response (bounce) – do NOT freeze engines here
	const glm::vec3 groundNormal(0.0f, 1.0f, 0.0f);
	const float restitution = 0.25f;

	if (impactSpeed > 0.5f) {
		landerVel.y  = impactSpeed * restitution;
		landerVel.x *= 0.2f;
		landerVel.z *= 0.2f;
	}
	else {
		landerVel = glm::vec3(0.0f);
	}

	landerPos = landerPos + groundNormal * 0.05f;
}


void ofApp::initLandingZones() {
	landingZones.clear();

	// --- tweakable parameters (edit these only) ---
	float LANDING_ZONE_SCALE      = 4.0f;    // footprint size (X/Z)
	float LANDING_ZONE_OFFSET     = 250.0f;  // distance from terrain center in X/Z
	float LANDING_ZONE_HEIGHT     = 32.0f;   // vertical thickness of the zone
	float LANDING_ZONE_Y_OFFSET   = 95.0f;  // RAISE/LOWER all zones in Y

	// terrain bounds from octree root
	ofVec3f terrainMin(
		octree.root.box.min().x(),
		octree.root.box.min().y(),
		octree.root.box.min().z()
	);
	ofVec3f terrainMax(
		octree.root.box.max().x(),
		octree.root.box.max().y(),
		octree.root.box.max().z()
	);

	ofVec3f terrainCenter = (terrainMin + terrainMax) * 0.5f;

	float baseSize  = (terrainMax.x - terrainMin.x) * 0.04f;
	float finalSize = baseSize * LANDING_ZONE_SCALE;

	// horizontal offsets from center
	std::vector<glm::vec3> offsets = {
		{  LANDING_ZONE_OFFSET, 0,  0 },
		{ -LANDING_ZONE_OFFSET, 0,  LANDING_ZONE_OFFSET },
		{  0,                   0, -LANDING_ZONE_OFFSET }
	};

	// choose a "floor" Y and then shift it by LANDING_ZONE_Y_OFFSET
	float rawFloorY = terrainMin.y;                 // very bottom of mesh
	float groundY   = rawFloorY + LANDING_ZONE_Y_OFFSET;

	for (auto &off : offsets) {
		glm::vec3 center = glm::vec3(terrainCenter.x, groundY, terrainCenter.z) + off;

		glm::vec3 min(
			center.x - finalSize * 0.5f,
			groundY,
			center.z - finalSize * 0.5f
		);

		glm::vec3 max(
			center.x + finalSize * 0.5f,
			groundY + LANDING_ZONE_HEIGHT,
			center.z + finalSize * 0.5f
		);

		landingZones.push_back(Box(
			Vector3(min.x, min.y, min.z),
			Vector3(max.x, max.y, max.z)
		));
	}

	// reset per-zone visited flags and progress
	landingZoneVisited.assign(landingZones.size(), false);
	score                 = 0;      // zones visited
	lastLandingZoneIndex  = -1;
	lastLandingSummary    = "None";
}



bool ofApp::isInsideLandingZone(const glm::vec3 &pos, int &zoneIndex)  {
	for (size_t i = 0; i < landingZones.size(); ++i) {
		Box zone = landingZones[i];
		Vector3 min = zone.min();
		Vector3 max = zone.max();

		if (pos.x >= min.x() && pos.x <= max.x() &&
			pos.y >= min.y() && pos.y <= max.y() &&
			pos.z >= min.z() && pos.z <= max.z()) {
			zoneIndex = static_cast<int>(i);
			return true;
		}
	}
	zoneIndex = -1;
	return false;
}


//--------------------------------------------------------------
void ofApp::loadDefaultLander() {
	std::string path = "geo/finalproject_vehicle.obj";  // change name if your ship file is different

	if (lander.load(path)) {
		bLanderLoaded = true;
		lander.setScaleNormalization(false);

		// spawn a bit above the terrain center so it can fall
		lander.setPosition(0, 100, 0);

		// sync physics state with the visual lander
		landerPos    = lander.getPosition();
		landerVel    = glm::vec3(0.0f);
		landerAngleY = 0.0f;
		angularVelY  = 0.0f;

		// reset game / fuel state so we can fly
		gameState     = STATE_READY;
		fuelRemaining = fuelMaxSeconds;
		bOutOfFuel    = false;

		// clear all thrust/rotation flags
		thrustForward = false;
		thrustBackward = false;
		thrustLeft = false;
		thrustRight = false;
		thrustUp = false;
		thrustDown = false;
		rotateLeft = false;
		rotateRight = false;

		bboxList.clear();
		for (int i = 0; i < lander.getMeshCount(); i++) {
			bboxList.push_back(Octree::meshBounds(lander.getMesh(i)));
		}

		std::cout << "Default lander loaded from " << path << std::endl;
	}
	else {
		std::cout << "Error: can't load default lander from " << path << std::endl;
	}
}

void ofApp::resetGame() {
	// stop particle effects and audio
	emitter.stop();
	radialEmitter.stop();
	if (bThrustLoopPlaying && thrustSound.isLoaded()) {
		thrustSound.stop();
		bThrustLoopPlaying = false;
	}

	// reset flags
	bGameOver       = false;
	bPlayerWon      = false;
	gameOverMessage = "";
	bOutOfFuel      = false;
	bOnGround       = false;
	bHardLanding    = false;
	lastImpactSpeed = 0.0f;

	// reset physics state
	landerVel    = glm::vec3(0.0f);
	angularVelY  = 0.0f;
	currentAltitude = 0.0f;

	// rebuild / reset landing zones & visited state
	initLandingZones();

	// respawn lander and refuel (loadDefaultLander already sets fuelRemaining,
	// gameState, orientation, etc.)
	loadDefaultLander();
}


string ofApp::gameStateToString(GameState s) {
	switch (s) {
		case STATE_READY:       return "READY";
		case STATE_PLAYING:     return "PLAYING";
		case STATE_LANDED_SAFE: return "LANDED SAFE";
		case STATE_CRASHED:     return "CRASHED";
		case STATE_OUT_OF_FUEL: return "OUT OF FUEL";
		default:                return "UNKNOWN";
	}
}

//--------------------------------------------------------------
void ofApp::drawHUD() {
	ofPushStyle();
	ofDisableDepthTest();
	ofSetColor(255);

	const float lineH  = 18.0f;   // line spacing
	const float margin = 20.0f;

	float x = margin;
	float y = margin;   // start at top-left

	// --- SYSTEM / LANDER ---
	ofDrawBitmapStringHighlight("SYSTEM:   [1] Load Lander", x, y);
	y += lineH;

	// --- PROGRESS (zones visited) ---
	std::string zonesStr = "ZONES LANDED: " +
		std::to_string(score) + " / " +
		std::to_string((int)landingZones.size());

	std::string lastSummaryStr = "LAST LANDING: " + lastLandingSummary;

	ofDrawBitmapStringHighlight(zonesStr,       x, y); y += lineH;
	ofDrawBitmapStringHighlight(lastSummaryStr, x, y); y += lineH * 2;


	// --- FLIGHT INFO ---
	// fuel
	std::string fuelStr     = "FUEL:        " + std::to_string((int)fuelRemaining) + " sec";

	// altitude
	std::string altitudeStr = "ALTITUDE:   " + std::to_string((int)currentAltitude) + " m";

	// current vertical speed (absolute value of Y velocity)
	float verticalSpeed = std::abs(landerVel.y);
	std::string speedStr     = "VERT SPEED: " + std::to_string((int)verticalSpeed) + " m/s";

	// game state + crash flag
	std::string stateStr     = "STATE:      " + gameStateToString(gameState);
	std::string crashStr     = "CRASHED?:   " + std::string(gameState == STATE_CRASHED ? "YES" : "NO");

	// crash rule – show the threshold used in physics
	std::string crashRuleStr = "CRASH IF VERT SPEED ≥ " +
							   std::to_string((int)crashLandingSpeed) + " m/s";

	ofDrawBitmapStringHighlight(fuelStr,      x, y); y += lineH;
	ofDrawBitmapStringHighlight(altitudeStr,  x, y); y += lineH;
	ofDrawBitmapStringHighlight(speedStr,     x, y); y += lineH;
	ofDrawBitmapStringHighlight(stateStr,     x, y); y += lineH;
	ofDrawBitmapStringHighlight(crashStr,     x, y); y += lineH;

	std::string sensorStr = std::string("ALT SENSOR: ") + (bAltitudeSensorOn ? "ON" : "OFF");
	ofDrawBitmapStringHighlight(sensorStr,    x, y); y += lineH;

	ofDrawBitmapStringHighlight(crashRuleStr, x, y); y += lineH * 2;

	// --- FLIGHT CONTROLS ---
	ofDrawBitmapStringHighlight("FLIGHT:", x, y);
	y += lineH;
	ofDrawBitmapStringHighlight("  ^v arrow-key  Forward / Backward",   x, y); y += lineH;
	ofDrawBitmapStringHighlight("  <> arrow-key  Strafe Left / Right",  x, y); y += lineH;
	ofDrawBitmapStringHighlight("  W/S          Thrust Up / Down",      x, y); y += lineH;
	ofDrawBitmapStringHighlight("  A/D          Strafe Left / Right",   x, y); y += lineH;
	ofDrawBitmapStringHighlight("  Q/E          Rotate Yaw Left / Right", x, y); y += lineH * 2;


	// --- CAMERA CONTROLS ---
	ofDrawBitmapStringHighlight("CAMERAS:", x, y);
	y += lineH;
	ofDrawBitmapStringHighlight("  [2] EasyCam",          x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [3] Tracking Cam",     x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [4] Onboard Cam",      x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [5] Reset EasyCam",    x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [6] EasyCam to Mouse", x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [7] EasyCam to Ship",  x, y); y += lineH;
	std::string followStr = std::string("  [8] EasyCam Follow Ship: ") +
							(bEasyCamFollowShip ? "ON" : "OFF");
	ofDrawBitmapStringHighlight(followStr, x, y); y += lineH * 2;

	// --- OCTREE / DEBUG CONTROLS ---
	ofDrawBitmapStringHighlight("OCTREE / DEBUG:", x, y);
	y += lineH;
	ofDrawBitmapStringHighlight("  [L] Show Leaf Nodes",     x, y); y += lineH;
	ofDrawBitmapStringHighlight("  [O] Show Octree Levels",  x, y); y += lineH;
	
	// --- GAME OVER OVERLAY (WIN/LOSE + RESTART BUTTON) ---
	if (bGameOver) {
		float panelW = 520.0f;
		float panelH = 220.0f;

		// positive offset pushes panel toward bottom of screen
		float gameOverPanelYOffset = 140.0f;   // tweak this

		float px = (ofGetWidth()  - panelW) * 0.5f;
		float py = (ofGetHeight() - panelH) * 0.5f + gameOverPanelYOffset;

		// dimmed background panel (solid)
		ofFill();
		ofSetColor(0, 0, 0, 210);
		ofDrawRectangle(px, py, panelW, panelH);

		// white border for clarity
		ofNoFill();
		ofSetColor(255, 255, 255, 255);
		ofSetLineWidth(2.0f);
		ofDrawRectangle(px, py, panelW, panelH);

		std::string title = bPlayerWon ? "MISSION SUCCESS" : "MISSION FAILED";

		float tx = px + 30.0f;
		float ty = py + 40.0f;

		ofSetColor(255);
		ofDrawBitmapStringHighlight(title,          tx, ty);      ty += lineH;
		ofDrawBitmapStringHighlight(gameOverMessage, tx, ty);      ty += lineH * 2;

		// Restart button – very obvious
		float bw = 260.0f;
		float bh = 50.0f;
		float bx = px + (panelW - bw) * 0.5f;
		float by = py + panelH - bh - 35.0f;

		restartButtonRect.set(bx, by, bw, bh);

		// button background
		ofFill();
		ofSetColor(40, 40, 40, 255);
		ofDrawRectangle(bx, by, bw, bh);

		// button border
		ofNoFill();
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(2.0f);
		ofDrawRectangle(bx, by, bw, bh);

		// button label
		std::string buttonText = "[ CLICK TO RESTART MISSION ]";
		float textX = bx + 18.0f;
		float textY = by + bh * 0.5f + 5.0f;

		ofSetColor(255, 255, 0, 255);
		ofDrawBitmapString(buttonText, textX, textY);
	}
	else {
		// disabled when game is live
		restartButtonRect.set(0, 0, 0, 0);
	}



	ofEnableDepthTest();
	ofPopStyle();
}



//--------------------------------------------------------------

void ofApp::draw() {

	loadVbo();

	// Solid clear color (will be covered by skybox anyway)
	ofBackground(ofColor::black);

	// Choose active camera
	ofCamera * activeCam = nullptr;
	if (currentCam == 2) {
		activeCam = &cam;
	} else if (currentCam == 3) {
		activeCam = &tracking;
	} else if (currentCam == 4) {
		activeCam = &onboard;
	}

	if (activeCam) {
		activeCam->begin();

		// -------------------------------------------------
		// SKYBOX: draw first, with lighting disabled
		// -------------------------------------------------
		ofPushStyle();
		ofPushMatrix();

		ofDisableLighting();
		glDepthMask(GL_FALSE);                 // don't write depth, always behind scene
		ofSetColor(255);                       // ensure texture shows true color

		ofVec3f camPos = activeCam->getPosition();
		ofTranslate(camPos);                   // keep cube centered on camera

		skyTex.bind();
		skybox.draw();                         // same texture on all 6 faces
		skyTex.unbind();

		glDepthMask(GL_TRUE);
		ofEnableLighting();                    // re-enable for terrain / ship

		ofPopMatrix();
		ofPopStyle();

		// -------------------------------------------------
		// SCENE: terrain, lander, octree, etc.
		// -------------------------------------------------
		ofPushMatrix();

		if (bWireframe) {
			// Wireframe mode (no lighting, show axis)
			ofDisableLighting();
			ofSetColor(ofColor::slateGray);
			mars.drawWireframe();

			if (bLanderLoaded) {
				lander.drawWireframe();
				if (!bTerrainSelected) {
					drawAxis(lander.getPosition());
				}
			}
			if (bTerrainSelected) {
				drawAxis(ofVec3f(0, 0, 0));
			}
		} else {
			// Shaded mode with lighting
			ofEnableLighting();
			mars.drawFaces();

			if (bLanderLoaded) {
				lander.drawFaces();

				if (!bTerrainSelected) {
					drawAxis(lander.getPosition());
				}

				// Optional bounding boxes for lander meshes
				if (bDisplayBBoxes) {
					ofNoFill();
					ofSetColor(ofColor::white);
					for (int i = 0; i < lander.getNumMeshes(); i++) {
						ofPushMatrix();
						ofMultMatrix(lander.getModelMatrix());
						ofRotateRad(-PI / 2, 1, 0, 0);
						Octree::drawBox(bboxList[i]);
						ofPopMatrix();
					}
				}

				// Collision boxes when lander is selected
				if (bLanderSelected) {
					ofVec3f min = lander.getSceneMin() + lander.getPosition();
					ofVec3f max = lander.getSceneMax() + lander.getPosition();
					Box bounds = Box(Vector3(min.x, min.y, min.z),
									 Vector3(max.x, max.y, max.z));

					ofSetColor(ofColor::white);
					Octree::drawBox(bounds);

					ofSetColor(ofColor::lightBlue);
					for (int i = 0; i < colBoxList.size(); i++) {
						Octree::drawBox(colBoxList[i]);
					}
				}
			}
		}

		if (bTerrainSelected) {
			drawAxis(ofVec3f(0, 0, 0));
		}

		// Optional vertex display
		if (bDisplayPoints) {
			glPointSize(3);
			ofSetColor(ofColor::green);
			mars.drawVertices();
		}

		// Highlight selected mesh point
		if (bPointSelected) {
			ofSetColor(ofColor::blue);
			ofDrawSphere(selectedPoint, .1);
		}

		// -------------------------------------------------
		// Octree visualization
		// -------------------------------------------------
		ofDisableLighting();
		if (bDisplayLeafNodes) {
			octree.numLeaf = 0; // reset each frame
			octree.drawLeafNodes(octree.root);
			cout << "num leaf: " << octree.numLeaf << endl;
		}
		else if (bDisplayOctree) {
			ofNoFill();
			ofSetColor(ofColor::white);
			octree.draw(numLevels, 0);
		}

		// Landing zones (always visible)
		ofNoFill();
		for (size_t i = 0; i < landingZones.size(); ++i) {
			const Box &zone = landingZones[i];

			bool visited = (i < landingZoneVisited.size() && landingZoneVisited[i]);
			if (visited) {
				ofSetColor(ofColor::green);
			} else {
				ofSetColor(ofColor::red);
			}

			Octree::drawBox(zone);
		}


		// If a node/point is selected, draw a sphere at that location
		if (pointSelected) {
			ofVec3f p = octree.mesh.getVertex(selectedNode.points[0]);
			ofVec3f d = p - cam.getPosition();
			ofSetColor(ofColor::lightGreen);
			ofDrawSphere(p, .02f * d.length());
		}

		// -------------------------------------------------
		// PARTICLES: thruster + explosion (independent of lander)
		// -------------------------------------------------
		if (vboCount > 0) {
			ofEnableBlendMode(OF_BLENDMODE_ALPHA);
			glEnable(GL_PROGRAM_POINT_SIZE);
			glEnable(GL_POINT_SPRITE);
			ofDisableDepthTest();

			shader.begin();
			shader.setUniformTexture("tex", particleTex, 0);
			shader.setUniform4f("particleColor", 1.0, 1.0, 1.0, 1.0);

			vbo.draw(GL_POINTS, 0, vboCount);

			shader.end();
			ofDisableBlendMode();
		}

		ofPopMatrix();
		activeCam->end();
	}

	// GUI + HUD
	glDepthMask(false);
	glDepthMask(true);

	drawHUD();
}




//
// Draw an XYZ axis in RGB at world (0,0,0) for reference.
//
void ofApp::drawAxis(ofVec3f location) {

	ofPushMatrix();
	ofTranslate(location);

	ofSetLineWidth(1.0);

	// X Axis
	ofSetColor(ofColor(255, 0, 0));
	ofDrawLine(ofPoint(0, 0, 0), ofPoint(1, 0, 0));
	

	// Y Axis
	ofSetColor(ofColor(0, 255, 0));
	ofDrawLine(ofPoint(0, 0, 0), ofPoint(0, 1, 0));

	// Z Axis
	ofSetColor(ofColor(0, 0, 255));
	ofDrawLine(ofPoint(0, 0, 0), ofPoint(0, 0, 1));

	ofPopMatrix();
}


void ofApp::keyPressed(int key) {
	ofVec3f p;	// Initialize temp storage for point ray intersection (see ['6'])
	switch (key) {

	// ------------------------------
	// SYSTEM / TOOLS
	// ------------------------------
	case 'B':
	case 'b':
		bDisplayBBoxes = !bDisplayBBoxes;
		break;

	case 'C':
	case 'c':
		if (cam.getMouseInputEnabled()) cam.disableMouseInput();
		else cam.enableMouseInput();
		break;

	case 'F':
	case 'f':
		ofToggleFullscreen();
		break;

	case 'H':
	case 'h':
		bAltitudeSensorOn = !bAltitudeSensorOn;
		break;

	case 'L':
	case 'l':
		bDisplayLeafNodes = !bDisplayLeafNodes;
		break;

	case 'O':
	case 'o':
		bDisplayOctree = !bDisplayOctree;
		break;

	case 'r':
	case 'R':
		cam.reset();
		break;

	// screenshot moved to P
	case 'p':
	case 'P':
		savePicture();
		break;

	case 'v':
	case 'V':
		togglePointsDisplay();
		break;

	// wireframe moved to Z
	case 'z':
	case 'Z':
		toggleWireframeMode();
		break;

	// load default lander
	case '1':
		loadDefaultLander();
		break;

	case 'X':
	case 'x':
		// ensure we’re up-to-date on current overlaps
		if (bLanderLoaded) {
			recomputeCollisionList();
			startCollisionResolve();
		}
		break;

	// ------------------------------
	// FLIGHT CONTROLS  (PRESS = START THRUST)
	// ------------------------------
	case OF_KEY_UP:
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustForward = true;
		}
		break;

	case OF_KEY_DOWN:
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustBackward = true;
		}
		break;

	case OF_KEY_LEFT:
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustLeft = true;
		}
		break;

	case OF_KEY_RIGHT:
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustRight = true;
		}
		break;

	case 'a':
	case 'A':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustLeft = true;
		}
		break;

	case 'd':
	case 'D':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustRight = true;
		}
		break;

	case 'w':
	case 'W':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustUp = true;
		}
		break;

	case 's':
	case 'S':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			thrustDown = true;
		}
		break;

	case 'q':
	case 'Q':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			rotateLeft = true;
		}
		break;

	case 'e':
	case 'E':
		if (bLanderLoaded && !bGameOver &&
			(gameState == STATE_READY || gameState == STATE_PLAYING)) {
			rotateRight = true;
		}
		break;

	// ------------------------------
	// CAMERA
	// ------------------------------
	case '2':	// default EasyCam
		currentCam = 2;
		break;
	case '3':	// Tracking cam
		currentCam = 3;
		break;
	case '4':	// Onboard cam
		currentCam = 4;
		break;
	case '5':	// Reset EasyCam
		cam.reset();
		cam.setPosition(0, 100, 100);
		cam.lookAt(ofVec3f(0, 0, 0));
		cam.setDistance(100);
		break;
	case '6':	// Look at Octree Selected Point
		raySelectWithOctree(p);
		camLookAt(cam, p);
		break;
	case '7':	// Look at Lander
		cam.lookAt(landerPos);
		break;
	case '8':   // toggle EasyCam follow-ship mode
		bEasyCamFollowShip = !bEasyCamFollowShip;
		break;

	// ------------------------------
	// MODIFIERS
	// ------------------------------
	case OF_KEY_ALT:
		cam.enableMouseInput();
		bAltKeyDown = true;
		break;

	case OF_KEY_CONTROL:
		bCtrlKeyDown = true;
		break;

	case OF_KEY_SHIFT:
		break;

	case OF_KEY_DEL:
		break;

	default:
		break;
	}
}


//--------------------------------------------------------------
void ofApp::toggleWireframeMode() {
	bWireframe = !bWireframe;
}

//--------------------------------------------------------------
void ofApp::toggleSelectTerrain() {
	bTerrainSelected = !bTerrainSelected;
}

//--------------------------------------------------------------
void ofApp::togglePointsDisplay() {
	bDisplayPoints = !bDisplayPoints;
}


void ofApp::keyReleased(int key) {

	switch (key) {

	case OF_KEY_ALT:
		cam.disableMouseInput();
		bAltKeyDown = false;
		break;

	case OF_KEY_CONTROL:
		bCtrlKeyDown = false;
		break;

	case OF_KEY_SHIFT:
		break;

	// FLIGHT CONTROLS  (RELEASE = STOP THRUST / ROTATION)
	case OF_KEY_UP:
		thrustForward = false;
		break;

	case OF_KEY_DOWN:
		thrustBackward = false;
		break;

	case OF_KEY_LEFT:
		thrustLeft = false;
		break;

	case OF_KEY_RIGHT:
		thrustRight = false;
		break;

	case 'a':
	case 'A':
		thrustLeft = false;
		break;

	case 'd':
	case 'D':
		thrustRight = false;
		break;

	case 'w':
	case 'W':
		thrustUp = false;
		break;

	case 's':
	case 'S':
		thrustDown = false;
		break;

	case 'q':
	case 'Q':
		rotateLeft = false;
		break;

	case 'e':
	case 'E':
		rotateRight = false;
		break;

	default:
		break;
	}
}


//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

	
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	// if end-screen is open, allow click on restart button only
	if (bGameOver) {
		if (restartButtonRect.inside((float)x, (float)y)) {
			resetGame();
		}
		return;
	}
	
	if (cam.getMouseInputEnabled()) return;

	if (bLanderLoaded) {
		glm::vec3 origin = cam.getPosition();
		glm::vec3 mouseWorld = cam.screenToWorld(glm::vec3(mouseX, mouseY, 0));
		glm::vec3 mouseDir = glm::normalize(mouseWorld - origin);

		ofVec3f min = lander.getSceneMin() + lander.getPosition();
		ofVec3f max = lander.getSceneMax() + lander.getPosition();

		Box bounds = Box(Vector3(min.x, min.y, min.z),
						 Vector3(max.x, max.y, max.z));
		bool hit = bounds.intersect(
			Ray(Vector3(origin.x, origin.y, origin.z),
				Vector3(mouseDir.x, mouseDir.y, mouseDir.z)),
			0, 10000);

		if (hit) {
			bLanderSelected = true;
			mouseDownPos = getMousePointOnPlane(lander.getPosition(), cam.getZAxis());
			mouseLastPos = mouseDownPos;
			bInDrag = true;

			lastDragDelta   = glm::vec3(0.0f);
		}
		else {
			bLanderSelected = false;
		}
	}
	else {
		ofVec3f p;
		raySelectWithOctree(p);
	}
}


bool ofApp::raySelectWithOctree(ofVec3f &pointRet) {
	ofVec3f mouse(mouseX, mouseY);
	ofVec3f rayPoint = cam.screenToWorld(mouse);
	ofVec3f rayDir = rayPoint - cam.getPosition();
	rayDir.normalize();

	Ray ray = Ray(
		Vector3(rayPoint.x, rayPoint.y, rayPoint.z),
		Vector3(rayDir.x,  rayDir.y,  rayDir.z)
	);

	uint64_t t1 = ofGetElapsedTimeMicros();
	pointSelected = octree.intersect(ray, octree.root, selectedNode);
	uint64_t t2 = ofGetElapsedTimeMicros();

	if (bTimingInfo) {
		double dtMs = (t2 - t1) / 1000.0;
		cout << "Time to Select: " << dtMs << " ms" << endl;
	}

	if (pointSelected) {
		pointRet = octree.mesh.getVertex(selectedNode.points[0]);
	}
	return pointSelected;
}



void ofApp::mouseDragged(int x, int y, int button) {

	// if moving camera, don't allow mouse interaction
	if (cam.getMouseInputEnabled()) return;

	// no dragging after we have landed or crashed
	if (gameState == STATE_LANDED_SAFE || gameState == STATE_CRASHED) {
		return;
	}

	if (bInDrag) {

		glm::vec3 landerPos = lander.getPosition();

		glm::vec3 mousePos = getMousePointOnPlane(landerPos, cam.getZAxis());
		glm::vec3 delta = mousePos - mouseLastPos;

		// remember last drag direction for resolution step
		lastDragDelta = delta;

		landerPos += delta;
		lander.setPosition(landerPos.x, landerPos.y, landerPos.z);
		mouseLastPos = mousePos;

		// recompute colliding octree boxes for the lander’s current AABB
		recomputeCollisionList();
	}
	else {
		ofVec3f p;
		raySelectWithOctree(p);
	}
}


//--------------------------------------------------------------
Box ofApp::currentLanderBounds() {
	ofVec3f min = lander.getSceneMin() + lander.getPosition();
	ofVec3f max = lander.getSceneMax() + lander.getPosition();
	return Box(Vector3(min.x, min.y, min.z), Vector3(max.x, max.y, max.z));
}

//--------------------------------------------------------------
void ofApp::recomputeCollisionList() {
	colBoxList.clear();
	Box bounds = currentLanderBounds();
	octree.intersect(bounds, octree.root, colBoxList);
}

//--------------------------------------------------------------
void ofApp::startCollisionResolve() {
	if (!bLanderLoaded) return;
	if (colBoxList.size() < 10) return;

	resolveDir = glm::vec3(0.0f, 1.0f, 0.0f);
	bResolvingCollision = true;
}

//--------------------------------------------------------------
void ofApp::stepCollisionResolve(float dt) {
	if (!bResolvingCollision) return;

	// move a small step opposite the collision path
	glm::vec3 p = lander.getPosition();
	p += resolveDir * resolveSpeed * dt;
	lander.setPosition(p.x, p.y, p.z);

	// re-check collisions after the move
	recomputeCollisionList();

	// stop once clear
	if (colBoxList.empty()) {
		bResolvingCollision = false;
	}
}


//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
	bInDrag = false;
	
	// when user stops dragging the lander, sync physics position
	if (bLanderLoaded) {
		glm::vec3 p = lander.getPosition();
		landerPos = p;
		landerVel = glm::vec3(0.0f);
	}
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}



//--------------------------------------------------------------
// setup basic ambient lighting in GL  (for now, enable just 1 light)
//
void ofApp::initLightingAndMaterials() {
	// Bright, hazy "sun" – high, slightly from the right
	keyLight.setup();
	keyLight.enable();
	keyLight.setDirectional();
	keyLight.setDiffuseColor( ofFloatColor(1.0f, 0.93f, 0.78f) );   // pale yellow-orange
	keyLight.setSpecularColor(ofFloatColor(1.0f, 0.98f, 0.90f));
	keyLight.lookAt(ofVec3f(-0.3f, -1.0f, -0.1f));                  // mostly top-down

	// Warm dusty sky fill – bright so shadows never go very dark
	fillLight.setup();
	fillLight.enable();
	fillLight.setPointLight();
	fillLight.setDiffuseColor( ofFloatColor(0.95f, 0.78f, 0.60f) ); // soft peach
	fillLight.setSpecularColor(ofFloatColor(0.40f, 0.30f, 0.22f));
	fillLight.setPosition(ofVec3f(0.0f, 1200.0f, 0.0f));
	fillLight.setAttenuation(1.0f, 0.0004f, 0.0000005f);

	// Very subtle cool rim to give shape without fighting the warm look
	rimLight.setup();
	rimLight.enable();
	rimLight.setDirectional();
	rimLight.setDiffuseColor( ofFloatColor(0.55f, 0.60f, 0.75f) );
	rimLight.setSpecularColor(ofFloatColor(0.80f, 0.85f, 1.00f));
	rimLight.lookAt(ofVec3f(0.0f, -0.2f, 1.0f));  // from behind the camera a bit

	// Global lighting: bright warm ambient, two-sided so steep slopes don’t go black
	static float ambient[]        = { 0.55f, 0.40f, 0.30f, 1.0f };  // strong warm ambient
	static float diffuse[]        = { 1.0f,  1.0f,  1.0f,  1.0f };
	static float position[]       = { 5.0f,  5.0f,  5.0f,  0.0f };
	static float lmodel_ambient[] = { 0.55f, 0.40f, 0.30f, 1.0f };
	static float lmodel_twoside[] = { GL_TRUE };

	glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position);

	glLightfv(GL_LIGHT1, GL_AMBIENT,  ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE,  diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, position);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT,  lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	// glEnable(GL_LIGHT1);
	glShadeModel(GL_SMOOTH);
}



void ofApp::savePicture() {
	ofImage picture;
	picture.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	picture.save("screenshot.png");
	cout << "picture saved" << endl;
}

//--------------------------------------------------------------
//
// support drag-and-drop of model (.obj) file loading.  when
// model is dropped in viewport, place origin under cursor
//
void ofApp::dragEvent2(ofDragInfo dragInfo) {

	ofVec3f point;
	mouseIntersectPlane(ofVec3f(0, 0, 0), cam.getZAxis(), point);
	if (lander.load(dragInfo.files[0])) {
		lander.setScaleNormalization(false);
		lander.setPosition(1, 1, 0);

		bLanderLoaded = true;
		bboxList.clear();
		for (int i = 0; i < lander.getMeshCount(); i++) {
			bboxList.push_back(Octree::meshBounds(lander.getMesh(i)));
		}

		// sync physics state with lander
		landerPos    = lander.getPosition();
		landerVel    = glm::vec3(0.0f);
		landerAngleY = 0.0f;
		angularVelY  = 0.0f;

		cout << "Mesh Count: " << lander.getMeshCount() << endl;
	}
	else cout << "Error: Can't load model" << dragInfo.files[0] << endl;
}


bool ofApp::mouseIntersectPlane(ofVec3f planePoint, ofVec3f planeNorm, ofVec3f &point) {
	ofVec2f mouse(mouseX, mouseY);
	ofVec3f rayPoint = cam.screenToWorld(glm::vec3(mouseX, mouseY, 0));
	ofVec3f rayDir = rayPoint - cam.getPosition();
	rayDir.normalize();
	return (rayIntersectPlane(rayPoint, rayDir, planePoint, planeNorm, point));
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
	if (lander.load(dragInfo.files[0])) {
		bLanderLoaded = true;
		lander.setScaleNormalization(false);
		lander.setPosition(0, 0, 0);
		cout << "number of meshes: " << lander.getNumMeshes() << endl;
		bboxList.clear();
		for (int i = 0; i < lander.getMeshCount(); i++) {
			bboxList.push_back(Octree::meshBounds(lander.getMesh(i)));
		}

		glm::vec3 origin = cam.getPosition();
		glm::vec3 camAxis = cam.getZAxis();
		glm::vec3 mouseWorld = cam.screenToWorld(glm::vec3(mouseX, mouseY, 0));
		glm::vec3 mouseDir = glm::normalize(mouseWorld - origin);
		float distance;

		bool hit = glm::intersectRayPlane(origin, mouseDir, glm::vec3(0, 0, 0), camAxis, distance);
		if (hit) {
			glm::vec3 intersectPoint = origin + distance * mouseDir;

			glm::vec3 min = lander.getSceneMin();
			glm::vec3 max = lander.getSceneMax();
			float offset = (max.y - min.y) / 2.0;
			lander.setPosition(intersectPoint.x, intersectPoint.y - offset, intersectPoint.z);

			landerBounds = Box(Vector3(min.x, min.y, min.z), Vector3(max.x, max.y, max.z));

			// sync physics state with lander
			landerPos    = lander.getPosition();
			landerVel    = glm::vec3(0.0f);
			landerAngleY = 0.0f;
			angularVelY  = 0.0f;
		}
	}
}


//  intersect the mouse ray with the plane normal to the camera
//  return intersection point.   (package code above into function)
//
glm::vec3 ofApp::getMousePointOnPlane(glm::vec3 planePt, glm::vec3 planeNorm) {
	// Setup our rays
	//
	glm::vec3 origin = cam.getPosition();
	glm::vec3 camAxis = cam.getZAxis();
	glm::vec3 mouseWorld = cam.screenToWorld(glm::vec3(mouseX, mouseY, 0));
	glm::vec3 mouseDir = glm::normalize(mouseWorld - origin);
	float distance;

	bool hit = glm::intersectRayPlane(origin, mouseDir, planePt, planeNorm, distance);

	if (hit) {
		// find the point of intersection on the plane using the distance
		// We use the parameteric line or vector representation of a line to compute
		//
		// p' = p + s * dir;
		//
		glm::vec3 intersectPoint = origin + distance * mouseDir;

		return intersectPoint;
	}
	else return glm::vec3(0, 0, 0);
}

void ofApp::updateLight(ofLight & light,
	float xzDeg,
	float yDeg,
	float angleOffset,
	float scale) {

	float radius = 5.0;

	float xzRad = ofDegToRad(xzDeg);
	float x = radius * cos(xzRad);
	float z = radius * sin(xzRad);

	float yRad = ofDegToRad(yDeg);
	float y = radius * sin(yRad);

	light.setPosition(x, y, z);
	light.setOrientation(ofQuaternion(0, 0, 0, 1));
	ofVec3f toCenter = -light.getPosition();
	toCenter.normalize();

	ofVec3f up(0, 1, 0);
	ofQuaternion orientation;
	orientation.makeRotate(ofVec3f(0, 0, -1), toCenter);
	light.setOrientation(orientation);
	light.tilt(angleOffset);

	light.setScale(scale);
}

// Camera Helper Methods
void ofApp::camLookAt(ofCamera & cam, const ofVec3f & target) {
	cam.lookAt(target);
}

void ofApp::buildOctreeFromAssimp(ofxAssimpModelLoader& model, Octree& octree, int depth) {
	ofMesh combinedMesh;

	for (int i = 0; i < model.getMeshCount(); i++) {
		combinedMesh.append(model.getMesh(i));
	}

	cout << "Combined mesh vertex count: "
		<< combinedMesh.getNumVertices() << endl;

	octree.create(combinedMesh, depth);
}
