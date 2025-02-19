#pragma once
#include "Game/GameCommon.hpp"

class Player
{
public:
	Player(Vec3 pos = Vec3(), EulerAngles rot = EulerAngles());
	virtual ~Player();
	
	void Update(float deltaSeconds);
	void Render() const;

	Camera* GetCamera();
	Mat44 GetModeMatrix();
	Vec3 GetForward();
public:

	Vec3 m_position;
	Vec3 m_velocity;
	EulerAngles m_orientationDegrees;
private:
	Camera* m_playerCamera = nullptr;
	float m_movingSpeed = 4.f;
	float m_rotatingSpeed = 90.f;

private:
	void HandleInput();
	void Movement(float deltaSeconds);
};