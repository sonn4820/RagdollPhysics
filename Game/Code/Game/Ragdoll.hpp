#pragma once
#include "Game/GameCommon.hpp"
#include "Game/GameObject.hpp"

/// <summary>
/// 
///	Notes:
///  1. In this ragdoll code, we treat all inertia as 1,1,1
/// 
/// TODO:
/// 1. Remove energy from the system by remove unimportant nodes (like upper arm)
/// 2. Hammer down the error fixing. Stop at the good amount of interation
/// 3. Install function's time measurement
/// 
/// 
/// </summary>

constexpr double restitution = 0.5;
constexpr double friction = 0.5;
constexpr double restingSpeed = 0.5;
constexpr double springStiffness = 50.0;
constexpr double damping = 5.0;
constexpr double deltaImpulseLimit = 10;


struct Object;
struct Node;
struct Constraint;
class Ragdoll;

struct VerletConfig
{
	DoubleVec3 gravAccel = DoubleVec3(0, 0, -9.81) * 10;
	int iterations = 25;
	double airFriction = 0.4;
};

struct RaycastRagdollResult3D : public RaycastResult3D
{
	Node* m_hitNode = nullptr;
	Constraint* m_hitConstraint = nullptr;
};

struct NodeCollisionPoint
{
	DoubleVec3 position;
	double penetration = 0.f;
	DoubleVec3 normalA;
	DoubleVec3 normalB;
	Node* nodeA = nullptr;
	Node* nodeB = nullptr;
};

struct NodeCollisionSolver
{
public:
	// Configuration parameters
	double m_restitution = restitution;			// Coefficient of restitution
	double m_friction = friction;				// Coefficient of friction
	double m_restingSpeed = restingSpeed;		// Speed threshold for resting contact
	double m_springStiffness = springStiffness;	// For resting contact resolution
	double m_damping = damping;					// Damping coefficient for resting contacts

	void ResolveCollision(NodeCollisionPoint& col);
	void ResolveRestingContact(const NodeCollisionPoint& col, const DoubleVec3& rA, const DoubleVec3& rB);
	void ResolveFriction(const NodeCollisionPoint& col, const DoubleVec3& rA, const DoubleVec3& rB, double normalImpulse);
	void ApplyImpulseCollision(Node* bodyA, Node* bodyB, const DoubleVec3& impulse, const DoubleVec3& rA, const DoubleVec3& rB);
};

struct VelocityLessState
{
	VelocityLessState() = default;
	VelocityLessState(DoubleQuaternion q, DoubleQuaternion q_prev) : q(q), q_prev(q_prev) {};
	DoubleQuaternion q;
	DoubleQuaternion q_prev;
};

struct VelocityState
{
	VelocityState() = default;
	VelocityState(DoubleQuaternion q, DoubleVec3 omega) : q(q), omega(omega) {};
	DoubleQuaternion q;
	DoubleVec3 omega;
};

struct Node : public GameObject
{
	Node(Game* game, std::string name = "", double radius = 0.5, Node* parent = nullptr, double mass = 1);
	~Node() = default;

	std::string m_name;
	Node* m_parent = nullptr;
	Ragdoll* m_ragdoll = nullptr;

	double m_radius = 0.25;

	bool m_isSphere = true;

	DoubleVec3 m_offsetToParent = DoubleVec3::ZERO;

public:

	void Render() const override;

	bool IsSphere() const;

	virtual double GetHalfLength() const = 0;
	virtual DoubleVec3 GetAxis() const = 0;
	virtual DoubleVec3 GetPointOnBody(DoubleVec3 const& distanceVector) const = 0;
	virtual bool NodeOverlapFixedAABB_Double(DoubleAABB3 const& aabb) = 0;
	virtual bool NodeOverlapFixedOBB_Double(DoubleOBB3 const& obb) = 0;
	virtual bool NodeOverlapFixedSphere_Double(DoubleVec3 const& center, double radius) = 0;
	virtual bool NodeOverlapFixedCapsule_Double(DoubleCapsule3 const& capsule) = 0;

	DoubleVec3 GetFurthestPointPenetrated(DoubleVec3 collisionPoint);
};

struct SphereNode : public Node
{
	SphereNode(Game* game, std::string name = "", double radius = 0.5, Node* parent = nullptr, double mass = 1, Rgba8 debugColor = Rgba8::COLOR_WHITE);
	~SphereNode() = default;
	double GetHalfLength() const override;
	DoubleVec3 GetAxis() const override;
	DoubleVec3 GetPointOnBody(DoubleVec3 const& distanceVector) const override;
	bool NodeOverlapFixedAABB_Double(DoubleAABB3 const& aabb) override;
	bool NodeOverlapFixedOBB_Double(DoubleOBB3 const& obb) override;
	bool NodeOverlapFixedSphere_Double(DoubleVec3 const& center, double radius) override;
	bool NodeOverlapFixedCapsule_Double(DoubleCapsule3 const& capsule) override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleAABB3 GetBoundingBox() const override;
};


struct CapsuleNode : public Node
{
	DoubleVec3 m_capsuleAxis;
	double m_capsuleHalfAxisLength;

	CapsuleNode(Game* game, std::string name = "", double radius = 0.5, DoubleVec3 axis = DoubleVec3(), double halfLength = 0, Node* parent = nullptr, double mass = 1, Rgba8 debugColor = Rgba8::COLOR_WHITE);
	~CapsuleNode() = default;
	double GetHalfLength() const override;
	DoubleVec3 GetAxis() const override;
	DoubleVec3 GetPointOnBody(DoubleVec3 const& distanceVector) const override;
	bool NodeOverlapFixedAABB_Double(DoubleAABB3 const& aabb) override;
	bool NodeOverlapFixedOBB_Double(DoubleOBB3 const& obb) override;
	bool NodeOverlapFixedSphere_Double(DoubleVec3 const& center, double radius) override;
	bool NodeOverlapFixedCapsule_Double(DoubleCapsule3 const& capsule) override;
	bool CollisionResolveVsRagdollNode(Node* node) override;

	DoubleAABB3 GetBoundingBox() const override;
};


struct Constraint
{
	Constraint(Game* game, Node* nA, Node* nB, const DoubleVec3& pinA, const DoubleVec3& pinB, const double targetDistance, const DoubleVec3& minAngle, const DoubleVec3& maxAngle, Rgba8 color = Rgba8::COLOR_RAGDOLL_CONSTRAINT);
	~Constraint();

	Game* m_game = nullptr;
	Ragdoll* m_ragdoll = nullptr;
	Node* nA = nullptr;
	Node* nB = nullptr;
	DoubleVec3 pinA;
	DoubleVec3 pinB;

	DoubleVec3 m_rPinA;
	DoubleVec3 m_rPinB;
	double m_targetDistance;

	DoubleVec3 m_minAngle;
	DoubleVec3 m_maxAngle;

	std::vector<Vertex_PCUTBN> m_vertexes;
	std::vector<unsigned int> m_indexes;
	VertexBuffer* m_vbuffer = nullptr;
	IndexBuffer* m_ibuffer = nullptr;

	virtual bool SolveDistanceAndVelocity(double fixRate = 20);
	virtual bool SolveAngle(double fixRate = 5);

	DoubleVec3 GetWorldPinA() const;
	DoubleVec3 GetWorldPinB() const;

	std::string GetName() const;

	void Render() const;
};

class Ragdoll
{
public:

	Ragdoll(Game* game, DoubleMat44 transform, float deadTimer = 5.f, VerletConfig config = VerletConfig(), int debugType = 0, Rgba8 nodeColor = Rgba8::COLOR_RAGDOLL_NODE, Rgba8 consraintColor = Rgba8::COLOR_RAGDOLL_CONSTRAINT);
	~Ragdoll();

	DoubleMat44 GetRootTransform() const;

	void Render() const;
	void Update(float deltaTime);
	void FixedUpdate(float fixedTime);

	// VERLET VELOCITY INTEGRATION
	void IntegratePosition_VelocityVerlet(float deltaTime, double friction);
	void IntegrateRotation_VelocityVerlet(float deltaTime);

	void AccumulateForce(std::string name, DoubleVec3 force);
	void ApplyGlobalAcceleration(DoubleVec3 accel);
	void ApplyGlobalImpulse(DoubleVec3 impulse);
	void ApplyGlobalImpulseOnRoot(DoubleVec3 impulse);
	void ApplyConstraints(int interation);
	void GetBoundingSphere(DoubleVec3& out_Center, double& out_radius);

	double GetAverageSpeedNodes() const;

	void BreakNodeFromRagdoll(Node* n);


	std::vector<Node*> GetNodeList() const;
	std::vector<Constraint*> GetConstraints() const;

	Node* GetNode(std::string name);
	Node* GetNode(int nodeIndex);
	std::string GetNodeName(int index) const;
	std::string GetConstraintName(int index) const;

	Game* m_game = nullptr;

	VerletConfig m_config;

	DoubleMat44 m_transform;

	Rgba8 m_nodeColor = Rgba8::COLOR_RAGDOLL_NODE;
	Rgba8 m_constraintColor = Rgba8::COLOR_RAGDOLL_CONSTRAINT;

	bool m_isBreakable = false;
	int m_brokenLimit = 0;
	int m_brokenCount = 0;

	bool m_isDead = false;
	bool m_isActive = true;
	double m_totalEnergy = 0.f;
	float m_deadTimer = 5.f;

	double DEBUG_maxVelocity = 200;
	double DEBUG_posFixRate = 20;
	double DEBUG_angleFixRate = 5;
private:
	void CheckShouldItBeDead();

	void CreateSphereNode();
	void CreateCapsuleNode();
	void CreateTPose_CapsulesAndSpheres();
	void CreateDebugNodes();

	Node* CreateRootNode(std::string name, DoubleVec3 offsetPositionToParent, double radius, double mass = 1.0);
	Node* CreateSphereNode(std::string name, DoubleVec3 offsetPositionToParent, double radius, std::string parentName, double mass = 1.0);
	Node* CreateCapsuleNode(std::string name, DoubleVec3 offsetPositionToParent, double radius, DoubleVec3 axis, double halfLength, std::string parentName, double mass = 1.0);

	Constraint* CreateConstraint(Node* n1 /* parent */, Node* n2 /* child */, const DoubleVec3& pinA, const DoubleVec3& pinB, const double targetDistance, const DoubleVec3& minAngle, const DoubleVec3& maxAngle);

private:
	std::vector<Node*> m_nodes;
	std::vector<Constraint*> m_constraints;

};


void PushRagdollOutOfDefaultPlane3D_Double(Ragdoll* ragdoll);
void PushRagdollOutOfAABB3D_Double(Ragdoll* ragdoll, DoubleAABB3& aabb);
void PushRagdollOutOfOBB3D_Double(Ragdoll* ragdoll, DoubleOBB3& obb);
void PushRagdollOutOfSphere3D_Double(Ragdoll* ragdoll, DoubleVec3& center, double radius);
void PushRagdollOutOfCapsule3D_Double(Ragdoll* ragdoll, DoubleCapsule3& capsule);
RaycastRagdollResult3D MouseRaycastVsRagdollNode(Camera* camera, Vec2 cursorPosition, Ragdoll* ragdoll);


VelocityLessState StepVelocityLess(float timestep, const VelocityLessState& state, const DoubleVec3& torque, double angular_rate_damping = 1.0);
VelocityState StepWithVelocity(float timestep, const VelocityState& state, const DoubleVec3& torque, Vec3 invInertia, double angular_rate_damping = 1.0);
DoubleQuaternion QuaternionDerivative(const DoubleQuaternion& q, const DoubleVec3& omega);
DoubleQuaternion QuaternionSecondDerivative(const DoubleQuaternion& q, const DoubleVec3& omega, const DoubleVec3& torque);


double GetTotalEnergy(DoubleVec3 velocity, double mass, DoubleVec3 gravity, double height);