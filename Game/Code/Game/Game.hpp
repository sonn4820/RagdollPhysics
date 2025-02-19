#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Ragdoll.hpp"
#include "Game/GameObject.hpp"
#include "Game/Octree.hpp"

class Player;
class Ragdoll;

struct Object_AABB : public GameObject
{
	Object_AABB(Game* game, DoubleAABB3 aabb);
	~Object_AABB() = default;

	void Render() const override;
	DoubleAABB3 GetBoundingBox() const override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleAABB3 m_aabb;
};

struct Object_OBB : public GameObject
{
	Object_OBB(Game* game, DoubleOBB3 obb);
	~Object_OBB() = default;

	void Render() const override;
	DoubleAABB3 GetBoundingBox() const override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleOBB3 m_obb;
};

struct Object_Sphere : public GameObject
{
	Object_Sphere(Game* game, DoubleVec3 center, double radius);
	~Object_Sphere() = default;

	void Render() const override;
	DoubleAABB3 GetBoundingBox() const override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleVec3 m_center;
	double m_radius = 1;
};

struct Object_Capsule : public GameObject
{
	Object_Capsule(Game* game, DoubleCapsule3 capsule);
	~Object_Capsule() = default;

	void Render() const override;
	DoubleAABB3 GetBoundingBox() const override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleCapsule3 m_capsule;
};

enum class GameState
{
	ATTRACT_MODE,
	FEATURE_MODE,
	PACHINKO_MODE,
	CANNON_MODE,
};

class Game
{
public:
	Game();
	~Game();

	void Startup();
	void Update(float deltaSeconds);
	void Render() const;
	void Shutdown();
	void Restart();

	void Init_Ragdolls();
	void Init_Octree();

	//void Raycast_Update();

	void IMGUI_UPDATE();

	// STATE
	void SwitchState(GameState state);

	// GAME RESTART
	void FeatureModeRestart();
	void PachinkoModeRestart();
	void CannonModeRestart();

	void SpawnRagdoll(Mat44 transform, Rgba8 color = Rgba8::COLOR_RAGDOLL_NODE, float deadTimer = 5.f, bool breakable = false, Vec3 initialVelocity = Vec3(), bool active = true);
	void DeleteRagdoll(Ragdoll* r);

	// UI
	void Menu_Init();

public:
	Camera* m_screenCamera;
	GameState m_currentState = GameState::ATTRACT_MODE;
	GameState m_previousState = GameState::ATTRACT_MODE;

	std::vector<GameObject*> m_allObjects;
	Octree* m_octree = nullptr;

	Player* m_player = nullptr;
	Clock* m_clock = nullptr;
	EulerAngles m_sunOrientation = EulerAngles(110, 60, 0);
	float m_sunIntensity = 1.f;
	float m_ambIntensity = 0.f;

	Shader* m_shader = nullptr;
	Shader* m_diffuseShader = nullptr;
	Texture* m_planeTextureN = nullptr;
	Texture* m_planeTextureS = nullptr;
	Texture* m_planeTextureD = nullptr;
	Texture* m_stoneTextureN = nullptr;
	Texture* m_stoneTextureS = nullptr;
	Texture* m_stoneTextureD = nullptr;
	Texture* m_transparentTexture = nullptr;
	Texture* m_skyboxTexture = nullptr;
	Texture* m_reticleTexture = nullptr;

	std::vector<GameObject*> m_fixedObjects;

	DoublePlane3 plane;

	// M2
	std::vector<Ragdoll*> m_ragdolls;

	// RAGDOLL DEBUG
	double DEBUG_NodeMoveSpeed = 20000;
	float DEBUG_spawn_X = 0.f;
	float DEBUG_spawn_Y = 0.f;
	float DEBUG_spawn_Z = 15.f;
	float DEBUG_spawn_Yaw = 0.f;
	float DEBUG_spawn_Pitch = 0.f;
	float DEBUG_spawn_Roll = 0.f;
	bool DEBUG_randomRotation = false;
	bool DEBUG_breakable = false;
	float DEBUG_ragdoll_deadTimer = 5.f;
	bool DEBUG_allRagdollLiveForever = false;
	FloatRange DEBUG_random_spawn_X = FloatRange(-40.f, 40.f);
	FloatRange DEBUG_random_spawn_Y = FloatRange(-40.f, 40.f);
	FloatRange DEBUG_random_spawn_Z = FloatRange(20.f, 40.f);

	// GAME DEBUG
	bool DEBUG_demoMode = false;
	unsigned int DEBUG_currentSeed = 0;
	int DEBUG_desireSeed = -1;
	bool DEBUG_isCameraMode = false;
	bool DEBUG_drawDebug = false;
	float DEBUG_previousDeltaSeconds = 0.0f;


	// UI
	Canvas* m_menuCanvas = nullptr;
	Text* m_title1 = nullptr;
	Text* m_title2 = nullptr;
	Button* m_featureMode = nullptr;
	Button* m_pachinkoMode = nullptr;
	Button* m_cannonMode = nullptr;

	// Pachinko Mode
	Vec3 m_pachinkoCursor = Vec3(-15, 0, 80);
	Vec3 m_demoPachinkoCursorTargetPosition = Vec3(-15, 0, 80);
	Timer* m_demoPachinkoTimer = nullptr;
	bool m_reachDestination = true;

	// Canon Mode
	float m_cannonForce = 0.f;
private:
	// VARIABLES
	int m_ragdollDebugType = 0;
	int m_numberOfRagdolls = 1;
	float m_timeDebt = 0.f;
	float m_fixedTimeStep = (float)TIME_STEP;
	float m_secondIntoMode = 0.f;

	std::vector<Vertex_PCUTBN> m_planeVerts;
	std::vector<unsigned int> m_planeIndexes;
	VertexBuffer* m_planeVBO;
	IndexBuffer* m_planeIBO;

	// UPDATE
	void UpdateFeatureMode(float deltaSeconds);
	void UpdatePachinkoMode(float deltaSeconds);
	void UpdateCannonMode(float deltaSeconds);
	void UpdateAttractMode(float deltaSeconds);
	void HandleInput();

	// RENDER
	void RenderFeatureMode() const;
	void RenderPachinkoMode() const;
	void RenderCannonMode() const;
	void RenderAttractMode() const;
	void RenderScreenWorld() const;
	void DrawSkybox() const;
	void DrawGrid() const;
};