#pragma once
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/Renderer.hpp"

constexpr float FORCE_EXIT_REST_THRESHOLD = 500.f;
constexpr double VELOCITY_EXIT_REST_THRESHOLD = 5;
constexpr double ENERGY_EXIT_REST_THRESHOLD = 100;

class Game;
struct Node;
struct CollisionRecord;
struct Octree;

class GameObject
{
public:
	GameObject(Game* game);
	virtual ~GameObject();

	virtual void Render() const = 0;
	virtual DoubleAABB3 GetBoundingBox() const = 0;

	virtual bool CollisionResolveVsRagdollNode(Node* node) = 0;

	CollisionRecord* Node_Intersect(GameObject* obj);
	void CreateBuffer(Renderer* renderer);

	DoubleMat44 GetModelMatrix() const;
	DoubleVec3 GetAcceleration() const;
	DoubleVec3 GetInverseInertiaTensor() const;

	void AccumulateForce(DoubleVec3 force);
	void AccumulateAngularForce(DoubleVec3 torque);
	void ApplyForceAtPoint(DoubleVec3 force, DoubleVec3 point);
	void AccumulateImpulse(DoubleVec3 impulse);
	void AccumulateAngularImpulse(DoubleVec3 impulse, DoubleVec3 r);

public:
	Game* m_game = nullptr;

	bool m_isResting = false;
	DoubleVec3 m_position = DoubleVec3::ZERO;
	DoubleVec3 m_velocity = DoubleVec3::ZERO; 
	DoubleVec3 m_acceleration = DoubleVec3::ZERO;
	DoubleQuaternion m_orientation = DoubleQuaternion(0, 0, 1, 0);
	DoubleVec3 m_angularVelocity = DoubleVec3::ZERO;
	DoubleVec3 m_torque = DoubleVec3::ZERO;
	DoubleVec3 m_netForce = DoubleVec3::ZERO;

	double m_mass = 1.0;
	double m_invMass = 1.0;

	std::vector<Vertex_PCUTBN> m_vertexes;
	std::vector<unsigned int> m_indexes;
	VertexBuffer* m_vbuffer = nullptr;
	IndexBuffer* m_ibuffer = nullptr;

	std::vector<Vertex_PCU> m_debugvertexes;
	VertexBuffer* m_debugbuffer = nullptr;

	Texture* m_textureD = nullptr;
	Texture* m_textureN = nullptr;
	Texture* m_textureS = nullptr;

	Rgba8 m_color = Rgba8::COLOR_WHITE;

	bool m_isFixed = false;
	bool m_isNode = false;

	Octree* m_currentOctree = nullptr;
};

