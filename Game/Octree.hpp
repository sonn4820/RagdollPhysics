#pragma once
#include <vector>
#include <deque>
#include "Engine/Math/AABB3.hpp"
#include "Game/GameObject.hpp"
#include "Engine/Math/MathUtils.hpp"

constexpr int LIFE_TIME = 64;

struct Node;

struct CollisionRecord
{
	CollisionRecord(Node* node, GameObject* object);

	Node* m_node = nullptr;
	GameObject* m_object = nullptr;

	void Resolve();
};

struct Octree
{
	Octree();
	Octree(DoubleAABB3 region, std::vector<GameObject*> objects);
	Octree(DoubleAABB3 region);
	~Octree();
	
	DoubleAABB3 m_region;
	std::vector<GameObject*> m_objects;
	std::deque<GameObject*> m_pendingInsertion;

	Octree* m_childNode[8] = { nullptr }; ;

	unsigned char m_activeNodes = 0;

	const float MIN_SIZE = 1.f;

	int m_maxLifespan = LIFE_TIME;
	int m_curLife = -1; 
	int m_layer = 0;
	std::string m_name = "";

	Octree* m_parent = nullptr;
	bool m_isRoot = false;

	bool bm_ready = false;
	bool bm_built = false;

	std::vector<Vertex_PCU> m_debugvertexes;
	VertexBuffer* m_debugbuffer = nullptr;

	void Init();
	void Update();
	void Render() const;
	bool Insert(GameObject* object);
	void BuildTree();
	void UpdateTree();
	void ResetTreeObjects(std::vector<GameObject*> objects);
	bool HasChildren() const;
	Octree* CreateNode(DoubleAABB3 region, std::vector<GameObject*> objects);
	Octree* CreateNode(DoubleAABB3 region, GameObject* object);

	void UpdateTreeObjects(Octree* tree);
	void PruneDeadBranches(Octree* tree);
private:

	std::vector<CollisionRecord*> GetIntersection(std::vector<GameObject*> parentObj);
};