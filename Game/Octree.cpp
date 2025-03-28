#include "Game/Octree.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/Game.hpp"

Octree::Octree()
	:m_region(DoubleAABB3(0, 0, 0, 0, 0, 0))
{
	m_curLife = -1;
}

Octree::Octree(DoubleAABB3 region, std::vector<GameObject*> objects)
	:m_region(region), m_objects(objects)
{
	for (size_t i = 0; i < m_objects.size(); i++)
	{
		m_objects[i]->m_currentOctree = this;
	}
	m_curLife = -1;
}

Octree::Octree(DoubleAABB3 region)
	:m_region(region)
{
	m_curLife = -1;
}

Octree::~Octree()
{
	delete m_debugbuffer;

	for (auto& i : m_childNode)
	{
		if (i)
		{
			delete i;
		}
	}
}

void Octree::Update()
{
	if (!bm_built || !bm_ready)
	{
		BuildTree();
		return;
	}

	UpdateTreeObjects(this);

	if (m_isRoot)
	{
		std::vector<GameObject*> newList;
		std::vector<CollisionRecord*> recordList = GetIntersection(newList);

		for (auto & i : recordList)
		{
			i->Resolve();
		}
	}

	// FOR DEBUG ---------------------------------------
	for (GameObject* n : m_objects)
	{
		n->m_currentOctree = this;
	}
	// -------------------------------------------------

	PruneDeadBranches(this);
}


void Octree::Render() const
{
	if (!m_debugDraw) return;

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);

	Rgba8 color = Rgba8::COLOR_PINK;
	if (m_curLife != -1)
	{
		float currentLifePercent = RangeMap((float)m_curLife, 0.f, (float)m_maxLifespan, 0.f, 1.f);
		color = Interpolate(Rgba8::COLOR_PINK, Rgba8::COLOR_BLACK, currentLifePercent);
	}

	g_theRenderer->SetModelConstants(Mat44(), color);
	if (m_debugbuffer) g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());

	if (HasChildren())
	{
		for (int flags = m_activeNodes, index = 0; flags > 0; flags >>= 1, index++)
		{
			if ((flags & 1) == 1)
			{
				m_childNode[index]->Render();
			}
		}
	}
}

bool Octree::Insert(GameObject* object)
{
	if (m_objects.empty() && m_activeNodes == 0)
	{
		m_objects.push_back(object);
		object->m_currentOctree = this;
		return true;
	}

	DoubleVec3 dimensions = m_region.m_maxs - m_region.m_mins;
	if (dimensions.x <= MIN_SIZE && dimensions.y <= MIN_SIZE && dimensions.z <= MIN_SIZE)
	{
		m_objects.push_back(object);
		return true;
	}
	if (!IsAABBInside(m_region, object->GetBoundingBox()))
	{
		if (m_parent)
		{
			return m_parent->Insert(object);
		}
		else
		{
			return false;
		}
	}
	else
	{
		DoubleVec3 half = dimensions / 2.0;
		DoubleVec3 center = m_region.m_mins + half;

		DoubleAABB3 childOctant[8];
		childOctant[0] = (m_childNode[0]) ? m_childNode[0]->m_region : DoubleAABB3(m_region.m_mins, center);
		childOctant[1] = (m_childNode[1]) ? m_childNode[1]->m_region : DoubleAABB3(DoubleVec3(center.x, m_region.m_mins.y, m_region.m_mins.z), DoubleVec3(m_region.m_maxs.x, center.y, center.z));
		childOctant[2] = (m_childNode[2]) ? m_childNode[2]->m_region : DoubleAABB3(DoubleVec3(center.x, m_region.m_mins.y, center.z), DoubleVec3(m_region.m_maxs.x, center.y, m_region.m_maxs.z));
		childOctant[3] = (m_childNode[3]) ? m_childNode[3]->m_region : DoubleAABB3(DoubleVec3(m_region.m_mins.x, m_region.m_mins.y, center.z), DoubleVec3(center.x, center.y, m_region.m_maxs.z));
		childOctant[4] = (m_childNode[4]) ? m_childNode[4]->m_region : DoubleAABB3(DoubleVec3(m_region.m_mins.x, center.y, m_region.m_mins.z), DoubleVec3(center.x, m_region.m_maxs.y, center.z));
		childOctant[5] = (m_childNode[5]) ? m_childNode[5]->m_region : DoubleAABB3(DoubleVec3(center.x, center.y, m_region.m_mins.z), DoubleVec3(m_region.m_maxs.x, m_region.m_maxs.y, center.z));
		childOctant[6] = (m_childNode[6]) ? m_childNode[6]->m_region : DoubleAABB3(center, m_region.m_maxs);
		childOctant[7] = (m_childNode[7]) ? m_childNode[7]->m_region : DoubleAABB3(DoubleVec3(m_region.m_mins.x, center.y, center.z), DoubleVec3(center.x, m_region.m_maxs.y, m_region.m_maxs.z));

		bool found = false;

		for (size_t i = 0; i < 8; i++)
		{
			if (IsAABBInside(childOctant[i], object->GetBoundingBox()))
			{
				if (m_childNode[i])
				{
					return m_childNode[i]->Insert(object);
				}
				else
				{
					m_childNode[i] = CreateNode(childOctant[i], object);
					m_activeNodes |= (1 << i);
				}
				found = true;
				break;
			}
		}

		if (!found)
		{
			m_objects.push_back(object);
			object->m_currentOctree = this;
			return true;
		}
	}

	return false;
}

void Octree::BuildTree()
{
	if (m_objects.empty()) 	return;
	DoubleVec3 dimensions = m_region.m_maxs - m_region.m_mins;

	if (dimensions.x <= MIN_SIZE && dimensions.y <= MIN_SIZE && dimensions.z <= MIN_SIZE)
	{
		return;
	}

	DoubleVec3 half = dimensions / 2.0;
	DoubleVec3 center = m_region.m_mins + half;

	DoubleAABB3 octant[8];
	octant[0] = DoubleAABB3(m_region.m_mins, center);
	octant[1] = DoubleAABB3(DoubleVec3(center.x, m_region.m_mins.y, m_region.m_mins.z), DoubleVec3(m_region.m_maxs.x, center.y, center.z));
	octant[2] = DoubleAABB3(DoubleVec3(center.x, m_region.m_mins.y, center.z), DoubleVec3(m_region.m_maxs.x, center.y, m_region.m_maxs.z));
	octant[3] = DoubleAABB3(DoubleVec3(m_region.m_mins.x, m_region.m_mins.y, center.z), DoubleVec3(center.x, center.y, m_region.m_maxs.z));
	octant[4] = DoubleAABB3(DoubleVec3(m_region.m_mins.x, center.y, m_region.m_mins.z), DoubleVec3(center.x, m_region.m_maxs.y, center.z));
	octant[5] = DoubleAABB3(DoubleVec3(center.x, center.y, m_region.m_mins.z), DoubleVec3(m_region.m_maxs.x, m_region.m_maxs.y, center.z));
	octant[6] = DoubleAABB3(center, m_region.m_maxs);
	octant[7] = DoubleAABB3(DoubleVec3(m_region.m_mins.x, center.y, center.z), DoubleVec3(center.x, m_region.m_maxs.y, m_region.m_maxs.z));

	std::vector<GameObject*> octObjects[8];
	std::vector<GameObject*> delist;

	for (GameObject* n : m_objects)
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (IsAABBInside(octant[i], n->GetBoundingBox()))
			{
				octObjects[i].push_back(n);
				delist.push_back(n);
				break;
			}
		}
	}

	for (GameObject* n : delist)
	{
		auto it = std::find(m_objects.begin(), m_objects.end(), n);
		m_objects.erase(it);
	}

	for (int a = 0; a < 8; a++)
	{
		if (!octObjects[a].empty())
		{
			m_childNode[a] = CreateNode(octant[a], octObjects[a]);
			m_activeNodes |= (1 << a);
			m_childNode[a]->BuildTree();
		}
	}

	bm_built = true;
	bm_ready = true;
}

void Octree::UpdateTree()
{
	bm_ready = false;
	if (!bm_built)
	{
		while (!m_pendingInsertion.empty())
		{
			m_objects.push_back(m_pendingInsertion.front());
			m_pendingInsertion.pop_front();
		}
		BuildTree();
	}
	else
	{
		while (!m_pendingInsertion.empty())
		{
			Insert(m_pendingInsertion.front());
			m_pendingInsertion.pop_front();
		}
	}
	bm_ready = true;
}

void Octree::ResetTreeObjects(std::vector<GameObject*> objects)
{
	m_objects.clear();
	for (auto object : objects)
	{
		m_pendingInsertion.push_back(object);
	}
	UpdateTree();
}

bool Octree::HasChildren() const
{
	for (Octree* child : m_childNode)
	{
		if (child) return true;
	}

	return false;
}

Octree* Octree::CreateNode(DoubleAABB3 region, std::vector<GameObject*> objects)
{
	if (objects.empty()) return nullptr;
	Octree* tree = new Octree(region, objects);
	tree->m_parent = this;
	tree->m_layer = tree->m_parent->m_layer + 1;
	tree->m_name = Stringf("Octree layer: %i", tree->m_layer);
	tree->Init();
	return tree;
}

Octree* Octree::CreateNode(DoubleAABB3 region, GameObject* object)
{
	if (!object) return nullptr;
	std::vector<GameObject*> objectList;
	objectList.push_back(object);
	Octree* tree = new Octree(region, objectList);
	tree->m_parent = this;
	tree->m_layer = tree->m_parent->m_layer + 1;
	tree->m_name = Stringf("Octree layer: %i", tree->m_layer);
	tree->Init();
	return tree;
}

void Octree::UpdateTreeObjects(Octree* tree)
{
	std::vector<GameObject*> movedObjects;

	//Update & move all objects in the node
	for (auto& object : tree->m_objects)
	{
		if(!object) continue;

		if (object->m_isNode && !object->m_isFixed)
		{
			movedObjects.push_back(object);
		}
	}

	//update any child nodes
	if (tree->HasChildren())
	{
		for (int flags = tree->m_activeNodes, index = 0; flags > 0; flags >>= 1, index++)
		{
			if ((flags & 1) == 1)
			{
				UpdateTreeObjects(tree->m_childNode[index]);
			}
		}
	}

	for (auto& movedObj : movedObjects)
	{
		if(!movedObj) continue;

		Octree* current = tree;
		auto it = std::find(current->m_objects.begin(), current->m_objects.end(), movedObj);
		current->m_objects.erase(it);

		while (!IsAABBInside(current->m_region, movedObj->GetBoundingBox()))
		{
			if (current->m_parent) current = current->m_parent;
			else
			{
				break;
			}
		}

		current->Insert(movedObj);
	}
}

void Octree::PruneDeadBranches(Octree* tree)
{
	if (tree->m_objects.empty())
	{
		if (!tree->HasChildren())
		{
			if (tree->m_curLife == -1)
			{
				tree->m_curLife = tree->m_maxLifespan;
			}
			else if (tree->m_curLife > 0)
			{
				tree->m_curLife--;
			}
		}
	}
	else
	{
		if (tree->m_curLife != -1)
		{
			if (tree->m_maxLifespan <= 64)
			{
				tree->m_maxLifespan *= 2;
			}

			tree->m_curLife = -1;
		}
	}

	//prune out any dead branches in the tree
	for (int flags = tree->m_activeNodes, index = 0; flags > 0; flags >>= 1, index++)
	{
		if ((flags & 1) == 1)
		{
			PruneDeadBranches(tree->m_childNode[index]);

			if (tree->m_childNode[index]->m_curLife == 0)
			{
				if (!tree->m_childNode[index]->m_objects.empty())
				{
					ERROR_AND_DIE("Tried to delete a used branch!");
				}
				else
				{
					delete tree->m_childNode[index];
					tree->m_childNode[index] = nullptr;
					tree->m_activeNodes ^= (byte)(1 << index);
				}
			}
		}
	}
}

void Octree::Init()
{
	AddVertsForLineAABB3D(m_debugvertexes, m_region, Rgba8::COLOR_WHITE);
	m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	m_debugbuffer->SetIsLinePrimitive(true);
}

std::vector<CollisionRecord*> Octree::GetIntersection(std::vector<GameObject*> parentObj)
{
	std::vector<CollisionRecord*> collisions;

	for (GameObject* pObj : parentObj)
	{
		for (GameObject* lObj : m_objects)
		{
			if (pObj && lObj)
			{
				CollisionRecord* record = pObj->Node_Intersect(lObj);
				if (record)
				{
					collisions.push_back(record);
				}
			}
		}
	}

	if (m_objects.size() > 1)
	{
		std::vector<GameObject*> tmp = m_objects;
		while (!tmp.empty())
		{
			if (!tmp[tmp.size() - 1])
			{
				tmp.pop_back();
				continue;
			}

			for (GameObject* lObj : m_objects)
			{
				if (!lObj)
				{
					continue;
				}
				if (tmp[tmp.size() - 1] == lObj)
				{
					continue;
				}

				CollisionRecord* record = tmp[tmp.size() - 1]->Node_Intersect(lObj);
				if (record)
				{
					collisions.push_back(record);
				}
			}

			tmp.pop_back();
		}
	}

	for (GameObject* obj : m_objects)
	{
		parentObj.push_back(obj);
	}

	for (int flags = m_activeNodes, index = 0; flags > 0; flags >>= 1, index++)
	{
		if ((flags & 1) == 1)
		{
			if (m_childNode[index])
			{
				std::vector<CollisionRecord*> childCollisions = m_childNode[index]->GetIntersection(parentObj);
				for (CollisionRecord* c : childCollisions)
				{
					collisions.push_back(c);
				}
			}
		}
	}

	return collisions;
}

CollisionRecord::CollisionRecord(Node* node, GameObject* object)
	:m_node(node), m_object(object)
{

}

void CollisionRecord::Resolve()
{
	m_object->CollisionResolveVsRagdollNode(m_node);
}
