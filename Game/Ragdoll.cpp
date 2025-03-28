#include "Game/Ragdoll.hpp"
#include "Game/Game.hpp"

Ragdoll::Ragdoll(Game* game, DoubleMat44 transform, float deadTimer, VerletConfig config, int debugType, Rgba8 nodeColor, Rgba8 constraintColor)
	:m_game(game), m_transform(transform), m_deadTimer(deadTimer), m_config(config), m_nodeColor(nodeColor), m_constraintColor(constraintColor)
{
	switch (debugType)
	{
	case 0:
		CreateTPose_CapsulesAndSpheres();
		break;
	case 1:
		CreateSphereNode();
		break;
	case 2:
		CreateCapsuleNode();
		break;
	case 3:
		CreateDebugNodes();
		break;
	default:
		break;
	}

	m_brokenLimit = g_theRNG->RollRandomIntInRange(3, 10);
}

Ragdoll::~Ragdoll()
{
	for (auto& n : m_nodes)
	{
		delete n;
		n = nullptr;
	}

	for (auto& c : m_constraints)
	{
		delete c;
		c = nullptr;
	}
}

DoubleMat44 Ragdoll::GetRootTransform() const
{
	return m_nodes[0]->GetModelMatrix();
}

void Ragdoll::Update(float deltaTime)
{
	m_deadTimer -= deltaTime;

	if (m_deadTimer <= 0.f)
	{
		m_deadTimer = 0.f;
		m_isDead = true;
	}
	else
	{
		m_isDead = false;
	}
}

void Ragdoll::FixedUpdate(float fixedTime)
{
	SolveOneIteration(fixedTime);
}


void Ragdoll::SolveOneIteration(float deltaTime)
{
	if (!m_isDead)
	{
		ApplyGlobalAcceleration(m_config.gravAccel);
		IntegratePosition_VelocityVerlet(deltaTime, m_config.airFriction);
		IntegrateRotation_VelocityVerlet(deltaTime);

		//for (auto& n : m_nodes)
		//{
		//	if (!n->m_isResting)
		//	{
		//		double energy = GetTotalEnergy(n->m_velocity, n->m_mass, m_config.gravAccel, n->m_position.z);
		//		n->m_isResting = energy < 50.0;
		//	}
		//}
	}

	for (auto& n : m_nodes)
	{
		n->m_netForce = DoubleVec3::ZERO;
		n->m_torque = DoubleVec3::ZERO;
	}

	ApplyConstraints(deltaTime, m_config.iterations, DEBUG_solveConstraintWithFixedIteration);

	PushRagdollOutOfDefaultPlane3D_Double(this);
}

void Ragdoll::Render() const
{
	for (auto& n : m_nodes)
	{
		n->Render();
	}
	for (auto& c : m_constraints)
	{
		c->Render();
	}
}

void Ragdoll::IntegratePosition_VelocityVerlet(float deltaTime, double f)
{
	for (auto& n : m_nodes)
	{
		if (n->m_isResting && m_game->DEBUG_gameObjectCanRest)
		{
			n->m_velocity = DoubleVec3();
			continue;
		}
		n->m_position += (1.0 - f) * n->m_velocity * deltaTime + 0.5 * n->m_acceleration * deltaTime * deltaTime;
		DoubleVec3 accelerationThisFrame = n->GetAcceleration();
		n->m_velocity += 0.5 * (n->m_acceleration + accelerationThisFrame) * deltaTime;

		n->m_acceleration = accelerationThisFrame;

		n->m_velocity.UniformClamp(-DEBUG_maxVelocity, DEBUG_maxVelocity);
	}
}

void Ragdoll::IntegrateRotation_VelocityVerlet(float deltaTime)
{
	for (auto& n : m_nodes)
	{
		if (n->m_isResting && m_game->DEBUG_gameObjectCanRest)
		{
			n->m_angularVelocity = DoubleVec3();
			continue;
		}
		VelocityState current(n->m_orientation, n->m_angularVelocity);
		VelocityState next = StepWithVelocity(deltaTime, current, n->m_torque, n->GetInverseInertiaTensor(), 0.7);
		n->m_orientation = next.q;
		n->m_orientation.Normalize();
		n->m_angularVelocity = next.omega;
	}
}

void Ragdoll::AccumulateForce(std::string name, DoubleVec3 force)
{
	Node* node = GetNode(name);
	node->AccumulateForce(force);
}


void Ragdoll::ApplyGlobalAcceleration(DoubleVec3 accel)
{
	for (auto& n : m_nodes)
	{
		n->AccumulateForce(accel * n->m_mass);
	}
}

void Ragdoll::ApplyGlobalImpulse(DoubleVec3 impulse)
{
	for (auto& n : m_nodes)
	{
		n->AccumulateImpulse(impulse);
	}
}

void Ragdoll::ApplyGlobalImpulseOnRoot(DoubleVec3 impulse)
{
	ApplyGlobalImpulse(impulse * 0.005);
	m_nodes[0]->AccumulateImpulse(impulse);
}

void Ragdoll::ApplyConstraints(float timeStep, int interation, bool fixedInteration)
{
	if (fixedInteration)
	{
		for (size_t i = 0; i < interation; i++)
		{
			for (auto& c : m_constraints)
			{
				c->SolveDistanceAndVelocity(timeStep, DEBUG_posFixRate);
				c->SolveAngle(timeStep, DEBUG_angleFixRate);
			}
		}
	}
	else
	{
		for (auto& c : m_constraints)
		{
			for (size_t i = 0; i < c->m_iteration; i++)
			{
				c->SolveDistanceAndVelocity(timeStep, DEBUG_posFixRate);
				c->SolveAngle(timeStep, DEBUG_angleFixRate);
			}
		}
	}
}

void Ragdoll::GetBoundingSphere(DoubleVec3& out_Center, double& out_radius)
{
	DoubleVec3 rootPos = GetNode(0)->m_position;
	double furthestNodeSquaredDistance = 0.f;
	double largestRadius = 0.f;
	for (auto& n : m_nodes)
	{
		DoubleVec3 nodePos = n->m_position;
		double squaredDist = (rootPos - nodePos).GetLengthSquared();
		if (n->m_radius > largestRadius)
		{
			largestRadius = n->m_radius;
		}
		if (squaredDist > furthestNodeSquaredDistance)
		{
			furthestNodeSquaredDistance = squaredDist;
		}
	}
	out_Center = rootPos;
	out_radius = sqrt(furthestNodeSquaredDistance) + largestRadius;
}

double Ragdoll::GetAverageSpeedNodes() const
{
	double totalSpeed = 0.f;
	for (Node* n : m_nodes)
	{
		totalSpeed += n->m_velocity.GetLength();
	}

	return totalSpeed /= (double)m_nodes.size();
}

void Ragdoll::BreakNodeFromRagdoll(Node* n)
{
	if (!m_isBreakable) return;
	if (m_brokenCount > m_brokenLimit) return;

	for (auto& c : m_constraints)
	{
		if (c->nB == n)
		{
			auto find = std::find(m_constraints.begin(), m_constraints.end(), c);
			delete c;
			m_constraints.erase(find);
			m_brokenCount++;
			return;
		}
	}
}

std::vector<Node*> Ragdoll::GetNodeList() const
{
	return m_nodes;
}

std::vector<Constraint*> Ragdoll::GetConstraints() const
{
	return m_constraints;
}

Node* Ragdoll::GetNode(std::string name)
{
	for (auto& n : m_nodes)
	{
		if (n->m_name == name)
		{
			return n;
		}
	}

	return m_nodes[0];
}

Node* Ragdoll::GetNode(int nodeIndex)
{
	return m_nodes[nodeIndex];
}

std::string Ragdoll::GetNodeName(int index) const
{
	return m_nodes[index]->m_name;
}

std::string Ragdoll::GetConstraintName(int index) const
{
	return m_constraints[index]->nA->m_name + " to " + m_constraints[index]->nB->m_name;
}

void Ragdoll::CheckShouldItBeDead()
{
	double thisFrameEnergy = 0;

	for (auto* n : m_nodes)
	{
		thisFrameEnergy += GetTotalEnergy(n->m_velocity, n->m_mass, m_config.gravAccel, n->m_position.z);
	}

	m_isDead |= abs(m_totalEnergy - thisFrameEnergy) <= 50;

	for (auto* n : m_nodes)
	{
		n->m_isFixed = m_isDead;
	}

	m_totalEnergy = thisFrameEnergy;
}

void Ragdoll::CreateSphereNode()
{
	DoubleVec3 position = m_transform.GetTranslation3D();
	auto newNode = new SphereNode(m_game, "root", 0.7, nullptr, 1, m_nodeColor);
	newNode->m_ragdoll = this;
	newNode->m_position = position;
	m_nodes.push_back(newNode);
}

void Ragdoll::CreateCapsuleNode()
{
	DoubleVec3 position = m_transform.GetTranslation3D();
	auto newNode = new CapsuleNode(m_game, "root", 0.7, DoubleVec3(0, 0, 1), 1.5, nullptr, 1, m_nodeColor);
	newNode->m_ragdoll = this;
	newNode->m_position = position;
	m_nodes.push_back(newNode);
}

void Ragdoll::CreateTPose_CapsulesAndSpheres()
{
	DoubleVec3 position = m_transform.GetTranslation3D();
	DoubleVec3 foward = m_transform.GetIBasis3D();
	DoubleVec3 left = m_transform.GetJBasis3D();
	DoubleVec3 up = m_transform.GetKBasis3D();

	// section 0
	Node* pelvis = CreateRootNode("pelvis", position, 0.7, 1.0);
	Node* lower_Torso = CreateSphereNode("lower_Torso", up * 1.5, 0.7, "pelvis", 1);
	Node* upper_Torso = CreateSphereNode("upper_Torso", up * 1.5, 0.7, "lower_Torso", 1);
	Node* head = CreateSphereNode("head", up * 1.5, 0.7, "upper_Torso", 0.5);

	// section 1
	Node* left_shoulder = CreateSphereNode("left_shoulder", left * 1.3 + up * 0.5, 0.6, "upper_Torso", 0.5);
	Node* left_upperArm = CreateCapsuleNode("left_upperArm", left * 1.45, 0.5, left, 0.3, "left_shoulder", 0.3);
	Node* left_lowerArm = CreateCapsuleNode("left_lowerArm", left * 1.65, 0.5, left, 0.3, "left_upperArm", 0.3);
	Node* left_hand = CreateSphereNode("left_hand", left * 1.3, 0.5, "left_lowerArm", 0.3);

	// section 2
	Node* right_shoulder = CreateSphereNode("right_shoulder", left * -1.3 + up * 0.5, 0.6, "upper_Torso", 0.5);
	Node* right_upperArm = CreateCapsuleNode("right_upperArm", left * -1.45, 0.5, -left, 0.3, "right_shoulder", 0.3);
	Node* right_lowerArm = CreateCapsuleNode("right_lowerArm", left * -1.65, 0.5, -left, 0.3, "right_upperArm", 0.3);
	Node* right_hand = CreateSphereNode("right_hand", left * -1.3, 0.5, "right_lowerArm", 0.3);

	// section 3
	Node* left_upperLeg = CreateCapsuleNode("left_upperLeg", foward * 0.3 + left * 0.8 + up * -1.8, 0.6, foward * 0.2 + up * -1, 0.7, "pelvis", 0.7);
	Node* left_lowerLeg = CreateCapsuleNode("left_lowerLeg", up * -2.7, 0.6, foward * -0.2 + up * -1, 0.7, "left_upperLeg", 0.4);
	Node* left_foot = CreateSphereNode("left_foot", up * -2.2, 0.6, "left_lowerLeg", 0.3);

	// section 4
	Node* right_upperLeg = CreateCapsuleNode("right_upperLeg", foward * 0.3 + left * -0.8 + up * -1.8, 0.6, foward * 0.2 + up * -1, 0.7, "pelvis", 0.7);
	Node* right_lowerLeg = CreateCapsuleNode("right_lowerLeg", up * -2.7, 0.6, foward * -0.2 + up * -1, 0.7, "right_upperLeg", 0.4);
	Node* right_foot = CreateSphereNode("right_foot", up * -2.2, 0.6, "right_lowerLeg", 0.3);

	// section 0
	DoubleVec3 s0_c0Dir = (lower_Torso->m_position - pelvis->m_position).GetNormalized();
	DoubleVec3 s0_c0PinA = pelvis->m_position + s0_c0Dir * pelvis->GetHalfLength();
	DoubleVec3 s0_c0PinB = lower_Torso->m_position - s0_c0Dir * lower_Torso->GetHalfLength();
	CreateConstraint(pelvis, lower_Torso,
		s0_c0PinA,
		s0_c0PinB,
		(s0_c0PinA - s0_c0PinB).GetLength(),
		DoubleVec3(-30.0, -30.0, -40.0),
		DoubleVec3(30.0, 30.0, 40.0));

	DoubleVec3 s0_c1Dir = (upper_Torso->m_position - lower_Torso->m_position).GetNormalized();
	DoubleVec3 s0_c1PinA = lower_Torso->m_position + s0_c1Dir * lower_Torso->GetHalfLength();
	DoubleVec3 s0_c1PinB = upper_Torso->m_position - s0_c1Dir * upper_Torso->GetHalfLength();
	CreateConstraint(lower_Torso, upper_Torso,
		s0_c1PinA,
		s0_c1PinB,
		(s0_c1PinA - s0_c1PinB).GetLength(),
		DoubleVec3(-45.0, -30.0, -45.0),
		DoubleVec3(45.0, 30.0, 45.0));

	DoubleVec3 s0_c2Dir = (head->m_position - upper_Torso->m_position).GetNormalized();
	DoubleVec3 s0_c2PinA = upper_Torso->m_position + s0_c2Dir * upper_Torso->GetHalfLength();
	DoubleVec3 s0_c2PinB = head->m_position - s0_c2Dir * head->GetHalfLength();
	CreateConstraint(upper_Torso, head,
		s0_c2PinA,
		s0_c2PinB,
		(s0_c2PinA - s0_c2PinB).GetLength(),
		DoubleVec3(-45.0, -60.0, -90.0),
		DoubleVec3(45.0, 40.0, 90.0));

	// section 1
	DoubleVec3 s1_c0Dir = (left_shoulder->m_position - upper_Torso->m_position).GetNormalized();
	DoubleVec3 s1_c0PinA = upper_Torso->m_position + s1_c0Dir * upper_Torso->GetHalfLength();
	DoubleVec3 s1_c0PinB = left_shoulder->m_position - s1_c0Dir * left_shoulder->GetHalfLength();
	CreateConstraint(upper_Torso, left_shoulder,
		s1_c0PinA,
		s1_c0PinB,
		(s1_c0PinA - s1_c0PinB).GetLength(),
		DoubleVec3(-0.0, -90.0, -40.0),
		DoubleVec3(90.0, 90.0, 90.0));

	DoubleVec3 s1_c1Dir = (left_upperArm->m_position - left_shoulder->m_position).GetNormalized();
	DoubleVec3 s1_c1PinA = left_shoulder->m_position + s1_c1Dir * left_shoulder->GetHalfLength();
	DoubleVec3 s1_c1PinB = left_upperArm->m_position - s1_c1Dir * left_upperArm->GetHalfLength();
	CreateConstraint(left_shoulder, left_upperArm,
		s1_c1PinA,
		s1_c1PinB,
		(s1_c1PinA - s1_c1PinB).GetLength(),
		DoubleVec3(-40.0, -40.0, -130.0),
		DoubleVec3(90.0, 120.0, 130.0));

	DoubleVec3 s1_c2Dir = (left_lowerArm->m_position - left_upperArm->m_position).GetNormalized();
	DoubleVec3 s1_c2PinA = left_upperArm->m_position + s1_c2Dir * left_upperArm->GetHalfLength();
	DoubleVec3 s1_c2PinB = left_lowerArm->m_position - s1_c2Dir * left_lowerArm->GetHalfLength();
	CreateConstraint(left_upperArm, left_lowerArm,
		s1_c2PinA,
		s1_c2PinB,
		(s1_c2PinA - s1_c2PinB).GetLength(),
		DoubleVec3(-45.0, -40.0, -45.0),
		DoubleVec3(90.0, 110.0, 45.0));

	DoubleVec3 s1_c3Dir = (left_hand->m_position - left_lowerArm->m_position).GetNormalized();
	DoubleVec3 s1_c3PinA = left_lowerArm->m_position + s1_c3Dir * left_lowerArm->GetHalfLength();
	DoubleVec3 s1_c3PinB = left_hand->m_position - s1_c3Dir * left_hand->GetHalfLength();
	CreateConstraint(left_lowerArm, left_hand,
		s1_c3PinA,
		s1_c3PinB,
		(s1_c3PinA - s1_c3PinB).GetLength(),
		DoubleVec3(-10.0, -20.0, -45.0),
		DoubleVec3(10.0, 20.0, 45.0));

	// section 2
	DoubleVec3 s2_c0Dir = (right_shoulder->m_position - upper_Torso->m_position).GetNormalized();
	DoubleVec3 s2_c0PinA = upper_Torso->m_position + s2_c0Dir * upper_Torso->GetHalfLength();
	DoubleVec3 s2_c0PinB = right_shoulder->m_position - s2_c0Dir * right_shoulder->GetHalfLength();
	CreateConstraint(upper_Torso, right_shoulder,
		s2_c0PinA,
		s2_c0PinB,
		(s2_c0PinA - s2_c0PinB).GetLength(),
		DoubleVec3(-0.0, -90.0, -40.0),
		DoubleVec3(90.0, 90.0, 90.0));

	DoubleVec3 s2_c1Dir = (right_upperArm->m_position - right_shoulder->m_position).GetNormalized();
	DoubleVec3 s2_c1PinA = right_shoulder->m_position + s2_c1Dir * right_shoulder->GetHalfLength();
	DoubleVec3 s2_c1PinB = right_upperArm->m_position - s2_c1Dir * right_upperArm->GetHalfLength();
	CreateConstraint(right_shoulder, right_upperArm,
		s2_c1PinA,
		s2_c1PinB,
		(s2_c1PinA - s2_c1PinB).GetLength(),
		DoubleVec3(-40.0, -40.0, -130.0),
		DoubleVec3(90.0, 120.0, 130.0));

	DoubleVec3 s2_c2Dir = (right_lowerArm->m_position - right_upperArm->m_position).GetNormalized();
	DoubleVec3 s2_c2PinA = right_upperArm->m_position + s2_c2Dir * right_upperArm->GetHalfLength();
	DoubleVec3 s2_c2PinB = right_lowerArm->m_position - s2_c2Dir * right_lowerArm->GetHalfLength();
	CreateConstraint(right_upperArm, right_lowerArm,
		s2_c2PinA,
		s2_c2PinB,
		(s2_c2PinA - s2_c2PinB).GetLength(),
		DoubleVec3(-45.0, -40.0, -45.0),
		DoubleVec3(90.0, 110.0, 45.0));

	DoubleVec3 s2_c3Dir = (right_hand->m_position - right_lowerArm->m_position).GetNormalized();
	DoubleVec3 s2_c3PinA = right_lowerArm->m_position + s2_c3Dir * right_lowerArm->GetHalfLength();
	DoubleVec3 s2_c3PinB = right_hand->m_position - s2_c3Dir * right_hand->GetHalfLength();
	CreateConstraint(right_lowerArm, right_hand,
		s2_c3PinA,
		s2_c3PinB,
		(s2_c3PinA - s2_c3PinB).GetLength(),
		DoubleVec3(-10.0, -20.0, -45.0),
		DoubleVec3(10.0, 20.0, 45.0));

	// section 3
	DoubleVec3 s3_c0Dir = (left_upperLeg->m_position - pelvis->m_position).GetNormalized();
	DoubleVec3 s3_c0PinA = pelvis->m_position + s3_c0Dir * pelvis->GetHalfLength();
	DoubleVec3 s3_c0PinB = left_upperLeg->m_position - s3_c0Dir * left_upperLeg->GetHalfLength();
	CreateConstraint(pelvis, left_upperLeg,
		s3_c0PinA,
		s3_c0PinB,
		(s3_c0PinA - s3_c0PinB).GetLength(),
		DoubleVec3(-15.0, -40.0, -10.0),
		DoubleVec3(0.0, 60.0, 10.0));

	DoubleVec3 s3_c1Dir = (left_lowerLeg->m_position - left_upperLeg->m_position).GetNormalized();
	DoubleVec3 s3_c1PinA = left_upperLeg->m_position + s3_c1Dir * left_upperLeg->GetHalfLength();
	DoubleVec3 s3_c1PinB = left_lowerLeg->m_position - s3_c1Dir * left_lowerLeg->GetHalfLength();
	CreateConstraint(left_upperLeg, left_lowerLeg,
		s3_c1PinA,
		s3_c1PinB,
		(s3_c1PinA - s3_c1PinB).GetLength(),
		DoubleVec3(-20.0, -140.0, -70.0),
		DoubleVec3(20.0, 0.0, 70.0));

	DoubleVec3 s3_c2Dir = (left_foot->m_position - left_lowerLeg->m_position).GetNormalized();
	DoubleVec3 s3_c2PinA = left_lowerLeg->m_position + s3_c2Dir * left_lowerLeg->GetHalfLength();
	DoubleVec3 s3_c2PinB = left_foot->m_position - s3_c2Dir * left_foot->GetHalfLength();
	CreateConstraint(left_lowerLeg, left_foot,
		s3_c2PinA,
		s3_c2PinB,
		(s3_c2PinA - s3_c2PinB).GetLength(),
		DoubleVec3(-20.0, -20.0, 0.0),
		DoubleVec3(45.0, 20.0, 0.0));

	// section 4
	DoubleVec3 s4_c0Dir = (right_upperLeg->m_position - pelvis->m_position).GetNormalized();
	DoubleVec3 s4_c0PinA = pelvis->m_position + s4_c0Dir * pelvis->GetHalfLength();
	DoubleVec3 s4_c0PinB = right_upperLeg->m_position - s4_c0Dir * right_upperLeg->GetHalfLength();
	CreateConstraint(pelvis, right_upperLeg,
		s4_c0PinA,
		s4_c0PinB,
		(s4_c0PinA - s4_c0PinB).GetLength(),
		DoubleVec3(-15.0, -40.0, -10.0),
		DoubleVec3(0.0, 60.0, 10.0));

	DoubleVec3 s4_c1Dir = (right_lowerLeg->m_position - right_upperLeg->m_position).GetNormalized();
	DoubleVec3 s4_c1PinA = right_upperLeg->m_position + s4_c1Dir * right_upperLeg->GetHalfLength();
	DoubleVec3 s4_c1PinB = right_lowerLeg->m_position - s4_c1Dir * right_lowerLeg->GetHalfLength();
	CreateConstraint(right_upperLeg, right_lowerLeg,
		s4_c1PinA,
		s4_c1PinB,
		(s4_c1PinA - s4_c1PinB).GetLength(),
		DoubleVec3(-20.0, -140.0, -70.0),
		DoubleVec3(20.0, 0.0, 70.0));

	DoubleVec3 s4_c2Dir = (right_foot->m_position - right_lowerLeg->m_position).GetNormalized();
	DoubleVec3 s4_c2PinA = right_lowerLeg->m_position + s4_c2Dir * right_lowerLeg->GetHalfLength();
	DoubleVec3 s4_c2PinB = right_foot->m_position - s4_c2Dir * right_foot->GetHalfLength();
	CreateConstraint(right_lowerLeg, right_foot,
		s4_c2PinA,
		s4_c2PinB,
		(s4_c2PinA - s4_c2PinB).GetLength(),
		DoubleVec3(-20.0, -20.0, 0.0),
		DoubleVec3(45.0, 20.0, 0.0));
}

void Ragdoll::CreateDebugNodes()
{
	Vec3 position = m_transform.GetTranslation3D();
	// Implement Facing up
	CreateRootNode("n0", position, 0.7, 1);
	//Node* n1 = CreateCapsuleNode("n1", DoubleVec3(0, 0, -3.8), 0.7, DoubleVec3(0, 0, -1), 2, "n0", 0.1, true, Rgba8::COLOR_BRIGHT_BLUE);
	//Node* n1 = CreateCapsuleNode("n1", DoubleVec3(0, 0, -3.8), 0.7, DoubleVec3(0, 0, -1), 2, "n0", 0.1, true, Rgba8::COLOR_BRIGHT_BLUE);
	//Node* n2 = CreateCapsuleNode("n2", DoubleVec3(0, 0, -5.8), 0.7, DoubleVec3(0, 0, -1), 2, "n1", 0.1, true, Rgba8::COLOR_BRIGHT_BLUE);
	//Node* n3 = CreateNode("n3", DoubleVec3(0, 0, 2.1), 0.7, "n2", 1, true, Rgba8::COLOR_DARK_GREEN);
	//Node* n4 = CreateNode("n4", DoubleVec3(1.8, 0, 0), 0.7, "n2", 1, true, Rgba8::COLOR_DARK_GREEN);
	//Node* n5 = CreateNode("n5", DoubleVec3(-1.8, 0, 0), 0.7, "n2", 1, true, Rgba8::COLOR_DARK_RED);

	//DoubleVec3 c0Dir = (n1->m_position - n0->m_position).GetNormalized();
	//DoubleVec3 c0PinA = n0->m_position + c0Dir * n0->GetHalfLength();
	//DoubleVec3 c0PinB = n1->m_position - c0Dir * n1->GetHalfLength();
	//Constraint* c0 = CreateConstraint(n0, n1,
	//	c0PinA,
	//	c0PinB,
	//	(c0PinA - c0PinB).GetLength(),
	//	DoubleVec3(-140, -140, -140),
	//	DoubleVec3(140, 140, 140));
	//
	//DoubleVec3 c1Dir = (n2->m_position - n1->m_position).GetNormalized();
	//DoubleVec3 c1PinA = n1->m_position + c1Dir * n1->GetHalfLength();
	//DoubleVec3 c1PinB = n2->m_position - c1Dir * n2->GetHalfLength();
	//Constraint* c1 = CreateConstraint(n1, n2,
	//	c1PinA,
	//	c1PinB,
	//	(c1PinA - c1PinB).GetLength(),
	//	DoubleVec3(-40, -40, -40),
	//	DoubleVec3(40, 40, 40));

	//DoubleVec3 c2Dir = (n3->m_position - n2->m_position).GetNormalized();
	//DoubleVec3 c2PinA = n2->m_position + c2Dir * n2->m_collisonRadius;
	//DoubleVec3 c2PinB = n3->m_position - c2Dir * n3->m_collisonRadius;
	//Constraint* c2 = CreateConstraint(n2, n3,
	//	c2PinA,
	//	c2PinB,
	//	(c2PinA - c2PinB).GetLength(),
	//	DoubleVec3(-10, -10, -10),
	//	DoubleVec3(10, 10, 10));

	//DoubleVec3 c3Dir = (n4->m_position - n2->m_position).GetNormalized();
	//DoubleVec3 c3PinA = n2->m_position + c3Dir * n2->m_collisonRadius;
	//DoubleVec3 c3PinB = n4->m_position - c3Dir * n4->m_collisonRadius;
	//Constraint* c3 = CreateConstraint(n2, n4,
	//	c3PinA,
	//	c3PinB,
	//	(c3PinA - c3PinB).GetLength(),
	//	DoubleVec3(0, 0, -10),
	//	DoubleVec3(90, 120, 30));
	//
	//DoubleVec3 c4Dir = (n5->m_position - n2->m_position).GetNormalized();
	//DoubleVec3 c4PinA = n2->m_position + c4Dir * n2->m_collisonRadius;
	//DoubleVec3 c4PinB = n5->m_position - c4Dir * n5->m_collisonRadius;
	//Constraint* c4 = CreateConstraint(n2, n5,
	//	c4PinA,
	//	c4PinB,
	//	(c4PinA - c4PinB).GetLength(),
	//	DoubleVec3(0, 0, -10),
	//	DoubleVec3(90, 120, 30));
}

Node* Ragdoll::CreateRootNode(std::string name,
	DoubleVec3 offsetPositionToParent,
	double radius,
	double mass)
{
	auto newNode = new SphereNode(m_game, name, radius, nullptr, mass, m_nodeColor);
	newNode->m_ragdoll = this;
	newNode->m_position = offsetPositionToParent;
	m_nodes.push_back(newNode);
	return newNode;
}

Node* Ragdoll::CreateSphereNode(std::string name,
	DoubleVec3 offsetPositionToParent,
	double radius,
	std::string parentName,
	double mass)
{
	DoubleVec3 offset = offsetPositionToParent;

	Node* parent = GetNode(parentName);
	if (parent)
	{
		offset += parent->m_offsetToParent;
	}
	auto newNode = new SphereNode(m_game, name, radius, parent, mass, m_nodeColor);
	DoubleMat44 matrix = DoubleMat44::CreateTranslation3D(m_nodes[0]->m_position);
	newNode->m_position = matrix.TransformPosition3D(offset);
	newNode->m_ragdoll = this;
	newNode->m_offsetToParent = offset;
	m_nodes.push_back(newNode);
	return newNode;
}

Node* Ragdoll::CreateCapsuleNode(std::string name,
	DoubleVec3 offsetPositionToParent,
	double radius,
	DoubleVec3 axis,
	double halfLength,
	std::string parentName,
	double mass /*= 1.0*/)
{
	DoubleVec3 offset = offsetPositionToParent;

	Node* parent = GetNode(parentName);
	if (parent)
	{
		offset += parent->m_offsetToParent;
	}
	auto newNode = new CapsuleNode(m_game, name, radius, axis.GetNormalized(), halfLength, parent, mass, m_nodeColor);
	DoubleMat44 matrix = DoubleMat44::CreateTranslation3D(m_nodes[0]->m_position);
	newNode->m_position = matrix.TransformPosition3D(offset);
	newNode->m_ragdoll = this;
	newNode->m_offsetToParent = offset;
	m_nodes.push_back(newNode);
	return newNode;
}

Constraint* Ragdoll::CreateConstraint(Node* nA, Node* nB,
	const DoubleVec3& pinA, const DoubleVec3& pinB,
	const double targetDistance,
	const DoubleVec3& minAngle, // MIN ANGLE in x axis, y axis, and z axis
	const DoubleVec3& maxAngle) // MAX ANGLE in x axis, y axis, and z axis
{
	Constraint* newConstraint = new Constraint(m_game, nA, nB, pinA, pinB, targetDistance, minAngle, maxAngle, m_constraintColor);
	newConstraint->m_ragdoll = this;
	m_constraints.push_back(newConstraint);
	return newConstraint;
}

Node::Node(Game* game, std::string name, double radius, Node* parent, double mass)
	:GameObject(game), m_name(name), m_parent(parent), m_radius(radius)
{
	m_orientation = DoubleQuaternion();

	m_mass = mass;
	m_invMass = 1 / mass;

	m_isNode = true;
	m_isFixed = false;
}


void Node::Render() const
{
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindShader(m_game->m_diffuseShader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);
	g_theRenderer->BindTexture(nullptr, 2);
	g_theRenderer->SetModelConstants(GetModelMatrix(), m_color);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);

	if (m_game->DEBUG_drawDebug)
	{
		g_theRenderer->BindShader(nullptr);
		std::vector<Vertex_PCU> debug;
		AddVertsForLineAABB3D(debug, GetBoundingBox(), Rgba8::COLOR_CYAN);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexArray(debug.size(), debug.data(), true);
		if (m_debugbuffer) g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());
	}
}

bool Node::IsSphere() const
{
	return m_isSphere;
}

DoubleVec3 Node::GetFurthestPointPenetrated(DoubleVec3 collisionPoint)
{
	if (IsSphere())
	{
		return m_position + (collisionPoint - m_position).GetNormalized() * m_radius;
	}
	else
	{
		DoubleVec3 start = m_position - GetAxis() * (GetHalfLength() - m_radius);
		DoubleVec3 end = m_position + GetAxis() * (GetHalfLength() - m_radius);
		DoubleCapsule3 thisCapsule = DoubleCapsule3(start, end, m_radius);

		Vec3 closestCenter = GetNearestPointOnLineSegment3D_Double(thisCapsule.GetBone(), collisionPoint);

		return closestCenter + (collisionPoint - closestCenter).GetNormalized() * m_radius;
	}
}

Constraint::Constraint(Game* game, Node* nA /* parent */, Node* nB /* child */, const DoubleVec3& pinA, const DoubleVec3& pinB, const double targetDistance, const DoubleVec3& minAngle, const DoubleVec3& maxAngle, Rgba8 color)
	:m_game(game), nA(nA), nB(nB), m_minAngle(minAngle), m_maxAngle(maxAngle), m_targetDistance(targetDistance)
{
	m_rPinA = pinA - nA->m_position;
	m_rPinB = pinB - nB->m_position;

	float radius = (float)(nA->m_radius + nB->m_radius) * 0.5f - 0.2f;
	AddVertsForSphere(m_vertexes, m_indexes, Vec3::ZERO, (float)radius, color, AABB2::ZERO_TO_ONE, 16, 32);

	m_vbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN) * (unsigned int)m_vertexes.size());
	m_ibuffer = g_theRenderer->CreateIndexBuffer(sizeof(unsigned int) * (unsigned int)m_indexes.size());
	g_theRenderer->CopyCPUToGPU(m_vertexes.data(), (int)(m_vertexes.size() * sizeof(Vertex_PCUTBN)), m_vbuffer);
	g_theRenderer->CopyCPUToGPU(m_indexes.data(), (int)(m_indexes.size() * sizeof(unsigned int)), m_ibuffer);

	if (nA->m_mass >= nB->m_mass)
	{
		m_iteration = 3 * (int)(nA->m_mass / nB->m_mass) + 2;
	}
	else
	{
		m_iteration = 3 * (int)(nB->m_mass / nA->m_mass) + 2;
	}

}

Constraint::~Constraint()
{
	delete m_vbuffer;
	delete m_ibuffer;
}

bool Constraint::SolveDistanceAndVelocity(float timeStep, double fixRate)
{
	DoubleVec3 worldPinA = nA->GetPointOnBody(m_rPinA);
	DoubleVec3 worldPinB = nB->GetPointOnBody(m_rPinB);
	DoubleVec3 deltaPos = worldPinB - worldPinA;
	DoubleVec3 deltaPosNormal = deltaPos.GetNormalized();

	double posLength = deltaPos.GetLength();

	double errorDist = m_targetDistance - posLength;

	// Resting Distance
	if (abs(errorDist) <= 0.1)
	{
		return false;
	}

	auto sign = (int)(abs(errorDist) / errorDist);

	DoubleVec3 deltaPosANormal = nA->m_orientation.GetConjugated().Rotate(deltaPosNormal).GetNormalized();
	DoubleVec3 deltaPosBNormal = nB->m_orientation.GetConjugated().Rotate(deltaPosNormal).GetNormalized();
	DoubleVec3 aCross1Pos = nA->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinA, deltaPosANormal);
	DoubleVec3 aCross2Pos = CrossProduct3D_Double(aCross1Pos, m_rPinA);
	double aDotPos = DotProduct3D_Double(deltaPosANormal, aCross2Pos);
	DoubleVec3 bCross1Pos = nB->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinB, deltaPosBNormal);
	DoubleVec3 bCross2Pos = CrossProduct3D_Double(bCross1Pos, m_rPinB);
	double bDotPos = DotProduct3D_Double(deltaPosBNormal, bCross2Pos);

	double jPos = -1.0 / (nA->m_invMass + nB->m_invMass + aDotPos + bDotPos);

	DoubleVec3 posACorrection = jPos * sign * nA->m_invMass * deltaPos * timeStep * fixRate;
	DoubleVec3 posBCorrection = jPos * sign * nB->m_invMass * deltaPos * timeStep * fixRate;

	nA->m_position += posACorrection;
	nB->m_position -= posBCorrection;

	// Calculate The Fix For Rotation
	DoubleVec3 jQuatBodyA = nA->m_orientation.GetConjugated().Rotate(jPos * deltaPos);
	DoubleVec3 jQuatBodyB = nB->m_orientation.GetConjugated().Rotate(jPos * deltaPos);

	DoubleVec3 crossQuatA = nA->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinA, jQuatBodyA);
	DoubleVec3 crossQuatB = nB->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinB, jQuatBodyB);

	DoubleQuaternion deltaQA = DoubleQuaternion::ComputeQuaternion(nA->m_orientation.Rotate(crossQuatA));
	DoubleQuaternion deltaQB = DoubleQuaternion::ComputeQuaternion(nB->m_orientation.Rotate(crossQuatB));

	//// VELOCITY AND ANGULAR VELOCITY FIX
	DoubleVec3 pinVelA = nA->m_velocity + nA->m_orientation.Rotate(CrossProduct3D_Double(nA->m_angularVelocity, m_rPinA));
	DoubleVec3 pinVelB = nB->m_velocity + nB->m_orientation.Rotate(CrossProduct3D_Double(nB->m_angularVelocity, m_rPinB));

	DoubleVec3 deltaVel = pinVelA - pinVelB;

	DoubleVec3 deltaVelNormal = deltaVel.GetNormalized();

	DoubleVec3 nDvA = nA->m_orientation.GetConjugated().Rotate(deltaVelNormal);
	DoubleVec3 nDvB = nB->m_orientation.GetConjugated().Rotate(deltaVelNormal);
	DoubleVec3 aCross1Vel = nA->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinA, nDvA);
	DoubleVec3 aCross2Vel = CrossProduct3D_Double(aCross1Vel, m_rPinA);
	double aDotVel = DotProduct3D_Double(nDvA, aCross2Vel);
	DoubleVec3 bCross1Vel = nB->GetInverseInertiaTensor() * CrossProduct3D_Double(m_rPinB, nDvB);
	DoubleVec3 bCross2Vel = CrossProduct3D_Double(bCross1Vel, m_rPinB);
	double bDotVel = DotProduct3D_Double(nDvB, bCross2Vel);

	double jVel = -1.0 / (nA->m_invMass + nB->m_invMass + aDotVel + bDotVel);

	DoubleVec3 aVelImpulse = (deltaVel * jVel * nA->m_invMass);
	DoubleVec3 bVelImpulse = (deltaVel * jVel * nB->m_invMass);

	DoubleVec3 limit = DoubleVec3(deltaImpulseLimit, deltaImpulseLimit, deltaImpulseLimit);

	aVelImpulse = Clamp(aVelImpulse, -limit, limit);
	bVelImpulse = Clamp(bVelImpulse, -limit, limit);

	nA->m_velocity += aVelImpulse;
	nB->m_velocity -= bVelImpulse;

	DoubleVec3 aAngularImpulse = nA->GetInverseInertiaTensor() * m_rPinA.Cross(nA->m_orientation.GetConjugated().Rotate(deltaVel * jVel));
	DoubleVec3 bAngularImpulse = nB->GetInverseInertiaTensor() * m_rPinB.Cross(nB->m_orientation.GetConjugated().Rotate(deltaVel * jVel));

	nA->m_angularVelocity += aAngularImpulse;
	nB->m_angularVelocity -= bAngularImpulse;

	// FIX ROTATION
	nA->m_orientation = deltaQA * nA->m_orientation;
	//nB->m_rotation = deltaQB * nB->m_rotation;

	nA->m_orientation.Normalize();
	nB->m_orientation.Normalize();

	// WAKE IF IT'S RESTING
	if (nA->m_isResting)
	{
		double energyA = GetTotalEnergy(nA->m_velocity, nA->m_mass, m_ragdoll->m_config.gravAccel, nA->m_position.z);
		if (energyA > ENERGY_EXIT_REST_THRESHOLD)
		{
			nA->m_isResting = false;
		}
	}

	if (nB->m_isResting)
	{
		double energyB = GetTotalEnergy(nB->m_velocity, nB->m_mass, m_ragdoll->m_config.gravAccel, nB->m_position.z);
		if (energyB > ENERGY_EXIT_REST_THRESHOLD)
		{
			nB->m_isResting = false;
		}
	}

	return true;
}

bool Constraint::SolveAngle(float timeStep, double fixRate)
{
	DoubleQuaternion qMin = DoubleQuaternion((double)SinDegreesDouble(m_minAngle.x / 2), (double)SinDegreesDouble(m_minAngle.y / 2), (double)SinDegreesDouble(m_minAngle.z / 2), 0);
	DoubleQuaternion qMax = DoubleQuaternion((double)SinDegreesDouble(m_maxAngle.x / 2), (double)SinDegreesDouble(m_maxAngle.y / 2), (double)SinDegreesDouble(m_maxAngle.z / 2), 0);

	DoubleQuaternion relativeQ = nB->m_orientation * nA->m_orientation.GetConjugated();
	DoubleQuaternion localQ_A = nA->m_orientation.GetConjugated() * relativeQ * nA->m_orientation;

	bool iFix = false;
	bool jFix = false;
	bool kFix = false;

	if (localQ_A.i < qMin.i || localQ_A.i > qMax.i) iFix = true;
	if (localQ_A.j < qMin.j || localQ_A.j > qMax.j) jFix = true;
	if (localQ_A.k < qMin.k || localQ_A.k > qMax.k) kFix = true;

	if (!iFix && !jFix && !kFix)
	{
		return false;
	}

	localQ_A.w = 0;
	localQ_A.i = DoubleMin(DoubleMax(localQ_A.i, qMin.i), qMax.i);
	localQ_A.j = DoubleMin(DoubleMax(localQ_A.j, qMin.j), qMax.j);
	localQ_A.k = DoubleMin(DoubleMax(localQ_A.k, qMin.k), qMax.k);

	double mag = localQ_A.GetMagnitude();
	localQ_A.w = sqrt(1.0 - mag);

	DoubleQuaternion deltaQ = nA->m_orientation * localQ_A * nA->m_orientation.GetConjugated();
	DoubleQuaternion qCorrection = relativeQ * deltaQ;

	double inertiaA = nA->GetInverseInertiaTensor().GetLength();
	double inertiaB = nB->GetInverseInertiaTensor().GetLength();
	double totalInertia = inertiaA + inertiaB;

	double ratioA = inertiaA / totalInertia;
	double ratioB = inertiaB / totalInertia;

	DoubleQuaternion unitQ = DoubleQuaternion(0, 0, 0, 1);

	DoubleQuaternion correctionA = DoubleQuaternion::SLerp(unitQ, qCorrection, ratioA * timeStep * fixRate);
	DoubleQuaternion correctionB = DoubleQuaternion::SLerp(unitQ, qCorrection.GetConjugated(), ratioB * timeStep * fixRate);

	nA->m_orientation = correctionA * nA->m_orientation;
	nB->m_orientation = correctionB * nB->m_orientation;

	nA->m_orientation.Normalize();
	nB->m_orientation.Normalize();

	DoubleVec3 worldOmegaA = nA->m_orientation.Rotate(nA->m_angularVelocity);
	DoubleVec3 worldOmegaB = nB->m_orientation.Rotate(nB->m_angularVelocity);

	DoubleVec3 correctionOmega = -(1.0f + restitution) * (worldOmegaA - worldOmegaB);

	if (!iFix) correctionOmega.x = 0;
	if (!jFix) correctionOmega.y = 0;
	if (!kFix) correctionOmega.z = 0;

	// Apply angular velocity corrections based on inertia ratios
	DoubleVec3 correctionA_AngularVel = nA->m_orientation.GetConjugated().Rotate(correctionOmega) * ratioA;
	DoubleVec3 correctionB_AngularVel = nB->m_orientation.GetConjugated().Rotate(correctionOmega) * ratioB;

	nA->m_angularVelocity += correctionA_AngularVel;
	nB->m_angularVelocity -= correctionB_AngularVel;

	return true;
}

DoubleVec3 Constraint::GetWorldPinA() const
{
	return nA->GetPointOnBody(m_rPinA);
}

DoubleVec3 Constraint::GetWorldPinB() const
{
	return nB->GetPointOnBody(m_rPinB);
}

std::string Constraint::GetName() const
{
	return nA->m_name + " to " + nB->m_name;
}

void Constraint::Render() const
{
	Mat44 mat;
	mat.SetTranslation3D(nA->m_position + (nB->m_position - nA->m_position) * 0.5);

	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindShader(m_game->m_diffuseShader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);
	g_theRenderer->BindTexture(nullptr, 2);
	g_theRenderer->SetModelConstants(mat);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);
}

void PushRagdollOutOfDefaultPlane3D_Double(Ragdoll* ragdoll)
{
	DoublePlane3 defaultPlane;
	defaultPlane.m_distanceFromOrigin = 0;
	defaultPlane.m_normal = DoubleVec3(0, 0, 1);

	for (Node* n : ragdoll->GetNodeList())
	{
		if (n->IsSphere())
		{
			// Penalty-based contact
			NodeCollisionPoint col;
			col.position = defaultPlane.GetNearestPoint(n->m_position);
			col.normalA = defaultPlane.m_normal;
			col.penetration = ((n->m_position - col.normalA * n->m_radius) - col.position).GetLength();
			col.nodeA = n;

			if (n->m_position.z < n->m_radius)
			{
				n->m_position.z = n->m_radius;

				NodeCollisionSolver solveVsPlane;
				solveVsPlane.ResolveCollision(col);

				n->AccumulateForce(-ragdoll->m_config.gravAccel);

				n->AccumulateForce(n->m_velocity * -9);

				ragdoll->BreakNodeFromRagdoll(n);
			}
		}
		else
		{
			double axisHalfLength = n->GetHalfLength() - n->m_radius;
			DoubleVec3 axisNorm = n->GetAxis().GetNormalized();
			DoubleCapsule3 cap = DoubleCapsule3(n->m_position - axisNorm * axisHalfLength, n->m_position + axisNorm * axisHalfLength, n->m_radius);

			NodeCollisionPoint col;
			col.position = defaultPlane.GetNearestPoint(n->m_position);
			col.normalA = defaultPlane.m_normal;
			col.penetration = ((n->m_position - col.normalA * n->m_radius) - col.position).GetLength();
			col.nodeA = n;

			if (cap.m_start.z < n->m_radius)
			{
				if (cap.m_end.z - cap.m_start.z > 0.1)
				{
					n->AccumulateAngularImpulse(n->m_velocity.Dot(col.normalA) * col.normalA, cap.m_start - n->m_position);
				}

				cap.m_start.z = n->m_radius;
				n->m_position = cap.m_start + cap.GetAxisNormal() * axisHalfLength;

				NodeCollisionSolver solveVsPlane;
				solveVsPlane.ResolveCollision(col);


				n->AccumulateForce(-ragdoll->m_config.gravAccel);

				n->AccumulateForce(n->m_velocity * -9);

				ragdoll->BreakNodeFromRagdoll(n);
			}

			if (cap.m_end.z < n->m_radius)
			{
				if (cap.m_start.z - cap.m_end.z > 0.1)
				{
					n->AccumulateAngularImpulse(n->m_velocity.Dot(col.normalA) * col.normalA, cap.m_end - n->m_position);
				}

				cap.m_end.z = n->m_radius;
				n->m_position = cap.m_end - cap.GetAxisNormal() * axisHalfLength;

				NodeCollisionSolver solveVsPlane;
				solveVsPlane.ResolveCollision(col);

				n->AccumulateForce(-ragdoll->m_config.gravAccel);

				n->AccumulateForce(n->m_velocity * -9);

				ragdoll->BreakNodeFromRagdoll(n);
			}
		}
	}
}

void PushRagdollOutOfAABB3D_Double(Ragdoll* ragdoll, DoubleAABB3& aabb)
{
	for (Node* n : ragdoll->GetNodeList())
	{
		n->NodeOverlapFixedAABB_Double(aabb);
	}
}

void PushRagdollOutOfOBB3D_Double(Ragdoll* ragdoll, DoubleOBB3& obb)
{
	for (Node* n : ragdoll->GetNodeList())
	{
		n->NodeOverlapFixedOBB_Double(obb);
	}
}

void PushRagdollOutOfSphere3D_Double(Ragdoll* ragdoll, DoubleVec3& center, double radius)
{
	for (Node* n : ragdoll->GetNodeList())
	{
		n->NodeOverlapFixedSphere_Double(center, radius);
	}
}

void PushRagdollOutOfCapsule3D_Double(Ragdoll* ragdoll, DoubleCapsule3& capsule)
{
	for (Node* n : ragdoll->GetNodeList())
	{
		n->NodeOverlapFixedCapsule_Double(capsule);
	}
}

RaycastRagdollResult3D MouseRaycastVsRagdollNode(Camera* camera, Vec2 cursorPosition, Ragdoll* ragdoll)
{
	RaycastRagdollResult3D finalResult;

	for (int i = 0; i < ragdoll->GetNodeList().size(); i++)
	{
		Node* n = ragdoll->GetNodeList()[i];
		DoubleVec3 center = n->m_position;
		double radius = n->m_radius;
		RaycastResult3D resultNode = MouseRaycastVsSphere3D(camera, cursorPosition, center, (float)radius);
		if (resultNode.m_didImpact)
		{
			finalResult.m_rayStartPos = resultNode.m_rayStartPos;
			finalResult.m_rayFwdNormal = resultNode.m_rayFwdNormal;
			finalResult.m_rayMaxLength = resultNode.m_rayMaxLength;
			finalResult.m_didImpact = resultNode.m_didImpact;
			finalResult.m_impactPos = resultNode.m_impactPos;
			finalResult.m_impactNormal = resultNode.m_impactNormal;
			finalResult.m_impactDist = resultNode.m_impactDist;
			finalResult.m_hitNode = n;
			break;
		}
	}

	return finalResult;
}


void NodeCollisionSolver::ResolveCollision(NodeCollisionPoint& col)
{
	// Skip if no actual collision
	if (col.penetration <= 0) return;

	Node* bodyA = col.nodeA;
	Node* bodyB = col.nodeB;

	DoubleVec3 rA = col.position - bodyA->m_position;
	DoubleVec3 rB;
	if (bodyB)
	{
		rB = col.position - bodyB->m_position;
	}

	DoubleVec3 angularVelAWorld = bodyA->m_orientation.Rotate(bodyA->m_angularVelocity);
	DoubleVec3  angularVelBWorld;
	if (bodyB)
	{
		angularVelBWorld = bodyB->m_orientation.Rotate(bodyB->m_angularVelocity);
	}

	DoubleVec3 velAWorld = bodyA->m_velocity + angularVelAWorld.Cross(rA);
	DoubleVec3 velBWorld;
	if (bodyB)
	{
		velBWorld = bodyB->m_velocity + angularVelBWorld.Cross(rB);
	}
	DoubleVec3 relativeVelA = velAWorld - velBWorld;

	double sRelativeA = relativeVelA.Dot(col.normalA);

	if (std::abs(sRelativeA) < m_restingSpeed)
	{
		ResolveRestingContact(col, rA, rB);
		return;
	}

	DoubleVec3 rBodyA = bodyA->m_orientation.GetConjugated().Rotate(rA);
	DoubleVec3 nBodyA = bodyA->m_orientation.GetConjugated().Rotate(col.normalA.GetNormalized());
	DoubleVec3 rBodyB;
	DoubleVec3 nBodyB;
	if (bodyB)
	{
		rBodyB = bodyB->m_orientation.GetConjugated().Rotate(rB);
		nBodyB = bodyB->m_orientation.GetConjugated().Rotate(col.normalB.GetNormalized());
	}

	DoubleVec3 crossA = bodyA->GetInverseInertiaTensor() * rBodyA.Cross(nBodyA);
	double termA = nBodyA.Dot(crossA.Cross(rBodyA));
	double termB = 0;
	double invMassB = 0;
	if (bodyB)
	{
		DoubleVec3 crossB = bodyB->GetInverseInertiaTensor() * rBodyB.Cross(nBodyB);
		termB = nBodyB.Dot(crossB.Cross(rBodyB));
		invMassB = bodyB->m_invMass;
	}

	double j = -(1.0f + m_restitution) * sRelativeA / (bodyA->m_invMass + invMassB + termA + termB);

	DoubleVec3 resolved_velA = bodyA->m_velocity + j * bodyA->m_invMass * col.normalA;
	DoubleVec3 angularImpulseA = bodyA->GetInverseInertiaTensor() * rBodyA.Cross(j * nBodyA);
	DoubleVec3 resolved_avelA = bodyA->m_angularVelocity + angularImpulseA;

	if (bodyB)
	{
		DoubleVec3 resolved_velB = bodyB->m_velocity + j * bodyB->m_invMass * col.normalB;
		DoubleVec3 angularImpulseB = bodyB->GetInverseInertiaTensor() * rBodyB.Cross(j * nBodyB);
		DoubleVec3 resolved_avelB = bodyB->m_angularVelocity + angularImpulseB;

		bodyA->m_velocity = resolved_velA;
		bodyA->m_angularVelocity = resolved_avelA;

		bodyB->m_velocity = resolved_velB;
		bodyB->m_angularVelocity = resolved_avelB;
	}
	else
	{
		bodyA->m_velocity = resolved_velA;
		bodyA->m_angularVelocity = resolved_avelA;
	}

	ResolveFriction(col, rBodyA, rBodyB, j);

	// REST IF LOW VELOCITY

	bodyA->m_isResting = bodyA->m_velocity.GetLength() < VELOCITY_EXIT_REST_THRESHOLD;

	if (bodyB)
	{
		bodyB->m_isResting = bodyB->m_velocity.GetLength() < VELOCITY_EXIT_REST_THRESHOLD;
	}
}

void NodeCollisionSolver::ResolveRestingContact(const NodeCollisionPoint& col, const DoubleVec3& rA, const DoubleVec3& rB)
{
	Node* bodyA = col.nodeA;
	Node* bodyB = col.nodeB;

	DoubleVec3 velA = bodyA->m_velocity + bodyA->m_angularVelocity.Cross(rA);
	DoubleVec3 velB;
	if (bodyB)
	{
		velB = bodyB->m_velocity + bodyB->m_angularVelocity.Cross(rB);
	}
	DoubleVec3 relativeVel = velA - velB;

	double d1pen_A = (col.position - bodyA->GetFurthestPointPenetrated(col.position)).Dot(col.normalA);
	double d2pen_A = relativeVel.Dot(col.normalA);

	DoubleVec3 forceA = (m_springStiffness * d1pen_A - m_damping * d2pen_A) * col.normalA;

	if (bodyB)
	{
		double d1pen_B = (col.position - bodyB->GetFurthestPointPenetrated(col.position)).Dot(col.normalB);
		double d2pen_B = (-relativeVel).Dot(col.normalB);

		DoubleVec3 forceB = (m_springStiffness * d1pen_B - m_damping * d2pen_B) * col.normalB;

		double totalMass = bodyA->m_mass + bodyB->m_mass;
		double ratioA = bodyA->m_mass / totalMass;
		double ratioB = bodyB->m_mass / totalMass;

		forceA = DoubleMin(ratioA, 1.0) * forceA;
		forceB = DoubleMin(ratioB, 1.0) * forceB;

		bodyA->AccumulateForce(forceA);
		bodyB->AccumulateForce(forceB);
	}
	else
	{
		bodyA->AccumulateForce(forceA);
	}
}

void NodeCollisionSolver::ResolveFriction(const NodeCollisionPoint& col, const DoubleVec3& rA, const DoubleVec3& rB, double normalImpulse)
{
	Node* bodyA = col.nodeA;
	Node* bodyB = col.nodeB;

	DoubleVec3 velA = bodyA->m_velocity + bodyA->m_angularVelocity.Cross(rA);
	DoubleVec3 velB;
	if (bodyB)
	{
		velB = bodyB->m_velocity + bodyB->m_angularVelocity.Cross(rB);
	}
	DoubleVec3 relativeVel = velA - velB;

	DoubleVec3 normalVel = col.normalA * relativeVel.Dot(col.normalA);
	DoubleVec3 tangentVel = relativeVel - normalVel;
	double tangentSpeed = tangentVel.GetLength();

	// Skip if no tangential velocity
	if (tangentSpeed < 0.0001f) return;

	DoubleVec3 tangent = tangentVel / tangentSpeed;
	double frictionImpulseMagnitude = tangentSpeed;
	double maxFriction = normalImpulse * m_friction;
	frictionImpulseMagnitude = DoubleMin(frictionImpulseMagnitude, maxFriction);
	DoubleVec3 frictionImpulse = tangent * -frictionImpulseMagnitude;

	ApplyImpulseCollision(bodyA, bodyB, frictionImpulse, rA, rB);
}

void NodeCollisionSolver::ApplyImpulseCollision(Node* bodyA, Node* bodyB, const DoubleVec3& impulse, const DoubleVec3& rA, const DoubleVec3& rB)
{
	bodyA->AccumulateImpulse(impulse);
	if (bodyB)
	{
		bodyB->AccumulateImpulse(-impulse);
	}

	bodyA->AccumulateAngularImpulse(impulse, rA);

	if (bodyB)
	{
		bodyB->AccumulateAngularImpulse(impulse, rB);
	}
}

VelocityLessState StepVelocityLess(float timestep, const VelocityLessState& state, const DoubleVec3& torque, double angular_rate_damping /*= 1.0*/)
{
	VelocityLessState next;

	DoubleQuaternion delta_q = state.q * state.q_prev.GetInversed();

	if (angular_rate_damping != 1.0)
	{
		DoubleQuaternion identity;
		delta_q = DoubleQuaternion::SLerp(identity, delta_q, angular_rate_damping);
	}

	// Convert torque to quaternion format
	DoubleQuaternion torque_quat(torque.x, torque.y, torque.z, 0);

	next.q_prev = state.q;
	next.q = delta_q * state.q * (torque_quat * (timestep * timestep * 0.5) + DoubleQuaternion(0, 0, 0, 1));
	//next.q.Normalize();

	return next;
}

VelocityState StepWithVelocity(float timestep, const VelocityState& state, const DoubleVec3& torque, Vec3 invInertia, double angular_rate_damping /*= 1.0*/)
{
	VelocityState next;

	DoubleQuaternion q_1d = QuaternionDerivative(state.q, state.omega);
	DoubleQuaternion q_2d = QuaternionSecondDerivative(state.q, state.omega, torque * invInertia);

	next.q = state.q + q_1d * timestep + q_2d * (timestep * timestep * 0.5);
	next.omega = state.omega * angular_rate_damping + invInertia * torque * timestep;

	return next;
}

DoubleQuaternion QuaternionDerivative(const DoubleQuaternion& q, const DoubleVec3& omega)
{
	DoubleQuaternion omega_quat(omega.x, omega.y, omega.z, 0);
	return  (q * omega_quat) * 0.5;
}

DoubleQuaternion QuaternionSecondDerivative(const DoubleQuaternion& q, const DoubleVec3& omega, const DoubleVec3& torque)
{
	DoubleQuaternion dQ = QuaternionDerivative(q, omega);
	double w = -2 * dQ.Dot(dQ);
	return  q * DoubleQuaternion(torque.x, torque.y, torque.z, w) * 0.5;

}

double GetTotalEnergy(DoubleVec3 velocity, double mass, DoubleVec3 gravity, double height)
{
	double velocityLength = velocity.GetLength();

	double KE = 0.5 * mass * velocityLength * velocityLength;
	double PE = mass * gravity.GetLength() * height;

	return KE + PE;
}

SphereNode::SphereNode(Game* game, std::string name /*= ""*/, double radius /*= 0.5*/, Node* parent /*= nullptr*/, double mass /*= 1*/, Rgba8 debugColor /*= Rgba8::COLOR_WHITE*/)
	:Node(game, name, radius, parent, mass)
{
	m_isSphere = true;
	m_color = debugColor;
	AddVertsForSphere(m_vertexes, m_indexes, Vec3::ZERO, (float)m_radius, Rgba8::COLOR_WHITE, AABB2::ZERO_TO_ONE, 16, 32);
	CreateBuffer(g_theRenderer);


	//Vec3 p = Vec3::ZERO;
	//Vec3 i = p + GetModelMatrix().GetIBasis3D().GetNormalized() * 0.5f;
	//Vec3 j = p + GetModelMatrix().GetJBasis3D().GetNormalized() * 0.5f;
	//Vec3 k = p + GetModelMatrix().GetKBasis3D().GetNormalized() * 0.5f;

	//Vertex_PCU k1(p, Rgba8::COLOR_BLUE);
	//Vertex_PCU k2(k, Rgba8::COLOR_BLUE);

	//Vertex_PCU i1(p, Rgba8::COLOR_RED);
	//Vertex_PCU i2(i, Rgba8::COLOR_RED);

	//Vertex_PCU j1(p, Rgba8::COLOR_GREEN);
	//Vertex_PCU j2(j, Rgba8::COLOR_GREEN);

	//m_debugvertexes.push_back(k1);
	//m_debugvertexes.push_back(k2);
	//m_debugvertexes.push_back(i1);
	//m_debugvertexes.push_back(i2);
	//m_debugvertexes.push_back(j1);
	//m_debugvertexes.push_back(j2);

	//m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	//g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	//m_debugbuffer->SetIsLinePrimitive(true);
}

double SphereNode::GetHalfLength() const
{
	return m_radius;
}

DoubleVec3 SphereNode::GetAxis() const
{
	return DoubleVec3::ZERO;
}

DoubleVec3 SphereNode::GetPointOnBody(DoubleVec3 const& distanceVector) const
{
	return m_position + m_orientation.Rotate(distanceVector);
}

bool SphereNode::NodeOverlapFixedAABB_Double(DoubleAABB3 const& aabb)
{
	if (m_ragdoll->m_isDead) return false;
	DoubleVec3 nearestPoint = aabb.GetNearestPoint(m_position);

	NodeCollisionPoint col;
	col.position = nearestPoint;
	col.normalA = (m_position - col.position).GetNormalized();
	col.penetration = ((m_position - col.normalA * m_radius) - col.position).GetLength();
	col.nodeA = this;

	if (PushSphereOutOfAABB3_Double(m_position, m_radius, aabb))
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
		return true;
	}

	return false;
}

bool SphereNode::NodeOverlapFixedOBB_Double(DoubleOBB3 const& obb)
{
	if (m_ragdoll->m_isDead) return false;
	DoubleVec3 nearestPoint = obb.GetNearestPoint(m_position);

	NodeCollisionPoint col;
	col.position = nearestPoint;
	col.normalA = (m_position - col.position).GetNormalized();
	col.penetration = ((m_position - col.normalA * m_radius) - col.position).GetLength();
	col.nodeA = this;

	if (PushSphereOutOfOBB3_Double(m_position, m_radius, obb))
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
		return true;
	}

	return false;
}

bool SphereNode::NodeOverlapFixedSphere_Double(DoubleVec3 const& center, double radius)
{
	if (m_ragdoll->m_isDead)  return false;
	DoubleVec3 staticSpherePos = center;
	DoubleVec3 nearestPoint = GetNearestPointOnSphere3D_Double(m_position, center, radius);

	NodeCollisionPoint col;
	col.position = nearestPoint;
	col.normalA = (nearestPoint - center).GetNormalized();
	col.penetration = (m_radius + radius) - (m_position - center).GetLength();
	col.nodeA = this;

	if (PushSphereOutOfSphere3D_Double(m_position, m_radius, staticSpherePos, radius, true))
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
		return true;
	}

	return false;
}

bool SphereNode::NodeOverlapFixedCapsule_Double(DoubleCapsule3 const& capsule)
{
	if (m_ragdoll->m_isDead) return false;
	DoubleCapsule3 staticCapsule = capsule;
	DoubleVec3 sphereCenterCap = GetNearestPointOnLineSegment3D_Double(capsule.m_start, capsule.m_end, m_position);
	DoubleVec3 nearestPoint = GetNearestPointOnCapsule3D_Double(m_position, capsule);

	NodeCollisionPoint col;
	col.position = nearestPoint;
	col.normalA = (nearestPoint - sphereCenterCap).GetNormalized();
	col.penetration = ((m_position - col.normalA * m_radius) - col.position).GetLength();
	col.nodeA = this;

	if (PushSphereOutOfCapsule3_Double(m_position, m_radius, staticCapsule, true))
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
		return true;
	}

	return false;
}

bool SphereNode::CollisionResolveVsRagdollNode(Node* node)
{
	if (m_ragdoll->m_isDead)   return false;
	if (m_parent && m_parent == node)
	{
		return false;
	}
	if (node->m_parent && node->m_parent == this)
	{
		return false;
	}

	CollisionInfo info;
	NodeCollisionPoint col;

	if (node->IsSphere())
	{
		DoubleVec3 toN1 = m_position - node->m_position;

		info = DoSpheresOverlap3D_Info(m_position, m_radius, node->m_position, node->m_radius);

		if (info.isColliding)
		{
			col.position = info.contactPoint;
			col.normalA = (m_position - col.position).GetNormalized();
			col.normalB = (node->m_position - col.position).GetNormalized();
			col.penetration = (m_radius - (m_radius + node->m_radius - toN1.GetLength()) * 0.5);
			col.nodeA = this;
			col.nodeB = node;
		}

		bool treatAsStatic = m_velocity.GetLength() <= restingSpeed && node->m_isResting;

		PushSphereOutOfSphere3D_Double(m_position, m_radius, node->m_position, node->m_radius, treatAsStatic);
	}
	else
	{
		double n2AxisHalfLength = node->GetHalfLength() - node->m_radius;
		DoubleVec3 n2AxisNormalized = node->GetAxis().GetNormalized();
		DoubleCapsule3 n2Cap = DoubleCapsule3(node->m_position - n2AxisNormalized * n2AxisHalfLength, node->m_position + n2AxisNormalized * n2AxisHalfLength, node->m_radius);

		info = DoCapsuleAndSphereOverlap3D_Info(n2Cap, m_position, m_radius);

		if (info.isColliding)
		{
			col.position = info.contactPoint;
			col.normalB = info.normal;
			col.normalA = -info.normal;
			DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(n2Cap.m_start, n2Cap.m_end, col.position);
			col.penetration = (nearestPointToBone - col.normalA * n2Cap.m_radius - col.position).GetLength();
			col.nodeB = this;
			col.nodeA = node;
		}

		bool treatAsStatic = m_velocity.GetLength() <= restingSpeed && node->m_isResting;

		if (PushCapsuleOutOfSphere3D_Double(n2Cap, m_position, m_radius, treatAsStatic))
		{
			node->m_position = n2Cap.m_start + node->GetAxis() * n2AxisHalfLength;
		}
	}

	if (info.isColliding)
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
	}

	return info.isColliding;
}

DoubleAABB3 SphereNode::GetBoundingBox() const
{
	DoubleVec3 Min(m_position.x - m_radius, m_position.y - m_radius, m_position.z - m_radius);
	DoubleVec3 Max(m_position.x + m_radius, m_position.y + m_radius, m_position.z + m_radius);
	return DoubleAABB3(Min, Max);
}

CapsuleNode::CapsuleNode(Game* game, std::string name /*= ""*/, double radius /*= 0.5*/, DoubleVec3 axis /*= DoubleVec3()*/, double halfLength /*= 0*/, Node* parent /*= nullptr*/, double mass /*= 1*/, Rgba8 debugColor /*= Rgba8::COLOR_WHITE*/)
	:Node(game, name, radius, parent, mass)
{
	m_isSphere = false;

	m_capsuleHalfAxisLength = halfLength;
	m_capsuleAxis = m_orientation.Rotate(axis).GetNormalized();
	m_color = debugColor;

	DoubleCapsule3 capsule = DoubleCapsule3(DoubleVec3::ZERO - m_capsuleAxis * m_capsuleHalfAxisLength, DoubleVec3::ZERO + m_capsuleAxis * m_capsuleHalfAxisLength, m_radius);
	AddVertsForCapsule3D(m_vertexes, m_indexes, capsule, Rgba8::COLOR_WHITE, AABB2::ZERO_TO_ONE, 32);
	CreateBuffer(g_theRenderer);


	//Vec3 p = Vec3::ZERO;
	//Vec3 i = p + GetModelMatrix().GetIBasis3D().GetNormalized() * 0.5f;
	//Vec3 j = p + GetModelMatrix().GetJBasis3D().GetNormalized() * 0.5f;
	//Vec3 k = p + GetModelMatrix().GetKBasis3D().GetNormalized() * 0.5f;

	//Vertex_PCU k1(p, Rgba8::COLOR_BLUE);
	//Vertex_PCU k2(k, Rgba8::COLOR_BLUE);

	//Vertex_PCU i1(p, Rgba8::COLOR_RED);
	//Vertex_PCU i2(i, Rgba8::COLOR_RED);

	//Vertex_PCU j1(p, Rgba8::COLOR_GREEN);
	//Vertex_PCU j2(j, Rgba8::COLOR_GREEN);

	//m_debugvertexes.push_back(k1);
	//m_debugvertexes.push_back(k2);
	//m_debugvertexes.push_back(i1);
	//m_debugvertexes.push_back(i2);
	//m_debugvertexes.push_back(j1);
	//m_debugvertexes.push_back(j2);

	//m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	//g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	//m_debugbuffer->SetIsLinePrimitive(true);
}

double CapsuleNode::GetHalfLength() const
{
	return m_capsuleHalfAxisLength + m_radius;
}

DoubleVec3 CapsuleNode::GetAxis() const
{
	return m_orientation.Rotate(m_capsuleAxis);
}

DoubleVec3 CapsuleNode::GetPointOnBody(DoubleVec3 const& distanceVector) const
{
	DoubleVec3 point = m_position + m_orientation.Rotate(distanceVector);
	DoubleVec3 axisNormalize = GetAxis().GetNormalized();
	DoubleCapsule3 capsule = DoubleCapsule3(m_position - axisNormalize * m_capsuleHalfAxisLength, m_position + axisNormalize * m_capsuleHalfAxisLength, m_radius);
	return capsule.GetNearestPoint(point);
}

bool CapsuleNode::NodeOverlapFixedAABB_Double(DoubleAABB3 const& aabb)
{
	if (m_ragdoll->m_isDead)  return false;
	DoubleVec3 axisNormalize = GetAxis().GetNormalized();

	DoubleVec3 start = m_position - axisNormalize * m_capsuleHalfAxisLength;
	DoubleVec3 end = m_position + axisNormalize * m_capsuleHalfAxisLength;
	DoubleCapsule3 capsule = DoubleCapsule3(start, end, m_radius);

	CollisionInfo info;

	info = DoCapsuleAndAABBOverlap3D_Info(capsule, aabb);

	if (info.isColliding)
	{
		NodeCollisionPoint col;
		col.position = info.contactPoint;
		col.normalA = info.normal;
		DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(capsule.m_start, capsule.m_end, col.position);
		col.penetration = (nearestPointToBone - col.normalA * m_radius - col.position).GetLength();
		col.nodeA = this;

		if (PushCapsuleOutOfAABB3D_Double(capsule, aabb))
		{
			m_position = capsule.m_start + axisNormalize * m_capsuleHalfAxisLength;
		}

		NodeCollisionSolver solver;
		solver.ResolveCollision(col);

		return true;
	}

	return false;
}

bool CapsuleNode::NodeOverlapFixedOBB_Double(DoubleOBB3 const& obb)
{
	if (m_ragdoll->m_isDead)   return false;
	DoubleVec3 axisNormalize = GetAxis().GetNormalized();

	DoubleVec3 start = m_position - axisNormalize * m_capsuleHalfAxisLength;
	DoubleVec3 end = m_position + axisNormalize * m_capsuleHalfAxisLength;
	DoubleCapsule3 capsule = DoubleCapsule3(start, end, m_radius);

	CollisionInfo info;

	info = DoCapsuleAndOBBOverlap3D_Info(capsule, obb);


	if (info.isColliding)
	{
		NodeCollisionPoint col;
		col.position = info.contactPoint;
		col.normalA = info.normal;
		DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(capsule.m_start, capsule.m_end, col.position);
		col.penetration = (nearestPointToBone - col.normalA * m_radius - col.position).GetLength();
		col.nodeA = this;

		if (PushCapsuleOutOfOBB3D_Double(capsule, obb))
		{
			m_position = capsule.m_start + axisNormalize * m_capsuleHalfAxisLength;
		}

		NodeCollisionSolver solver;
		solver.ResolveCollision(col);

		return true;
	}

	return false;
}

bool CapsuleNode::NodeOverlapFixedSphere_Double(DoubleVec3 const& center, double radius)
{
	if (m_ragdoll->m_isDead)   return false;
	DoubleVec3 staticSpherePos = center;
	DoubleVec3 axisNormalize = GetAxis().GetNormalized();

	DoubleVec3 start = m_position - axisNormalize * m_capsuleHalfAxisLength;
	DoubleVec3 end = m_position + axisNormalize * m_capsuleHalfAxisLength;
	DoubleCapsule3 capsule = DoubleCapsule3(start, end, m_radius);

	CollisionInfo info;

	info = DoCapsuleAndSphereOverlap3D_Info(capsule, center, radius);

	if (info.isColliding)
	{
		NodeCollisionPoint col;
		col.position = info.contactPoint;
		col.normalA = info.normal;
		DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(capsule.m_start, capsule.m_end, col.position);
		col.penetration = (nearestPointToBone - col.normalA * m_radius - col.position).GetLength();
		col.nodeA = this;

		if (PushCapsuleOutOfSphere3D_Double(capsule, staticSpherePos, radius, true))
		{
			m_position = capsule.m_start + axisNormalize * m_capsuleHalfAxisLength;
		}

		NodeCollisionSolver solver;
		solver.ResolveCollision(col);

		return true;
	}

	return false;
}

bool CapsuleNode::NodeOverlapFixedCapsule_Double(DoubleCapsule3 const& capsule)
{
	if (m_ragdoll->m_isDead)   return false;
	DoubleVec3 axisNormalize = GetAxis().GetNormalized();

	DoubleVec3 start = m_position - axisNormalize * m_capsuleHalfAxisLength;
	DoubleVec3 end = m_position + axisNormalize * m_capsuleHalfAxisLength;
	DoubleCapsule3 thisCapsule = DoubleCapsule3(start, end, m_radius);
	DoubleCapsule3 staticCapsule = capsule;

	CollisionInfo info;

	info = DoCapsulesOverlap3D_Info(thisCapsule, capsule);

	if (info.isColliding)
	{
		DoubleVec3 capPoints[2];
		Optimized_GetNearestPointsBetweenLines3D_Double(capPoints, thisCapsule.GetBone(), capsule.GetBone());

		NodeCollisionPoint col;
		col.position = info.contactPoint;
		col.normalA = info.normal;
		DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(capsule.m_start, capsule.m_end, col.position);
		col.penetration = (nearestPointToBone - col.normalA * m_radius - col.position).GetLength();
		col.nodeA = this;

		if (PushCapsuleOutOfCapsule3D_Double(thisCapsule, staticCapsule))
		{
			m_position = thisCapsule.m_start + axisNormalize * m_capsuleHalfAxisLength;
		}

		NodeCollisionSolver solver;
		solver.ResolveCollision(col);

		return true;
	}

	return false;
}

bool CapsuleNode::CollisionResolveVsRagdollNode(Node* node)
{
	if (m_ragdoll->m_isDead)   return false;
	if (m_parent && m_parent == node)
	{
		return false;
	}
	if (node->m_parent && node->m_parent == this)
	{
		return false;
	}

	CollisionInfo info;
	NodeCollisionPoint col;

	if (node->IsSphere())
	{
		double n1AxisHalfLength = GetHalfLength() - m_radius;
		DoubleVec3 n1AxisNormalized = GetAxis().GetNormalized();
		DoubleCapsule3 n1Cap = DoubleCapsule3(m_position - n1AxisNormalized * n1AxisHalfLength, m_position + n1AxisNormalized * n1AxisHalfLength, m_radius);

		info = DoCapsuleAndSphereOverlap3D_Info(n1Cap, node->m_position, node->m_radius);

		if (info.isColliding)
		{
			col.position = info.contactPoint;
			col.normalA = info.normal;
			col.normalB = -info.normal;
			DoubleVec3 nearestPointToBone = GetNearestPointOnLineSegment3D_Double(n1Cap.m_start, n1Cap.m_end, col.position);
			col.penetration = (m_radius + node->m_radius - (nearestPointToBone - node->m_position).GetLength()) * 0.5;
			col.nodeA = this;
			col.nodeB = node;
		}

		bool treatAsStatic = m_velocity.GetLength() <= restingSpeed && node->m_isResting;

		if (PushCapsuleOutOfSphere3D_Double(n1Cap, node->m_position, node->m_radius, treatAsStatic))
		{
			m_position = n1Cap.m_start + n1AxisNormalized * n1AxisHalfLength;
		}
	}
	else
	{
		double n1AxisHalfLength = GetHalfLength() - m_radius;
		double n2AxisHalfLength = node->GetHalfLength() - node->m_radius;

		DoubleVec3 n1AxisNormalized = GetAxis().GetNormalized();
		DoubleVec3 n2AxisNormalized = node->GetAxis().GetNormalized();

		DoubleCapsule3 n1Cap = DoubleCapsule3(m_position - n1AxisNormalized * n1AxisHalfLength, m_position + n1AxisNormalized * n1AxisHalfLength, m_radius);
		DoubleCapsule3 n2Cap = DoubleCapsule3(node->m_position - n2AxisNormalized * n2AxisHalfLength, node->m_position + n2AxisNormalized * n2AxisHalfLength, node->m_radius);

		info = DoCapsulesOverlap3D_Info(n1Cap, n2Cap);

		if (info.isColliding)
		{
			DoubleVec3 capPoints[2];
			Optimized_GetNearestPointsBetweenLines3D_Double(capPoints, n1Cap.GetBone(), n2Cap.GetBone());

			col.position = info.contactPoint;
			col.normalA = info.normal;
			col.normalB = -info.normal;
			col.penetration = (m_radius + node->m_radius - (capPoints[0] - capPoints[1]).GetLength()) * 0.5;
			col.nodeA = this;
			col.nodeB = node;
		}

		bool treatAsStatic = m_velocity.GetLength() <= restingSpeed && node->m_isResting;

		if (PushCapsuleOutOfCapsule3D_Double(n1Cap, n2Cap, treatAsStatic))
		{
			m_position = n1Cap.m_start + n1AxisNormalized * n1AxisHalfLength;
			node->m_position = n2Cap.m_start + n2AxisNormalized * n2AxisHalfLength;
		}
	}

	if (info.isColliding)
	{
		NodeCollisionSolver solver;
		solver.ResolveCollision(col);
	}

	return info.isColliding;
}

DoubleAABB3 CapsuleNode::GetBoundingBox() const
{
	DoubleCapsule3 capsule = DoubleCapsule3(m_position - GetAxis() * m_capsuleHalfAxisLength, m_position + GetAxis() * m_capsuleHalfAxisLength, m_radius);

	return capsule.GetBoundingBox();
}

void RagdollPhysicsJob::Execute()
{
	if (m_ragdoll->m_isDead) return;

	for (size_t i = 0; i < m_iterations; i++)
	{
		m_ragdoll->SolveOneIteration(m_timeStep);
	}
}
