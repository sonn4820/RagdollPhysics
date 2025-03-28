#include "GameObject.hpp"
#include "Game/Octree.hpp"
#include "Game/Game.hpp"

GameObject::GameObject(Game* game)
	:m_game(game)
{

}

GameObject::~GameObject()
{
	delete m_vbuffer;
	delete m_ibuffer;
	delete m_debugbuffer;
}

CollisionRecord* GameObject::Node_Intersect(GameObject* obj)
{
	if (!m_isNode && !obj->m_isNode)
	{
		return nullptr;
	}
	if (!m_isNode && obj->m_isNode)
	{
		return obj->Node_Intersect(this);
	}
	if (DoAABBsOverlap3D_Double(GetBoundingBox(), obj->GetBoundingBox()))
	{
		return new CollisionRecord((Node*)this, obj);
	}

	return nullptr;
}

void GameObject::CreateBuffer(Renderer* renderer)
{
	m_vbuffer = renderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN) * (unsigned int)m_vertexes.size());
	m_ibuffer = renderer->CreateIndexBuffer(sizeof(unsigned int) * (unsigned int)m_indexes.size());
	renderer->CopyCPUToGPU(m_vertexes.data(), (int)(m_vertexes.size() * sizeof(Vertex_PCUTBN)), m_vbuffer);
	renderer->CopyCPUToGPU(m_indexes.data(), (int)(m_indexes.size() * sizeof(unsigned int)), m_ibuffer);
}

DoubleMat44 GameObject::GetModelMatrix() const
{
	return m_orientation.GetConjugated().GetMatrix(m_position);
}

DoubleVec3 GameObject::GetAcceleration() const
{
	return m_netForce * m_invMass;
}

DoubleVec3 GameObject::GetInverseInertiaTensor() const
{
	return { 1, 1, 1 };
}

void GameObject::AccumulateForce(DoubleVec3 force)
{
	if (m_isResting && force.GetLength() > FORCE_EXIT_REST_THRESHOLD)
	{
		m_isResting = false;
	}
	m_netForce += force;
}

void GameObject::AccumulateAngularForce(DoubleVec3 torque)
{
	if (m_isResting && torque.GetLength() > FORCE_EXIT_REST_THRESHOLD)
	{
		m_isResting = false;
	}
	m_torque += torque;
}

void GameObject::ApplyForceAtPoint(DoubleVec3 force, DoubleVec3 point)
{
	AccumulateForce(force);
	DoubleVec3 arm = point - m_position;
	AccumulateAngularForce(arm.Cross(force));
}

void GameObject::AccumulateImpulse(DoubleVec3 impulse)
{
	//if (impulse.GetLength() > restingSpeed)
	//{
	//	m_isResting = false;
	//}
	//else
	//{
	//	return;
	//}
	m_velocity += impulse;
}

void GameObject::AccumulateAngularImpulse(DoubleVec3 impulse, DoubleVec3 r)
{
	//if (impulse.GetLength() > restingSpeed)
	//{
	//	m_isResting = false;
	//}
	//else
	//{
	//	return;
	//}
	m_angularVelocity += GetInverseInertiaTensor() * r.Cross(impulse);
}

