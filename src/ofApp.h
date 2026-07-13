#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include  "ofxAssimpModelLoader.h"
#include "Octree.h"
#include <vector>
#include "Particle.h"
#include "ParticleEmitter.h"

// -------------------------------------------------------
// Single global GameState enum used everywhere
// -------------------------------------------------------
enum GameState {
	STATE_READY,
	STATE_PLAYING,
	STATE_LANDED_SAFE,
	STATE_CRASHED,
	STATE_OUT_OF_FUEL
};

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent2(ofDragInfo dragInfo);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
		void drawAxis(ofVec3f);
		void initLightingAndMaterials();
		void savePicture();
		void toggleWireframeMode();
		void togglePointsDisplay();
		void toggleSelectTerrain();
		bool mouseIntersectPlane(ofVec3f planePoint, ofVec3f planeNorm, ofVec3f &point);
		bool raySelectWithOctree(ofVec3f &pointRet);
		glm::vec3 getMousePointOnPlane(glm::vec3 p , glm::vec3 n);

		ofEasyCam cam;
		ofCamera * activeCam = &cam;
		ofCamera onboard, tracking;
		int currentCam = 2;
		// CAM = 2 | EasyCam
		// CAM = 3 | Tracking
		// CAM = 4 | Onboard
		void camLookAt(ofCamera & cam, const ofVec3f & target);

		ofxAssimpModelLoader mars, lander;
		void buildOctreeFromAssimp(ofxAssimpModelLoader& model, Octree& octree, int depth);
		ofLight light;
		ofLight keyLight, fillLight, rimLight, shipLight;
		void updateLight(ofLight & light, float xzDeg, float yDeg, float angleOffset, float scale);
		Box boundingBox, landerBounds;
		Box testBox;
		vector<Box> colBoxList;
		bool bLanderSelected = false;
		Octree octree;
		TreeNode selectedNode;
		glm::vec3 mouseDownPos, mouseLastPos;
		bool bInDrag = false;


		ofxIntSlider numLevels;
		ofxPanel gui;
	
		void drawHUD();
		bool bShowHelpHud = true;
		std::string gameStateToString(GameState s);


		bool bAltKeyDown;
		bool bCtrlKeyDown;
		bool bWireframe;
		bool bDisplayPoints;
		bool bPointSelected;
		bool bHide;
		bool pointSelected = false;
		bool bDisplayLeafNodes = false;
		bool bDisplayOctree = false;
		bool bDisplayBBoxes = false;
		
		bool bLanderLoaded;
		bool bTerrainSelected;
		ofxToggle bTimingInfo;
	
		ofVec3f selectedPoint;
		ofVec3f intersectPoint;

		vector<Box> bboxList;

		const float selectionRange = 4.0;
	
		bool bResolvingCollision = false;
		float resolveSpeed = 2.0f;
		glm::vec3 resolveDir = glm::vec3(0);
		glm::vec3 lastDragDelta = glm::vec3(0);

		void startCollisionResolve();
		void stepCollisionResolve(float dt);
		void recomputeCollisionList();
		Box currentLanderBounds();
	
	
		void updatePhysics(float dt);
		void updateAltitudeSensor();
		void loadDefaultLander();
		
		// physics toggle (if we ever want to turn physics off)
		bool bUsePhysics = true;

		// lander physics state (world space)
		glm::vec3 landerPos = glm::vec3(0);      	// current position of lander
		glm::vec3 landerVel = glm::vec3(0);      	// current velocity
		glm::vec3 accumulatedForces = glm::vec3(0); // forces accumulated this frame

		// yaw (rotation about Y axis) and angular velocity in degrees/sec
		float landerAngleY = 0.0f;
		float angularVelY = 0.0f;

		// physics constants  (arcade-fast)
		float landerMass       = 1.0f;

		// thrusts (engine power)
		float mainThrust       = 120.0f;   // forward/back
		float sideThrust       = 90.0f;    // strafe
		float verticalThrust   = 110.0f;   // up/down

		// gravity (MUST be negative to pull down)
		float gravityAccel     = -6.0f;    // strong pull; increase magnitude for heavier

		// damping (air resistance)
		float linearDamping    = 0.97f;    // close to 1.0 so it keeps high speed
		float angularDamping   = 0.92f;    // yaw slows reasonably fast

		// speed caps
		float maxHorizontalSpeed = 120.0f;  // X/Z limit (very fast)
		float maxVerticalSpeed   = 80.0f;   // Y limit (fast up/down)
		float maxAngularSpeed    = 160.0f;  // deg/sec yaw limit (snappy turning)


		// fuel system (2 minutes = 120 seconds)
		float fuelMaxSeconds = 120.0f;
		float fuelRemaining  = 120.0f;
		bool  bOutOfFuel     = false;

		// thruster control flags (set from keyPressed / keyReleased)
		// Arrow keys + A/D/W/S now all directly thrust; no rotation controls.
		bool thrustForward = false;
		bool thrustBackward = false;
		bool thrustLeft = false;
		bool thrustRight = false;
		bool thrustUp = false;
		bool thrustDown = false;
		bool rotateLeft = false;
		bool rotateRight = false;

		float currentAltitude = 0.0f;  // altitude above ground
		bool bAltitudeSensorOn = true; // can be toggled with a hotkey later

		// simple game state (uses global GameState enum above)
		GameState gameState = STATE_READY;

		// contact / landing logic

		bool  bOnGround       = false;
		bool  bHardLanding    = false;   // true if last impact was "hard" (non-crash)
		float lastImpactSpeed = 0.0f;

		// game progress & scoring:
		//  - score = number of distinct landing zones visited (0..3)
		int score = 0;
		std::string lastLandingSummary = "None";

		// landing speed thresholds (more forgiving)
		float safeLandingSpeed  = 3.0f;   // was 2.0
		float crashLandingSpeed = 15.0f;   // was 5.0

		void updateCollisionAndLanding(float dt);

		// turbulence parameters (lower for easier control)
		bool  bEnableTurbulence   = true;
		float turbulenceStrength  = 0.15f;  // was 0.3
		float turbulenceFreq      = 0.4f;   // was 0.5
		float turbulenceTime      = 0.0f;

		// landing zone support
		std::vector<Box>  landingZones;
		int               lastLandingZoneIndex = -1;  // last zone we were inside, or -1
		std::vector<bool> landingZoneVisited;        // true per zone if already counted

		// game-over / restart UI
		bool        bGameOver        = false;
		bool        bPlayerWon       = false;
		std::string gameOverMessage  = "";
		ofRectangle restartButtonRect;

		// helpers
		void initLandingZones();
		bool isInsideLandingZone(const glm::vec3 &pos, int &zoneIndex);
		void resetGame();

		// particle logic
		ParticleEmitter emitter;
		ParticleEmitter radialEmitter;
		TurbulenceForce * turbForce;
		GravityForce * gravityForce;
		ImpulseRadialForce * radialForce;

		bool emitterActive = false;
		float particleGravity = 10;
		float particleDamping = 0.99;
		float particleRadius = 0.05;
		ofVec3f particleVelocity = ofVec3f(0, 0, 0);
		float particleLifespan = 3.0;
		float particleRate = 1.0;
		ofVec3f particleTurbMin = ofVec3f(0, 0, 0);
		ofVec3f particleTurbMax = ofVec3f(0 ,0, 0);
		float particleRadialForceVal = 1000;

		void loadVbo();
		ofTexture particleTex;
		ofVbo vbo;
		ofShader shader;
		int vboCount = 0;
	
		ofBoxPrimitive skybox;
		ofImage skyTex;
	
		// audio
		ofSoundPlayer thrustSound;
		ofSoundPlayer explosionSound;
		ofSoundPlayer ambientSound;
		ofSoundPlayer successSound;
		bool bThrustLoopPlaying = false;
		bool bAmbientPlaying    = false;

		// EasyCam follow mode
		bool bEasyCamFollowShip = false;
};
