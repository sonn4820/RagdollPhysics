#include "Game/Game.hpp"
#include "Game/Player.hpp"

constexpr float CANNON_CHARGE_RATE = 32000.f;
constexpr float CANNON_CHARGE_LIMIT = 64000.f;
constexpr int RAGDOLLS_LIMIT = 14;
Game::Game()
{


}
//..............................
Game::~Game()
{
	delete m_menuCanvas;
	delete m_planeVBO;
	delete m_planeIBO;
}
//..............................
void Game::Startup()
{
	IntVec2 screenSize = Window::GetMainWindowInstance()->GetClientDimensions();
	m_screenCamera = new Camera();
	m_screenCamera->SetOrthographicView(Vec2(0, 0), Vec2((float)screenSize.x, (float)screenSize.y));
	m_clock = new Clock(*Clock::s_theSystemClock);

	m_planeTextureN = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Grass_Normal.png");
	m_planeTextureS = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Grass_SpecGlossEmit.png");
	m_planeTextureD = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Grass_Diffuse.png");
	m_stoneTextureN = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Brick_Normal.png");
	m_stoneTextureS = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Brick_SpecGlossEmit.png");
	m_stoneTextureD = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Brick_Diffuse.png");
	m_transparentTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/t.png");
	m_skyboxTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/skybox.png");
	m_reticleTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/reticle.png");

	m_shader = g_theRenderer->CreateShader("Data/Shaders/EverythingShader", VertexType::Vertex_PCUTBN);
	m_diffuseShader = g_theRenderer->CreateShader("Data/Shaders/Diffuse", VertexType::Vertex_PCUTBN);

	plane = DoublePlane3();

	float QuadSize = 20.f;
	for (int x = -50; x < 50; x++)
	{
		for (int y = -50; y < 50; y++)
		{
			AddVertsForQuad3D(m_planeVerts, m_planeIndexes, Vec3(x * QuadSize, y * QuadSize, 0), Vec3((x + 1) * QuadSize, y * QuadSize, 0), Vec3(x * QuadSize, (y + 1) * QuadSize, 0), Vec3((x + 1) * QuadSize, (y + 1) * QuadSize, 0));
		}
	}

	m_planeVBO = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN) * (unsigned int)m_planeVerts.size());
	m_planeIBO = g_theRenderer->CreateIndexBuffer(sizeof(unsigned int) * (unsigned int)m_planeIndexes.size());
	g_theRenderer->CopyCPUToGPU(m_planeVerts.data(), (int)(m_planeVerts.size() * sizeof(Vertex_PCUTBN)), m_planeVBO);
	g_theRenderer->CopyCPUToGPU(m_planeIndexes.data(), (int)(m_planeIndexes.size() * sizeof(unsigned int)), m_planeIBO);

	m_ragdolls.reserve(40);
	m_allObjects.reserve(400);
	m_fixedObjects.reserve(30);

	Menu_Init();

	SwitchState(GameState::ATTRACT_MODE); // Player Init here
}
//----------------------------------------------------------------------------------------------------------------------------------------
// UPDATE

void Game::Update(float deltaSeconds)
{
	m_secondIntoMode += deltaSeconds;

	HandleInput();

	if (m_currentState == GameState::ATTRACT_MODE)
	{
		UpdateAttractMode(deltaSeconds);
	}
	else
	{
		IMGUI_UPDATE();
	}

	if (m_currentState == GameState::FEATURE_MODE)
	{
		UpdateFeatureMode(deltaSeconds);
	}
	if (m_currentState == GameState::PACHINKO_MODE)
	{
		UpdatePachinkoMode(deltaSeconds);
	}
	if (m_currentState == GameState::CANNON_MODE)
	{
		UpdateCannonMode(deltaSeconds);
	}
}
//..............................
void Game::UpdateFeatureMode(float deltaSeconds)
{
	Update_Ragdolls(deltaSeconds);

	if (DEBUG_DebugDraw)
	{
		DebugAddWorldBasis(Mat44(), 0);

		Mat44 textXTransform;
		textXTransform.AppendTranslation3D(Vec3(1.3f, 0.f, 0.2f));
		textXTransform.AppendZRotation(90);
		DebugAddWorldText("X - FORWARD", textXTransform, 0.2f, Vec2(0.5f, 0.5f), 0, Rgba8::COLOR_RED);
		Mat44 textYTransform;
		textYTransform.AppendTranslation3D(Vec3(0.f, 1.0f, 0.2f));
		DebugAddWorldText("Y - LEFT", textYTransform, 0.2f, Vec2(0.5f, 0.5f), 0, Rgba8::COLOR_GREEN);
		Mat44 textZTransform;
		textZTransform.AppendTranslation3D(Vec3(-0.4f, 0.0f, 1.0f));
		textZTransform.AppendYRotation(90);
		textZTransform.AppendZRotation(90);
		DebugAddWorldText("Z - UP", textZTransform, 0.2f, Vec2(0.5f, 0.5f), 0, Rgba8::COLOR_BLUE);
	}

	m_player->Update(deltaSeconds);

	DEBUG_previousDeltaSeconds = deltaSeconds;

	if (g_theInput->WasKeyJustPressed('H'))
	{
		Mat44 matrix = Mat44::CreateTranslation3D(Vec3(DEBUG_spawn_X, DEBUG_spawn_Y, DEBUG_spawn_Z));
		matrix.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		matrix.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		matrix.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		SpawnRagdoll(matrix, Rgba8::GetRandomColor(false), Vec3::ZERO);
	}

	if (DEBUG_raycast && !DEBUG_isCameraMode)
	{
		RaycastVsRagdolls();
	}
}

void Game::UpdatePachinkoMode(float deltaSeconds)
{
	//DEBUG_isCameraMode = false;

	Update_Ragdolls(deltaSeconds);

	if (DEBUG_demoMode)
	{
		if (m_reachDestination)
		{
			m_demoPachinkoCursorTargetPosition = Vec3(g_theRNG->RollRandomFloatInRange(-20.f, -8.f),
				g_theRNG->RollRandomFloatInRange(-24.f, 24.f),
				g_theRNG->RollRandomFloatInRange(75.f, 90.f));
			m_reachDestination = false;
			m_demoPachinkoTimer->Stop();
		}
		else
		{
			if (m_demoPachinkoTimer->IsStopped())
			{
				m_demoPachinkoTimer->Start();
			}

			if ((m_pachinkoCursor - m_demoPachinkoCursorTargetPosition).GetLengthSquared() < 0.2f)
			{
				Mat44 matrix = Mat44::CreateTranslation3D(m_pachinkoCursor);
				if (DEBUG_randomRotation)
				{
					matrix.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
					matrix.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
					matrix.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
				}

				SpawnRagdoll(matrix, Rgba8::COLOR_RAGDOLL_NODE, Vec3::ZERO);

				m_reachDestination = true;
			}
			else
			{
				m_pachinkoCursor = Interpolate(m_pachinkoCursor, m_demoPachinkoCursorTargetPosition, 2.f * SmoothStart2(m_demoPachinkoTimer->GetElapsedFraction()) * deltaSeconds);
				m_pachinkoCursor = Clamp(m_pachinkoCursor, Vec3(-20, -28, 65), Vec3(-8, 28, 90));
			}
		}
	}
	else
	{
		m_player->Update(deltaSeconds);

		if (!DEBUG_isCameraMode)
		{
			float speedCursor = 10.f;
			if (g_theInput->IsKeyDown('A'))
			{
				m_pachinkoCursor += Vec3(0, 1, 0) * -speedCursor * deltaSeconds;
			}
			if (g_theInput->IsKeyDown('D'))
			{
				m_pachinkoCursor += Vec3(0, 1, 0) * speedCursor * deltaSeconds;
			}
			if (g_theInput->IsKeyDown('W'))
			{
				m_pachinkoCursor += Vec3(1, 0, 0) * -speedCursor * deltaSeconds;
			}
			if (g_theInput->IsKeyDown('S'))
			{
				m_pachinkoCursor += Vec3(1, 0, 0) * speedCursor * deltaSeconds;
			}
			if (g_theInput->IsKeyDown('Q'))
			{
				m_pachinkoCursor += Vec3(0, 0, 1) * speedCursor * deltaSeconds;
			}
			if (g_theInput->IsKeyDown('E'))
			{
				m_pachinkoCursor += Vec3(0, 0, 1) * -speedCursor * deltaSeconds;
			}

			m_pachinkoCursor = Clamp(m_pachinkoCursor, Vec3(-20, -28, 65), Vec3(-8, 28, 90));

			if (g_theInput->WasKeyJustPressed(KEYCODE_SPACE))
			{
				Mat44 matrix = Mat44::CreateTranslation3D(m_pachinkoCursor);
				if (DEBUG_randomRotation)
				{
					matrix.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
					matrix.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
					matrix.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
				}

				SpawnRagdoll(matrix, Rgba8::COLOR_RAGDOLL_NODE, Vec3::ZERO);
			}
		}

	}


	if (DEBUG_raycast && !DEBUG_isCameraMode)
	{
		RaycastVsRagdolls();
	}
}

void Game::UpdateCannonMode(float deltaSeconds)
{
	Update_Ragdolls(deltaSeconds);

	m_player->Update(deltaSeconds);

	if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE) && DEBUG_isCameraMode)
	{
		m_cannonForce += CANNON_CHARGE_RATE * deltaSeconds;
		m_cannonForce = Clamp(m_cannonForce, 0.f, CANNON_CHARGE_LIMIT);

	}
	if (g_theInput->WasKeyJustReleased(KEYCODE_LEFT_MOUSE) && DEBUG_isCameraMode && m_secondIntoMode > 1.f)
	{
		Mat44 matrix = Mat44::CreateLookForward(-m_player->GetForward());
		matrix.SetTranslation3D(m_player->m_position);
		SpawnRagdoll(matrix, Rgba8::COLOR_RAGDOLL_NODE, m_player->GetForward() * m_cannonForce);
		m_cannonForce = 0.f;
	}
	if (g_theInput->WasKeyJustReleased(KEYCODE_RIGHT_MOUSE) && DEBUG_isCameraMode)
	{
		DEBUG_breakable = !DEBUG_breakable;
	}
}

//..............................
void Game::UpdateAttractMode(float deltaSeconds)
{
	DEBUG_isCameraMode = false;
	m_menuCanvas->Update(deltaSeconds);
}

void Game::Render() const
{
	if (m_currentState == GameState::ATTRACT_MODE)
	{
		RenderAttractMode();
	}
	if (m_currentState == GameState::FEATURE_MODE)
	{
		RenderFeatureMode();
		DebugRenderWorld(*m_player->GetCamera());
	}
	if (m_currentState == GameState::PACHINKO_MODE)
	{
		RenderPachinkoMode();
		DebugRenderWorld(*m_player->GetCamera());
	}
	if (m_currentState == GameState::CANNON_MODE)
	{
		RenderCannonMode();
		DebugRenderWorld(*m_player->GetCamera());
	}

	RenderScreenWorld();
	DebugRenderScreen(*m_screenCamera);
}
//..............................

void Game::RenderFeatureMode() const
{
	g_theRenderer->ClearScreen(Rgba8(70, 70, 70, 255));
	g_theRenderer->BeginCamera(*m_player->GetCamera());
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetLightConstants(m_sunOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D(), m_sunIntensity, m_ambIntensity, m_player->GetCamera()->m_position, 0.f, 0.1f, 0.f, 1.f);

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindShader(m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(m_planeTextureD, 0);
	g_theRenderer->BindTexture(m_planeTextureN, 1);
	g_theRenderer->BindTexture(m_planeTextureS, 2);
	g_theRenderer->DrawIndexedBuffer(m_planeVBO, m_planeIBO, m_planeIndexes.size(), 0, VertexType::Vertex_PCUTBN);

	DrawSkybox();

	if (DEBUG_DebugDraw)
	{
		DrawGrid();
	}

	for (auto& r : m_ragdolls)
	{
		r->Render();
	}

	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	for (auto fixedObject : m_fixedObjects)
	{
		fixedObject->Render();
	}

	if (DEBUG_DebugDraw && DEBUG_DebugDrawOctree)
	{
		m_octree->Render();
	}

	if (DEBUG_raycast)
	{
		std::vector<Vertex_PCU> raycast;
		if (m_ragdollRaycastResult.m_didImpact)
		{
			AddVertsForCylinder3D(raycast, m_ragdollRaycastResult.m_rayStartPos, m_ragdollRaycastResult.m_impactPos, 0.025f, Rgba8::COLOR_GREEN);
			AddVertsForSphere(raycast, m_ragdollRaycastResult.m_impactPos, 0.035f);
			AddVertsForArrow3D(raycast, m_ragdollRaycastResult.m_impactPos, m_ragdollRaycastResult.m_impactPos + m_ragdollRaycastResult.m_rayFwdNormal * (m_ragdollRaycastResult.m_rayMaxLength - m_ragdollRaycastResult.m_impactDist), 0.025f, Rgba8::COLOR_DARK_RED);
		}
		else
		{
			AddVertsForArrow3D(raycast, m_ragdollRaycastResult.m_rayStartPos, m_ragdollRaycastResult.m_rayStartPos + m_ragdollRaycastResult.m_rayFwdNormal * m_ragdollRaycastResult.m_rayMaxLength, 0.025f, Rgba8::COLOR_DARK_GRAY);
		}
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexArray(raycast.size(), raycast.data());
	}

	g_theRenderer->EndCamera(*m_player->GetCamera());
}

void Game::RenderPachinkoMode() const
{
	g_theRenderer->ClearScreen(Rgba8(70, 70, 70, 255));
	g_theRenderer->BeginCamera(*m_player->GetCamera());
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetLightConstants(m_sunOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D(), m_sunIntensity, m_ambIntensity, m_player->GetCamera()->m_position, 0.f, 0.1f, 0.f, 1.f);

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindShader(m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(m_planeTextureD, 0);
	g_theRenderer->BindTexture(m_planeTextureN, 1);
	g_theRenderer->BindTexture(m_planeTextureS, 2);
	g_theRenderer->DrawIndexedBuffer(m_planeVBO, m_planeIBO, m_planeIndexes.size(), 0, VertexType::Vertex_PCUTBN);

	DrawSkybox();

	for (auto& r : m_ragdolls)
	{
		r->Render();
	}

	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	for (auto fixedObject : m_fixedObjects)
	{
		fixedObject->Render();
	}

	if (DEBUG_DebugDraw && DEBUG_DebugDrawOctree)
	{
		m_octree->Render();
	}

	std::vector<Vertex_PCU> cursorVerts;
	AddVertsForSphere(cursorVerts, m_pachinkoCursor, 1.f, Rgba8::COLOR_RED);
	AddVertsForZCylinder3D(cursorVerts, Vec2(m_pachinkoCursor.x, m_pachinkoCursor.y), FloatRange(0.f, m_pachinkoCursor.z), 0.2f, 8, Rgba8(255, 0, 0, 100));
	AddVertsForDisc2D(cursorVerts, Vec2(m_pachinkoCursor), 1.f, Rgba8::COLOR_DARK_RED);
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);
	g_theRenderer->BindTexture(nullptr, 2);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(cursorVerts.size(), cursorVerts.data());


	if (DEBUG_raycast)
	{
		std::vector<Vertex_PCU> raycast;
		if (m_ragdollRaycastResult.m_didImpact)
		{
			AddVertsForCylinder3D(raycast, m_ragdollRaycastResult.m_rayStartPos, m_ragdollRaycastResult.m_impactPos, 0.025f, Rgba8::COLOR_GREEN);
			AddVertsForSphere(raycast, m_ragdollRaycastResult.m_impactPos, 0.035f);
			AddVertsForArrow3D(raycast, m_ragdollRaycastResult.m_impactPos, m_ragdollRaycastResult.m_impactPos + m_ragdollRaycastResult.m_rayFwdNormal * (m_ragdollRaycastResult.m_rayMaxLength - m_ragdollRaycastResult.m_impactDist), 0.025f, Rgba8::COLOR_DARK_RED);
		}
		else
		{
			AddVertsForArrow3D(raycast, m_ragdollRaycastResult.m_rayStartPos, m_ragdollRaycastResult.m_rayStartPos + m_ragdollRaycastResult.m_rayFwdNormal * m_ragdollRaycastResult.m_rayMaxLength, 0.025f, Rgba8::COLOR_DARK_GRAY);
		}
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexArray(raycast.size(), raycast.data());
	}

	g_theRenderer->EndCamera(*m_player->GetCamera());
}

void Game::RenderCannonMode() const
{
	g_theRenderer->ClearScreen(Rgba8(70, 70, 70, 255));
	g_theRenderer->BeginCamera(*m_player->GetCamera());
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetLightConstants(m_sunOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D(), m_sunIntensity, m_ambIntensity, m_player->GetCamera()->m_position, 0.f, 0.1f, 0.f, 1.f);

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindShader(m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(m_planeTextureD, 0);
	g_theRenderer->BindTexture(m_planeTextureN, 1);
	g_theRenderer->BindTexture(m_planeTextureS, 2);
	g_theRenderer->DrawIndexedBuffer(m_planeVBO, m_planeIBO, m_planeIndexes.size(), 0, VertexType::Vertex_PCUTBN);

	for (auto& r : m_ragdolls)
	{
		r->Render();
	}

	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	for (auto fixedObject : m_fixedObjects)
	{
		fixedObject->Render();
	}

	if (DEBUG_DebugDraw && DEBUG_DebugDrawOctree)
	{
		m_octree->Render();
	}

	DrawSkybox();

	g_theRenderer->EndCamera(*m_player->GetCamera());
}

//..............................
void Game::RenderAttractMode() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0, 255));
	g_theRenderer->BeginCamera(*m_screenCamera);

	g_theRenderer->BindShader(nullptr);
	m_menuCanvas->Render();

	g_theRenderer->EndCamera(*m_screenCamera);
}
//..............................
void Game::RenderScreenWorld() const
{
	g_theRenderer->BeginCamera(*m_screenCamera);
	g_theRenderer->SetDepthStencilMode(DepthMode::DISABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetSamplerMode(SampleMode::POINT_CLAMP);

	AABB2 screenBound = AABB2(m_screenCamera->GetOrthographicBottomLeft(), m_screenCamera->GetOrthographicTopRight());

	std::vector<Vertex_PCU> textVerts;

	std::vector<Vertex_PCU> UIVerts;
	UIVerts.reserve(6);

	if (m_currentState == GameState::CANNON_MODE)
	{
		Vec2 center = screenBound.GetCenter();
		Vec2 min = center - Vec2(70, 70);
		Vec2 max = center + Vec2(70, 70);
		std::vector<Vertex_PCU> reticleVerts;
		reticleVerts.reserve(6);
		AddVertsForAABB2D(reticleVerts, AABB2(min, max), Rgba8::DARK);
		g_theRenderer->SetModelConstants();
		g_theRenderer->BindTexture(m_reticleTexture);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->DrawVertexArray((int)reticleVerts.size(), reticleVerts.data());

		Vec2 bottom = center - Vec2(0, 65);
		Vec2 top = Vec2(center.x, RangeMapClamped(m_cannonForce, 0.f, CANNON_CHARGE_LIMIT, bottom.y, center.y));
		AddVertsForLineSegment2DWith2Colors(UIVerts, bottom, top, 5.f, Rgba8::COLOR_ORANGE, Rgba8::COLOR_RED);

		if (DEBUG_breakable)
		{
			g_theFont->AddVertsForText2D(textVerts, bottom - Vec2(80, 20), 10.f, "BREAK MODE : ON", Rgba8::COLOR_GREEN);
		}
		else
		{
			g_theFont->AddVertsForText2D(textVerts, bottom - Vec2(80, 20), 10.f, "BREAK MODE : OFF", Rgba8::COLOR_RED);
		}

	}

	if (m_clock->IsPaused())
	{
		AddVertsForAABB2D(UIVerts, screenBound, Rgba8::DARK);
	}

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(&g_theFont->GetTexture());
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(textVerts.size(), textVerts.data());

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray((int)UIVerts.size(), UIVerts.data());

	g_theDevConsole->Render(screenBound, g_theRenderer);
}

void Game::DrawSkybox() const
{
	std::vector<Vertex_PCU> skyBoxVerts;

	AddVertsForSkyBox(skyBoxVerts, AABB3(-1024, -1024, -1224, 1024, 1024, 824), Rgba8::COLOR_LIGHT_GRAY);

	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(m_skyboxTexture);
	g_theRenderer->BindTexture(nullptr, 1);
	g_theRenderer->BindTexture(nullptr, 2);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray((int)skyBoxVerts.size(), skyBoxVerts.data());
}

//..............................
void Game::Shutdown()
{
	for (auto& r : m_ragdolls)
	{
		delete r;
	}
	m_ragdolls.clear();

	for (auto& fixedObject : m_fixedObjects)
	{
		delete fixedObject;
	}

	m_allObjects.clear();
	m_fixedObjects.clear();

	if (m_octree) delete m_octree;
	m_octree = nullptr;
}

void Game::Restart()
{
	m_ragdollRaycastResult = RaycastRagdollResult3D();

	if (m_currentState == GameState::FEATURE_MODE)
	{
		FeatureModeRestart();
	}
	if (m_currentState == GameState::PACHINKO_MODE)
	{
		PachinkoModeRestart();
	}
	if (m_currentState == GameState::CANNON_MODE)
	{
		CannonModeRestart();
	}

	UpdateAllCurrentRagdolls();
}

void Game::Init_Ragdolls()
{
	for (auto& r : m_ragdolls)
	{
		if (r) delete r;
	}

	m_ragdolls.clear();

	Rgba8 color = Rgba8::COLOR_RAGDOLL_NODE;
	if (m_numberOfRagdolls > 1) color = Rgba8::COLOR_RED;

	Mat44 mainMatrix = Mat44::CreateTranslation3D(Vec3(DEBUG_spawn_X, DEBUG_spawn_Y, DEBUG_spawn_Z));
	mainMatrix.AppendZRotation(DEBUG_spawn_Yaw);
	mainMatrix.AppendYRotation(DEBUG_spawn_Pitch);
	mainMatrix.AppendXRotation(DEBUG_spawn_Roll);
	SpawnRagdoll(mainMatrix, color, Vec3::ZERO);

	for (size_t a = 0; a < m_numberOfRagdolls - 1; a++)
	{
		float x = g_theRNG->RollRandomFloatInRange(DEBUG_random_spawn_X.m_min, DEBUG_random_spawn_X.m_max);
		float y = g_theRNG->RollRandomFloatInRange(DEBUG_random_spawn_Y.m_min, DEBUG_random_spawn_Y.m_max);
		float z = g_theRNG->RollRandomFloatInRange(DEBUG_random_spawn_Z.m_min, DEBUG_random_spawn_Z.m_max);

		Mat44 matrix = Mat44::CreateTranslation3D(Vec3(x, y, z));

		matrix.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		matrix.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		matrix.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));

		SpawnRagdoll(matrix, Rgba8::GetRandomColor(false), Vec3::ZERO);
	}
}

void Game::Init_Octree()
{
	m_octree = new Octree(DoubleAABB3(-1024, -1024, -1024, 1024, 1024, 1024), m_allObjects);
	m_octree->m_isRoot = true;
	m_octree->Init();
	m_octree->UpdateTree();
}

void Game::Update_Ragdolls(float deltaSeconds)
{
	if (DEBUG_usingMultithreading)
	{
		ManagingRagdolls_Multi_Threaded(deltaSeconds);
	}
	else
	{
		ManagingRagdolls_Single_Threaded(deltaSeconds);
	}


	if (m_currentState == GameState::PACHINKO_MODE || m_currentState == GameState::CANNON_MODE)
	{
		if (m_ragdolls.size() > RAGDOLLS_LIMIT)
		{
			DeleteRagdoll(m_ragdolls.front());
		}
	}

	if (m_currentState == GameState::FEATURE_MODE)
	{
		if (!DEBUG_isCameraMode)
		{
			if (m_ragdolls.empty()) return;

			if (g_theInput->IsKeyDown('A'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(0, 1, 0) * -DEBUG_NodeMoveSpeed * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
			if (g_theInput->IsKeyDown('D'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(0, 1, 0) * DEBUG_NodeMoveSpeed * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
			if (g_theInput->IsKeyDown('W'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(1, 0, 0) * -DEBUG_NodeMoveSpeed * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
			if (g_theInput->IsKeyDown('S'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(1, 0, 0) * DEBUG_NodeMoveSpeed * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
			if (g_theInput->IsKeyDown('Q'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(0, 0, 1) * DEBUG_NodeMoveSpeed * 2 * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
			if (g_theInput->IsKeyDown('E'))
			{
				m_ragdolls[0]->ApplyGlobalAcceleration(DoubleVec3(0, 0, 1) * -DEBUG_NodeMoveSpeed * 2 * deltaSeconds);
				m_ragdolls[0]->m_deadTimer = DEBUG_ragdoll_deadTimer;
				m_ragdolls[0]->m_timeSinceSpawn = 0.f;
			}
		}
	}
}

void Game::ManagingRagdolls_Single_Threaded(float deltaSeconds)
{
	if (m_ragdolls.empty() || !m_octree) return;

	for (auto& r : m_ragdolls)
	{
		r->Update(deltaSeconds);

		if (DEBUG_allRagdollLiveForever)
		{
			r->m_isDead = false;
		}
	}

	m_timeDebt += deltaSeconds;
	while (m_timeDebt >= m_fixedTimeStep)
	{
		for (auto& r : m_ragdolls)
		{
			r->SolveOneIteration(m_fixedTimeStep);
		}

		m_octree->Update();

		m_timeDebt -= m_fixedTimeStep;
	}
}

void Game::ManagingRagdolls_Multi_Threaded(float deltaSeconds)
{
	if (m_ragdolls.empty() || !m_octree) return;

	// Process completed jobs from previous frames
	for (size_t i = 0; i < g_theJobSystem->GetNumCompletedJobs(); i++)
	{
		Job* job = g_theJobSystem->RetrieveJob();
		delete job;
	}

	size_t numActiveRagdolls = 0;
	for (auto& ragdoll : m_ragdolls)
	{
		if (!ragdoll) continue;

		ragdoll->Update(deltaSeconds);

		if (DEBUG_allRagdollLiveForever)
		{
			ragdoll->m_isDead = false;
		}

		if (!ragdoll->m_isDead)
		{
			numActiveRagdolls++;
		}
	}

	if (numActiveRagdolls == 0)
	{
		m_octree->Update();
		return;
	}

	bool useMultithreading = (numActiveRagdolls >= MULTITHREADING_THRESHOLD);
	int iterations = IntMax(1, (int)(deltaSeconds / m_fixedTimeStep));

	// Single-threaded 
	if (!useMultithreading)
	{
		m_timeDebt += deltaSeconds;
		while (m_timeDebt >= m_fixedTimeStep)
		{
			for (auto& r : m_ragdolls)
			{
				r->SolveOneIteration(m_fixedTimeStep);
			}

			m_octree->Update();
			m_timeDebt -= m_fixedTimeStep;
		}

		return;
	}

	//-------------------------------------------------------------
	// Multi-threaded 

	for (size_t i = 0; i < iterations; i++)
	{
		for (auto& ragdoll : m_ragdolls)
		{
			if (ragdoll && !ragdoll->m_isDead)
			{
				// Create job for this ragdoll with fixed timesteps
				RagdollPhysicsJob* job = new RagdollPhysicsJob(ragdoll, m_fixedTimeStep);
				g_theJobSystem->QueueJob(job);
			}
		}

		while (g_theJobSystem->GetNumCompletedJobs() > 0)
		{
			Job* job = g_theJobSystem->RetrieveJob();
			delete job;
		}

		m_octree->Update();
	}

}

void Game::IMGUI_UPDATE()
{
	ImVec4 activeColor(0.0f, 0.5f, 0.0f, 1.0f);      // Green color
	ImVec4 inactiveColor(0.5f, 0.0f, 0.0f, 1.0f);      // Red color

	ImGui::Begin("Ragdoll Control Panel");
	ImGui::SetWindowCollapsed(false, 2);
	ImGui::SetWindowSize(ImVec2(500, 800));
	ImGui::SeparatorText("App Stats");
	g_theApp->ShowFPSGraph();
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	ImGui::Text("Current DeltaSecond: %.3f", m_clock->GetDeltaSeconds());
	ImGui::Text("Total Physics Objects: %i", m_allObjects.size());
	ImGui::Text("Total Ragdolls: %i", m_ragdolls.size());
	int constraintNum = 0;
	for (auto& ragdoll : m_ragdolls)
	{
		constraintNum += (int)ragdoll->GetConstraints().size();
	}
	ImGui::Text("Total Constraints: %i", constraintNum);

	ImGui::Spacing();
	//if (DEBUG_usingMultithreading)
	//{
	//	ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	//}
	//else
	//{
	//	ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	//}
	//ImGui::Button("Using Multi-threading", ImVec2(250, 30));
	//if (ImGui::IsItemClicked(0))
	//{
	//	DEBUG_usingMultithreading = !DEBUG_usingMultithreading;
	//}
	//ImGui::PopStyleColor(1);

	if (m_currentState == GameState::FEATURE_MODE)
	{
		ImGui::SeparatorText("Control");
		ImGui::Spacing();
		ImGui::Text("WASD  to move the Ragdoll / Camera (Based On Cursor Mode)");
		ImGui::Text("QE    to elevate the Ragdoll / Camera (Based On Cursor Mode)");
		ImGui::Text("H	 to spawn ragdoll");
		ImGui::Text("R     to reset the scene");
		ImGui::Text("F1    to toggle Camera Mode");
		ImGui::Text("F2    to toggle Debug Draw");
		ImGui::Text("F3    to toggle Breakable Ragdoll");
		ImGui::Text("ESC   to back to the menu");
	}

	if (m_currentState == GameState::PACHINKO_MODE)
	{
		ImGui::SeparatorText("Control");
		ImGui::Spacing();
		ImGui::Text("WASD  to move the cursor");
		ImGui::Text("QE    to elevate the cursor");
		ImGui::Text("Space to spawn ragdoll");
		ImGui::Text("R     to randomize the scene");
		ImGui::Text("F1	to toggle Mouse Mode");
		ImGui::Text("F2    to toggle Debug Draw");
		ImGui::Text("F3    to toggle Breakable Ragdoll");
		ImGui::Text("ESC   to back to the menu");
	}

	if (m_currentState == GameState::CANNON_MODE)
	{
		ImGui::SeparatorText("Control");
		ImGui::Spacing();
		ImGui::Text("WASD			to move the Player");
		ImGui::Text("Hold LMB		to increase the velocity");
		ImGui::Text("Release LMB     to shoot ragdoll");
		ImGui::Text("R			   to randomize the scene");
		ImGui::Text("F1			  to toggle Mouse Mode");
		ImGui::Text("F2			  to toggle Debug Draw");
		ImGui::Text("F3			  to toggle Breakable Ragdoll");
		ImGui::Text("ESC			 to back to the menu");
	}


	ImGui::SeparatorText("General");
	if (m_currentState == GameState::PACHINKO_MODE)
	{
		ImGui::Text("Current Seed For Demo: %i", DEBUG_currentSeed);
		ImGui::PushItemWidth(185);
		ImGui::InputInt("New Seed (-1 = Best Seed, 0 = Random)", &DEBUG_desireSeed);
	}
	ImGui::Button("Restart", ImVec2(90, 30));
	if (ImGui::IsItemClicked(0))
	{
		Restart();
	}

	ImGui::SameLine();
	if (DEBUG_isCameraMode)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}
	ImGui::Button("Change Camera mode (F1)", ImVec2(170, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_isCameraMode = !DEBUG_isCameraMode;
	}
	ImGui::PopStyleColor(1);

	if (m_currentState == GameState::PACHINKO_MODE)
	{
		ImGui::SameLine();
		if (DEBUG_demoMode)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}

		ImGui::Button("Demo Mode (F9)", ImVec2(150, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_demoMode = !DEBUG_demoMode;
		}
		ImGui::PopStyleColor(1);
	}

	ImGui::SeparatorText("Time Scale");
	//----------
	if (m_clock->GetTimeScale() == 1.f)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}
	ImGui::Button("Normal", ImVec2(90, 30)); ImGui::SameLine();
	if (ImGui::IsItemClicked(0))
	{
		m_clock->SetTimeScale(1.f);
	}
	ImGui::PopStyleColor(1);
	//----------
	if (m_clock->GetTimeScale() == 0.3f)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}
	ImGui::Button("Slow-mo", ImVec2(90, 30)); ImGui::SameLine();
	if (ImGui::IsItemClicked(0))
	{
		m_clock->SetTimeScale(0.3f);
	}
	ImGui::PopStyleColor(1);
	//----------
	if (m_clock->GetTimeScale() == m_fixedTimeStep)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}
	ImGui::Button("One Frame", ImVec2(90, 30));
	if (ImGui::IsItemClicked(0))
	{
		m_clock->SetTimeScale(m_fixedTimeStep);
	}
	ImGui::PopStyleColor(1);

	if (m_currentState == GameState::FEATURE_MODE || m_currentState == GameState::PACHINKO_MODE)
	{
		ImGui::SeparatorText("Debug Raycast");

		if (DEBUG_raycast)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}

		ImGui::Button("Toggle Raycast [F4]", ImVec2(180, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_raycast = !DEBUG_raycast;
		}
		ImGui::PopStyleColor(1);

		if (m_ragdollRaycastResult.m_didImpact)
		{
			Node* n = m_ragdollRaycastResult.m_hitNode;
			if (n)
			{
				ImGui::Begin("Raycast Result");
				ImGui::SetWindowSize(ImVec2(350, 120));
				ImGui::Text("Node: %s", n->m_name.c_str());
				ImGui::Text("Total Constraints: %i", (int)m_ragdollRaycastResult.m_hitConstraints.size());
				ImGui::Text("Is Resting: %s", (n->m_isResting) ? "true" : "false");
				ImGui::Text("Total Energy: %.2f", GetTotalEnergy(n->m_velocity, n->m_mass, m_ragdolls[0]->m_config.gravAccel, n->m_position.z));
				if (DEBUG_wakeWithEnergy)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1), "(wake if above: %.2f)", DEBUG_energyThresholdExit);
				}
				ImGui::Text("Velocity Magnitude: %.2f", n->m_velocity.GetLength());
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1), "(rest if below: %.2f)", DEBUG_velocityThresholdExit);
				ImGui::End();
			}
		}
	}

	ImGui::SeparatorText("Debug Draw");

	if (DEBUG_DebugDraw)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}

	ImGui::Button("Render Debug (F2)", ImVec2(150, 30));
	if (ImGui::IsItemClicked(0))
	{
		ToggleDebugDraw();
	}
	ImGui::PopStyleColor(1);

	if (DEBUG_DebugDraw)
	{
		if (DEBUG_DebugDrawOctree)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}
		ImGui::Button("Draw Octree", ImVec2(150, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_DebugDrawOctree = !DEBUG_DebugDrawOctree;
		}
		ImGui::PopStyleColor(1);
		if (DEBUG_DebugDrawOctree)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.6f, 1), "Pink: Octree regions");
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1), "Cyan: Object's bounding box");
		}

		if (DEBUG_DebugDrawBasis)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}
		ImGui::Button("Draw Basis", ImVec2(150, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_DebugDrawBasis = !DEBUG_DebugDrawBasis;
		}
		ImGui::PopStyleColor(1);
		if (DEBUG_DebugDrawBasis)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.f, 0.f, 1), "Red: I Basis");
			ImGui::TextColored(ImVec4(0.f, 1.0f, 0.0f, 1), "Green: J Basis");
			ImGui::TextColored(ImVec4(0.f, 0.0f, 1.0f, 1), "Blue: K Basis");
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Gray: Node's Relation");
		}

		if (DEBUG_DebugDrawVel)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}
		ImGui::Button("Draw Velocities", ImVec2(150, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_DebugDrawVel = !DEBUG_DebugDrawVel;
		}
		ImGui::PopStyleColor(1);
		if (DEBUG_DebugDrawVel)
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1), "Yellow: Velocity");
			ImGui::TextColored(ImVec4(0.81f, 0.62f, 1.0f, 1), "Purple: Angular Velocity");
		}
	}

	// FEATURE MODE ______________________________________________________________________________________________
	if (m_currentState == GameState::FEATURE_MODE)
	{
		ImGui::SeparatorText("Ragdoll Spawn Setting");
		ImGui::PushItemWidth(75);
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Controllable Ragdoll Spawn Position");
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ImGui::InputFloat("X", &DEBUG_spawn_X, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Y", &DEBUG_spawn_Y, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Z", &DEBUG_spawn_Z, 0.0, 0.0, "%.2f");
		ImGui::InputFloat("Yaw", &DEBUG_spawn_Yaw, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Pitch", &DEBUG_spawn_Pitch, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Roll", &DEBUG_spawn_Roll, 0.0, 0.0, "%.2f");
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ImGui::TextColored(ImVec4(1, 0.5f, 0.7f, 1), "Uncontrollable Ragdoll Spawn Position Random Range");
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ImGui::InputFloat("X Min", &DEBUG_random_spawn_X.m_min, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("X Max", &DEBUG_random_spawn_X.m_max, 0.0, 0.0, "%.2f");
		ImGui::InputFloat("Y Min", &DEBUG_random_spawn_Y.m_min, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Y Max", &DEBUG_random_spawn_Y.m_max, 0.0, 0.0, "%.2f");
		ImGui::InputFloat("Z Min", &DEBUG_random_spawn_Z.m_min, 0.0, 0.0, "%.2f"); ImGui::SameLine();
		ImGui::InputFloat("Z Max", &DEBUG_random_spawn_Z.m_max, 0.0, 0.0, "%.2f");
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ImGui::InputInt("Number of Ragdoll", &m_numberOfRagdolls);
		ImGui::Dummy(ImVec2(0.0f, 2.0f));

		ImGui::TextColored(ImVec4(0.5, 0.5, 1.0, 1), "Ragdoll Type");
		ImGui::Button("Humanoid", ImVec2(70, 30)); ImGui::SameLine();
		if (ImGui::IsItemClicked(0))
		{
			m_ragdollDebugType = 0;
			Restart();
		}
		ImGui::Button("Debug", ImVec2(70, 30)); ImGui::SameLine();
		if (ImGui::IsItemClicked(0))
		{
			m_ragdollDebugType = 3;
			Restart();
		}
		//ImGui::Button("Sphere", ImVec2(70, 30)); ImGui::SameLine();
		//if (ImGui::IsItemClicked(0))
		//{
		//	m_ragdollDebugType = 1;
		//	Restart();
		//}
		//ImGui::Button("Capsule", ImVec2(70, 30));
		//if (ImGui::IsItemClicked(0))
		//{
		//	m_ragdollDebugType = 2;
		//	Restart();
		//}

		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		ImGui::Button("Spawn", ImVec2(150, 30)); ImGui::SameLine();
		if (ImGui::IsItemClicked(0))
		{
			Restart();
		}
		ImGui::Button("Drop 1 Ragdoll (H)", ImVec2(150, 30));
		if (ImGui::IsItemClicked(0))
		{
			SpawnRagdoll(Mat44::CreateTranslation3D(Vec3(DEBUG_spawn_X, DEBUG_spawn_Y, DEBUG_spawn_Z)), Rgba8::GetRandomColor(false), Vec3::ZERO);
		}
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
	}

	if (m_currentState == GameState::PACHINKO_MODE)
	{
		ImGui::SeparatorText("Ragdoll Spawn Setting");
		ImGui::PushItemWidth(75);

		if (DEBUG_randomRotation)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
		}

		ImGui::Button("Spawn With Random Rotation", ImVec2(250, 30));
		if (ImGui::IsItemClicked(0))
		{
			DEBUG_randomRotation = !DEBUG_randomRotation;
		}
		ImGui::PopStyleColor(1);
	}
	//  ______________________________________________________________________________________________

	ImGui::SeparatorText("Ragdoll Setting");
	ImGui::PushItemWidth(75);

	ImGui::InputFloat("Ragdoll Dead Timer", &DEBUG_ragdoll_deadTimer, 0.0, 0.0, "%.2f");
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	ImGui::InputFloat("Ragdoll Can Rest Timer", &DEBUG_ragdoll_canRestTimer, 0.0, 0.0, "%.2f");
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	if (DEBUG_allRagdollLiveForever)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}

	ImGui::Button("All Ragdoll Alive Forever", ImVec2(250, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_allRagdollLiveForever = !DEBUG_allRagdollLiveForever;

		if (!DEBUG_allRagdollLiveForever)
		{
			for (auto& r : m_ragdolls)
			{
				if (r->m_deadTimer <= 1.f)
				{
					r->m_deadTimer = 1.f;
				}
			}
		}
	}
	ImGui::PopStyleColor(1);

	if (DEBUG_gameObjectCanRest)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}
	ImGui::Button("All Game Object Can Rest", ImVec2(250, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_gameObjectCanRest = !DEBUG_gameObjectCanRest;
	}
	ImGui::PopStyleColor(1);

	if (DEBUG_wakeWithEnergy)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}

	ImGui::Button("Can Wake with Energy Threshold", ImVec2(250, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_wakeWithEnergy = !DEBUG_wakeWithEnergy;
	}
	ImGui::PopStyleColor(1);

	if (DEBUG_solveConstraintWithFixedIteration)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}

	ImGui::Button("Solve Constraints With Fixed Iteration", ImVec2(300, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_solveConstraintWithFixedIteration = !DEBUG_solveConstraintWithFixedIteration;

		for (auto& ragdoll : m_ragdolls)
		{
			ragdoll->DEBUG_solveConstraintWithFixedIteration = DEBUG_solveConstraintWithFixedIteration;
		}
	}
	ImGui::PopStyleColor(1);

	if (DEBUG_breakable)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
	}

	ImGui::Button("Breakable Ragdoll (F3)", ImVec2(250, 30));
	if (ImGui::IsItemClicked(0))
	{
		DEBUG_breakable = !DEBUG_breakable;

		for (auto& r : m_ragdolls)
		{
			r->m_isBreakable = DEBUG_breakable;
		}
	}
	ImGui::PopStyleColor(1);


	ImGui::SeparatorText("Other Configurations");
	if (m_currentState == GameState::FEATURE_MODE)
	{
		ImGui::InputDouble("Debug Move Speed", &DEBUG_NodeMoveSpeed, 0.0, 0.0, "%.2f");
	}
	ImGui::InputFloat("Force Threshold Exit Resting", &DEBUG_forceThresholdExit, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Constraint Loop Num", &DEBUG_constraintNumLoop, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Velocity Threshold Exit Resting", &DEBUG_velocityThresholdExit, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Energy Threshold Exit Resting", &DEBUG_energyThresholdExit, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Max Velocity Value XYZ", &DEBUG_maxVelocity, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Position Constraint Fix Rate", &DEBUG_posFixRate, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Angle Constraint Fix Rate", &DEBUG_angleFixRate, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Coefficient of Restitution", &DEBUG_restitution, 0.0, 0.0, "%.2f");
	DEBUG_restitution = Clamp(DEBUG_restitution, 0.f, 1.f);
	ImGui::InputDouble("Max Friction", &DEBUG_maxFriction, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Collision Velocity Threshold", &DEBUG_contactCollisionThreshold, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Spring Stiffness", &DEBUG_springStiffness, 0.0, 0.0, "%.2f");
	ImGui::InputDouble("Damping", &DEBUG_damping, 0.0, 0.0, "%.2f");
	ImGui::Text("Current Time Step: %.3f", m_fixedTimeStep);
	float newTimeStep = (float)TIME_STEP;
	ImGui::InputFloat("New Fixed Time Step", &newTimeStep);
	ImGui::Spacing();

	ImGui::Button("Apply", ImVec2(150, 30));
	if (ImGui::IsItemClicked(0))
	{
		if (m_numberOfRagdolls < 0) m_numberOfRagdolls = 0;
		m_fixedTimeStep = newTimeStep;
		Restart();
	}


	// DATA
	if (m_currentState == GameState::FEATURE_MODE)
	{
		ImGui::SeparatorText("Data");
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Resting Data"))
		{
			for (auto& n : m_ragdolls[0]->GetNodeList())
			{
				bool rest = n->m_isResting;
				if (rest)
				{
					ImGui::Text("Nodes [%s] is resting", n->m_name.c_str());
				}
				else
				{
					ImGui::Text("Nodes [%s] is NOT resting", n->m_name.c_str());
				}

			}
		}
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Position Data"))
		{
			for (auto& n : m_ragdolls[0]->GetNodeList())
			{
				DoubleVec3 p = n->m_position;
				ImGui::Text("Nodes [%s] Position: %.1f, %.1f, %.1f", n->m_name.c_str(), p.x, p.y, p.z);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Rotation Euler Data"))
		{
			for (auto& n : m_ragdolls[0]->GetNodeList())
			{
				EulerAngles e = n->GetModelMatrix().GetEulerAngle();
				ImGui::Text("Nodes [%s] Rotation: %.1f, %.1f, %.1f", n->m_name.c_str(), e.m_yawDegrees, e.m_pitchDegrees, e.m_rollDegrees);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Rotation Quat Data"))
		{
			for (auto& n : m_ragdolls[0]->GetNodeList())
			{
				Quaternion q = n->m_orientation;
				ImGui::Text("Nodes [%s] Rotation: %.1f, %.1f, %.1f, %.1f", n->m_name.c_str(), q.w, q.i, q.j, q.k);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Velocity Data"))
		{
			for (int i = 0; i < m_ragdolls[0]->GetNodeList().size(); i++)
			{
				DoubleVec3 v = m_ragdolls[0]->GetNodeList()[i]->m_velocity;
				ImGui::Text("Nodes [%s] Velocity: %.1f, %.1f, %.1f", m_ragdolls[0]->GetNodeName(i).c_str(), v.x, v.y, v.z);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Angular Velocity Data"))
		{
			for (int i = 0; i < m_ragdolls[0]->GetNodeList().size(); i++)
			{
				DoubleVec3 v = m_ragdolls[0]->GetNodeList()[i]->m_angularVelocity;
				ImGui::Text("Nodes [%s] Angular Velocity: %.1f, %.1f, %.1f", m_ragdolls[0]->GetNodeName(i).c_str(), v.x, v.y, v.z);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Net Force Data"))
		{
			for (int i = 0; i < m_ragdolls[0]->GetNodeList().size(); i++)
			{
				DoubleVec3 v = m_ragdolls[0]->GetNodeList()[i]->m_netForce;
				ImGui::Text("Nodes [%s] Net Force: %.1f, %.1f, %.1f", m_ragdolls[0]->GetNodeName(i).c_str(), v.x, v.y, v.z);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Dead State"))
		{
			for (int i = 0; i < m_ragdolls.size(); i++)
			{
				std::string result = (m_ragdolls[i]->m_isDead) ? "true" : "false";
				ImGui::Text("Ragdoll [%i] is dead: %s", i, result.c_str());
				//ImGui::Text("Ragdoll [%i] Total energy: %.2f", i, m_ragdolls[i]->m_totalEnergy);
				ImGui::Text("Ragdoll [%i] Dead Timer: %.2f", i, m_ragdolls[i]->m_deadTimer);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Octree Data"))
		{
			for (auto& allObject : m_allObjects)
			{
				if (allObject->m_isNode)
				{
					int layer = -1;
					if (allObject->m_currentOctree) layer = allObject->m_currentOctree->m_layer;
					ImGui::Text("Nodes [%s] Octree Layer: %i", ((Node*)allObject)->m_name.c_str(), layer);
				}

			}
		}
	}

	ImGui::End();
}

void Game::DrawGrid() const
{
	// Drawing Grid

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	std::vector<Vertex_PCU> gridVertexes;

	float smallSize = 0.02f;
	float medSize = 0.03f;
	float largeSize = 0.06f;

	for (int x = -50; x <= 50; x++)
	{
		AABB3 cube;
		Rgba8 color;

		if (x % 5 == 0)
		{
			cube = AABB3(-medSize + x, -50.f, -medSize, medSize + x, 50.f, medSize);
			color = Rgba8(0, 180, 0);
		}
		else
		{
			cube = AABB3(-smallSize + x, -50.f, -smallSize, smallSize + x, 50.f, smallSize);
			color = Rgba8::COLOR_GRAY;
		}
		if (x == 0)
		{
			cube = AABB3(-largeSize + x, -50.f, -largeSize, largeSize + x, 50.f, largeSize);
			color = Rgba8::COLOR_GREEN;
		}
		AddVertsForAABB3D(gridVertexes, cube, color);
	}
	for (int y = -50; y <= 50; y++)
	{
		AABB3 cube;
		Rgba8 color;

		if (y % 5 == 0)
		{
			cube = AABB3(-50.f, -medSize + y, -medSize, 50.f, medSize + y, medSize);
			color = Rgba8(180, 0, 0);
		}
		else
		{
			cube = AABB3(-50.f, -smallSize + y, -smallSize, 50.f, smallSize + y, smallSize);
			color = Rgba8::COLOR_GRAY;
		}
		if (y == 0)
		{
			cube = AABB3(-50.f, -largeSize + y, -largeSize, 50.f, largeSize + y, largeSize);
			color = Rgba8::COLOR_RED;
		}
		AddVertsForAABB3D(gridVertexes, cube, color);
	}
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray((int)gridVertexes.size(), gridVertexes.data());
}

void Game::ToggleDebugDraw()
{
	DEBUG_DebugDraw = !DEBUG_DebugDraw;
	DEBUG_DebugDrawOctree = DEBUG_DebugDraw;
	DEBUG_DebugDrawBasis = DEBUG_DebugDraw;
	DEBUG_DebugDrawVel = DEBUG_DebugDraw;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// GAME RESTART

void Game::FeatureModeRestart()
{
	EulerAngles playerRotation;
	Vec3 playerPosition;
	if (m_player && m_previousState == GameState::FEATURE_MODE)
	{
		playerRotation = m_player->m_orientationDegrees;
		playerPosition = m_player->m_position;
	}
	else
	{
		playerRotation = EulerAngles(180.f, 40.f, 0.f);
		playerPosition = Vec3(25.f, 0.f, 20.f);
	}

	bool debug = DEBUG_DebugDraw;

	if (m_player) delete m_player;
	m_player = new Player(playerPosition, playerRotation);

	Vec3 playerPos = m_player->m_position;
	EulerAngles playerRot = m_player->m_orientationDegrees;

	double speed = DEBUG_NodeMoveSpeed;

	double maxVel = (!m_ragdolls.empty()) ? m_ragdolls[0]->DEBUG_maxVelocity : 200;
	double posFix = (!m_ragdolls.empty()) ? m_ragdolls[0]->DEBUG_posFixRate : 20;
	double angleFix = (!m_ragdolls.empty()) ? m_ragdolls[0]->DEBUG_angleFixRate : 5;
	float timeStep = m_fixedTimeStep;

	Shutdown();

	Object_AABB* aabb1 = new Object_AABB(this, DoubleAABB3(-10, -10, 0, -5, -5, 5));
	aabb1->m_textureD = m_stoneTextureD;
	aabb1->m_textureN = m_stoneTextureN;
	aabb1->m_textureS = m_stoneTextureS;
	Object_AABB* aabb2 = new Object_AABB(this, DoubleAABB3(-19, -10, 0, -14, -5, 5));
	aabb2->m_textureD = m_stoneTextureD;
	aabb2->m_textureN = m_stoneTextureN;
	aabb2->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(aabb1);
	m_fixedObjects.push_back(aabb2);
	Mat44 obbMat;
	obbMat.AppendXRotation(45);
	obbMat.AppendYRotation(10);
	obbMat.AppendZRotation(55);
	Object_OBB* obb = new Object_OBB(this, DoubleOBB3(Vec3(8.5, 8.5, 5), obbMat.GetIBasis3D(), obbMat.GetJBasis3D(), Vec3(2.5, 2.5, 3)));
	obb->m_textureD = m_stoneTextureD;
	obb->m_textureN = m_stoneTextureN;
	obb->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(obb);
	Object_Sphere* sphere = new Object_Sphere(this, DoubleVec3(7, -7, 3), 3);
	sphere->m_textureD = m_stoneTextureD;
	sphere->m_textureN = m_stoneTextureN;
	sphere->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(sphere);
	Object_Capsule* capsule = new Object_Capsule(this, DoubleCapsule3(Vec3(-5, 5, 3), Vec3(-8, 10, 7), 2.f));
	capsule->m_textureD = m_stoneTextureD;
	capsule->m_textureN = m_stoneTextureN;
	capsule->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(capsule);

	m_allObjects.push_back(aabb1);
	m_allObjects.push_back(aabb2);
	m_allObjects.push_back(obb);
	m_allObjects.push_back(sphere);
	m_allObjects.push_back(capsule);

	Init_Ragdolls();
	Init_Octree();

	m_player->m_position = playerPos;
	m_player->m_orientationDegrees = playerRot;

	DEBUG_NodeMoveSpeed = speed;

	if (!m_ragdolls.empty())
	{
		m_ragdolls[0]->DEBUG_maxVelocity = maxVel;
		m_ragdolls[0]->DEBUG_posFixRate = posFix;
		m_ragdolls[0]->DEBUG_angleFixRate = angleFix;
	}

	m_fixedTimeStep = timeStep;
	DEBUG_DebugDraw = debug;
}

void Game::PachinkoModeRestart()
{
	if (DEBUG_desireSeed == 0)
	{
		g_theRNG->m_seed = g_theRNG->RollRandomIntInRange(0, INT_MAX);
	}
	else
	{
		g_theRNG->m_seed = DEBUG_desireSeed;
	}
	if (DEBUG_desireSeed <= -1)
	{
		g_theRNG->m_seed = 516307273;
	}

	g_theRNG->m_position = 0;
	DEBUG_currentSeed = g_theRNG->m_seed;


	if (m_demoPachinkoTimer) delete m_demoPachinkoTimer;
	m_demoPachinkoTimer = new Timer(2.f, m_clock);

	EulerAngles playerRotation;
	Vec3 playerPosition;
	if (m_player && m_previousState == GameState::PACHINKO_MODE)
	{
		playerRotation = m_player->m_orientationDegrees;
		playerPosition = m_player->m_position;
	}
	else
	{
		playerRotation = EulerAngles(180.f, 20.f, 0.f);
		playerPosition = Vec3(75.f, 0.f, 75.f);
	}

	bool debug = DEBUG_DebugDraw;

	if (m_player) delete m_player;
	m_player = new Player(playerPosition, playerRotation);

	Shutdown();

	Object_AABB* wallBehind = new Object_AABB(this, DoubleAABB3(-31, -30, 0, -30, 30, 70));
	wallBehind->m_textureD = m_stoneTextureD;
	wallBehind->m_textureN = m_stoneTextureN;
	wallBehind->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(wallBehind);
	m_allObjects.push_back(wallBehind);

	Object_AABB* wallLeft = new Object_AABB(this, DoubleAABB3(-30, -30, 0, -5, -29, 70));
	wallLeft->m_textureD = m_stoneTextureD;
	wallLeft->m_textureN = m_stoneTextureN;
	wallLeft->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(wallLeft);
	m_allObjects.push_back(wallLeft);

	Object_AABB* wallRight = new Object_AABB(this, DoubleAABB3(-30, 30, 0, -5, 31, 70));
	wallRight->m_textureD = m_stoneTextureD;
	wallRight->m_textureN = m_stoneTextureN;
	wallRight->m_textureS = m_stoneTextureS;
	m_fixedObjects.push_back(wallRight);
	m_allObjects.push_back(wallRight);

	//Object_AABB* wallFront = new Object_AABB(this, DoubleAABB3(-5, -30, 0, -4, 30, 70));
	//wallFront->m_textureD = m_transparentTexture;
	//wallFront->m_textureN = m_transparentTexture;
	//wallFront->m_textureS = m_transparentTexture;
	//m_fixedObjects.push_back(wallFront);
	//m_allObjects.push_back(wallFront);

	for (int i = 0; i < 5; i++)
	{
		DoubleVec3 center = DoubleVec3(g_theRNG->RollRandomFloatInRange(-20, -10), g_theRNG->RollRandomFloatInRange(-25, 25), g_theRNG->RollRandomFloatInRange(5, 60));
		DoubleVec3 halfD = DoubleVec3(g_theRNG->RollRandomFloatInRange(2, 5), g_theRNG->RollRandomFloatInRange(2, 5), g_theRNG->RollRandomFloatInRange(2, 5));

		Object_AABB* aabb = new Object_AABB(this, DoubleAABB3(center, halfD.z, halfD.x, halfD.y));
		aabb->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(aabb);
		m_allObjects.push_back(aabb);
	}

	for (int i = 0; i < 5; i++)
	{
		Mat44 obb_mat;
		obb_mat.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		obb_mat.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		obb_mat.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		obb_mat.SetTranslation3D(DoubleVec3(g_theRNG->RollRandomFloatInRange(-20, -10), g_theRNG->RollRandomFloatInRange(-25, 25), g_theRNG->RollRandomFloatInRange(5, 60)));
		DoubleVec3 obb_halfD = DoubleVec3(g_theRNG->RollRandomFloatInRange(2, 5), g_theRNG->RollRandomFloatInRange(2, 5), g_theRNG->RollRandomFloatInRange(2, 5));

		Object_OBB* obb = new Object_OBB(this, DoubleOBB3(obb_mat.GetTranslation3D(), obb_mat.GetIBasis3D(), obb_mat.GetJBasis3D(), obb_halfD));
		obb->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(obb);
		m_allObjects.push_back(obb);
	}

	for (int i = 0; i < 5; i++)
	{
		Object_Sphere* sphere = new Object_Sphere(this,
			DoubleVec3(g_theRNG->RollRandomFloatInRange(-20, -10), g_theRNG->RollRandomFloatInRange(-25, 25), g_theRNG->RollRandomFloatInRange(5, 60)),
			g_theRNG->RollRandomFloatInRange(2, 5));
		sphere->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(sphere);
		m_allObjects.push_back(sphere);
	}

	for (int i = 0; i < 5; i++)
	{
		DoubleVec3 capsuleStart = DoubleVec3(g_theRNG->RollRandomFloatInRange(-20, -10), g_theRNG->RollRandomFloatInRange(-25, 25), g_theRNG->RollRandomFloatInRange(5, 60));
		Mat44 capsule_mat;
		capsule_mat.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		capsule_mat.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		capsule_mat.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		DoubleVec3 capsuleEnd = capsuleStart + capsule_mat.GetIBasis3D() * g_theRNG->RollRandomFloatInRange(2, 5);

		Object_Capsule* capsule = new Object_Capsule(this, DoubleCapsule3(capsuleStart, capsuleEnd, g_theRNG->RollRandomFloatInRange(2, 5)));
		capsule->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(capsule);
		m_allObjects.push_back(capsule);
	}

	Init_Octree();

	DEBUG_DebugDraw = debug;
}

void Game::CannonModeRestart()
{
	DEBUG_isCameraMode = true;

	EulerAngles playerRotation;
	Vec3 playerPosition;
	if (m_player && m_previousState == GameState::CANNON_MODE)
	{
		playerRotation = m_player->m_orientationDegrees;
		playerPosition = m_player->m_position;
	}
	else
	{
		playerRotation = EulerAngles(180.f, 20.f, 0.f);
		playerPosition = Vec3(75.f, 0.f, 10.f);
	}

	bool debug = DEBUG_DebugDraw;

	if (m_player) delete m_player;
	m_player = new Player(playerPosition, playerRotation);
	Shutdown();

	float shapeLimit = 10;
	float spawnlimit = 300;
	float deadZone = 150;
	for (int i = 0; i < shapeLimit; i++)
	{
		float x = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.x - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.x + deadZone, spawnlimit));
		float y = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.y - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.y + deadZone, spawnlimit));
		DoubleVec3 center = DoubleVec3(x, y, g_theRNG->RollRandomFloatInRange(5, 60));
		DoubleVec3 halfD = DoubleVec3(g_theRNG->RollRandomFloatInRange(5, 20), g_theRNG->RollRandomFloatInRange(5, 20), g_theRNG->RollRandomFloatInRange(5, 20));

		Object_AABB* aabb = new Object_AABB(this, DoubleAABB3(center, halfD.z, halfD.x, halfD.y));
		aabb->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(aabb);
		m_allObjects.push_back(aabb);
	}

	for (int i = 0; i < shapeLimit; i++)
	{
		Mat44 obb_mat;
		obb_mat.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		obb_mat.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		obb_mat.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		float x = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.x - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.x + deadZone, spawnlimit));
		float y = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.y - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.y + deadZone, spawnlimit));
		obb_mat.SetTranslation3D(DoubleVec3(x, y, g_theRNG->RollRandomFloatInRange(5, 60)));
		DoubleVec3 obb_halfD = DoubleVec3(g_theRNG->RollRandomFloatInRange(5, 20), g_theRNG->RollRandomFloatInRange(5, 20), g_theRNG->RollRandomFloatInRange(5, 20));

		Object_OBB* obb = new Object_OBB(this, DoubleOBB3(obb_mat.GetTranslation3D(), obb_mat.GetIBasis3D(), obb_mat.GetJBasis3D(), obb_halfD));
		obb->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(obb);
		m_allObjects.push_back(obb);
	}

	for (int i = 0; i < shapeLimit; i++)
	{
		float x = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.x - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.x + deadZone, spawnlimit));
		float y = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.y - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.y + deadZone, spawnlimit));
		DoubleVec3 center = DoubleVec3(x, y, g_theRNG->RollRandomFloatInRange(5, 60));
		Object_Sphere* sphere = new Object_Sphere(this, center, g_theRNG->RollRandomFloatInRange(5, 20));
		sphere->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(sphere);
		m_allObjects.push_back(sphere);
	}

	for (int i = 0; i < shapeLimit; i++)
	{
		float x = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.x - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.x + deadZone, spawnlimit));
		float y = g_theRNG->RollRandomFloatInRange(g_theRNG->RollRandomFloatInRange(-spawnlimit, playerPosition.y - deadZone), g_theRNG->RollRandomFloatInRange(playerPosition.y + deadZone, spawnlimit));
		DoubleVec3 capsuleStart = DoubleVec3(x, y, g_theRNG->RollRandomFloatInRange(5, 60));

		Mat44 capsule_mat;
		capsule_mat.AppendXRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		capsule_mat.AppendYRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		capsule_mat.AppendZRotation(g_theRNG->RollRandomFloatInRange(0, 360));
		DoubleVec3 capsuleEnd = capsuleStart + capsule_mat.GetIBasis3D() * g_theRNG->RollRandomFloatInRange(10, 25);

		Object_Capsule* capsule = new Object_Capsule(this, DoubleCapsule3(capsuleStart, capsuleEnd, g_theRNG->RollRandomFloatInRange(5, 20)));
		capsule->m_color = Rgba8::GetRandomColor(false);
		m_fixedObjects.push_back(capsule);
		m_allObjects.push_back(capsule);
	}


	Init_Octree();

	DEBUG_DebugDraw = debug;
}

void Game::SpawnRagdoll(Mat44 transform, Rgba8 color, Vec3 initialVelocity)
{
	VerletConfig config;

	Ragdoll* newR = new Ragdoll(this, transform, DEBUG_ragdoll_deadTimer, config, m_ragdollDebugType, color, Rgba8::GetDarkerColor(color, 0.3f));
	newR->m_deadTimer = DEBUG_ragdoll_deadTimer;
	newR->m_isBreakable = DEBUG_breakable;
	newR->DEBUG_solveConstraintWithFixedIteration = DEBUG_solveConstraintWithFixedIteration;

	newR->ApplyGlobalImpulseOnRoot(initialVelocity);
	newR->DEBUG_maxVelocity = DEBUG_maxVelocity;
	newR->DEBUG_posFixRate = DEBUG_posFixRate;
	newR->DEBUG_angleFixRate = DEBUG_angleFixRate;

	m_ragdolls.push_back(newR);

	if (m_octree)
	{
		for (size_t x = 0; x < newR->GetNodeList().size(); x++)
		{
			m_allObjects.push_back(newR->GetNodeList()[x]);
			m_octree->m_pendingInsertion.push_back(newR->GetNodeList()[x]);
		}
		m_octree->UpdateTree();
	}
	else
	{
		for (size_t x = 0; x < newR->GetNodeList().size(); x++)
		{
			m_allObjects.push_back(newR->GetNodeList()[x]);
		}
	}

}

void Game::DeleteRagdoll(Ragdoll* r)
{
	for (size_t i = 0; i < r->GetNodeList().size(); i++)
	{
		auto findNode = std::find(m_allObjects.begin(), m_allObjects.end(), r->GetNodeList()[i]);
		m_allObjects.erase(findNode);
	}

	auto findRagdoll = std::find(m_ragdolls.begin(), m_ragdolls.end(), r);
	m_ragdolls.erase(findRagdoll);
	delete r;

	delete m_octree;
	Init_Octree();
}

void Game::Menu_Init()
{
	m_menuCanvas = new Canvas(g_UI, m_screenCamera);

	IntVec2 screenSize = Window::GetMainWindowInstance()->GetClientDimensions();
	float aspect = Window::GetMainWindowInstance()->GetAspect();

	TextSetting titleTextSetting1 = TextSetting("Ragdoll Physics\nArtifact");
	titleTextSetting1.m_color = Rgba8::COLOR_WHITE;
	titleTextSetting1.m_height = 80.f * aspect;
	Vec2 titlePos = Vec2(screenSize.x * 0.5f, screenSize.y * 0.85f);
	m_title1 = new Text(m_menuCanvas, titlePos, titleTextSetting1, nullptr);

	TextSetting titleTextSetting2 = TextSetting("by Son Nguyen");
	titleTextSetting2.m_color = Rgba8::COLOR_GREEN;
	titleTextSetting2.m_height = 30.f * aspect;
	m_title2 = new Text(m_menuCanvas, titlePos + Vec2(0.f, -250.f), titleTextSetting2, nullptr);

	TextSetting featureTS = TextSetting("Feature Mode");
	featureTS.m_color = Rgba8::COLOR_WHITE;
	featureTS.m_height = 22.f * aspect;
	m_featureMode = new Button(m_menuCanvas, AABB2(screenSize.x * 0.5f - 200, 300, screenSize.x * 0.5f + 200, 390), featureTS, Rgba8::COLOR_DARK_GRAY, Rgba8::COLOR_DARK_GREEN, Rgba8::COLOR_WHITE, true, Rgba8::COLOR_BLACK, nullptr);
	m_featureMode->m_textColorHover = Rgba8::COLOR_WHITE;
	m_featureMode->OnClickEvent([this]() {SwitchState(GameState::FEATURE_MODE); });

	TextSetting pachinkoTS = TextSetting("Pachinko Mode");
	pachinkoTS.m_color = Rgba8::COLOR_WHITE;
	pachinkoTS.m_height = 22.f * aspect;
	m_featureMode = new Button(m_menuCanvas, AABB2(screenSize.x * 0.5f - 200, 200, screenSize.x * 0.5f + 200, 290), pachinkoTS, Rgba8::COLOR_DARK_GRAY, Rgba8::COLOR_DARK_GREEN, Rgba8::COLOR_WHITE, true, Rgba8::COLOR_BLACK, nullptr);
	m_featureMode->m_textColorHover = Rgba8::COLOR_WHITE;
	m_featureMode->OnClickEvent([this]() {SwitchState(GameState::PACHINKO_MODE); });

	TextSetting gameTS = TextSetting("Cannon Game Mode");
	gameTS.m_color = Rgba8::COLOR_WHITE;
	gameTS.m_height = 22.f * aspect;
	m_featureMode = new Button(m_menuCanvas, AABB2(screenSize.x * 0.5f - 200, 100, screenSize.x * 0.5f + 200, 190), gameTS, Rgba8::COLOR_DARK_GRAY, Rgba8::COLOR_DARK_GREEN, Rgba8::COLOR_WHITE, true, Rgba8::COLOR_BLACK, nullptr);
	m_featureMode->m_textColorHover = Rgba8::COLOR_WHITE;
	m_featureMode->OnClickEvent([this]() {SwitchState(GameState::CANNON_MODE); });
}

void Game::RaycastVsRagdolls()
{
	if (m_ragdolls.empty()) return;
	IntVec2 clientDim = Window::GetMainWindowInstance()->GetClientDimensions();
	Vec2 mousePos = g_theInput->GetCursorNormalizedPosition() * Vec2((float)clientDim.x, (float)clientDim.y);
	RaycastRagdollResult3D closestRagdollNodeHit;

	for (auto& ragdoll : m_ragdolls)
	{
		RaycastRagdollResult3D result = MouseRaycastVsRagdollNode(m_player->GetCamera(), mousePos, ragdoll);
		if (result.m_didImpact)
		{
			if (!closestRagdollNodeHit.m_didImpact)
			{
				closestRagdollNodeHit = result;
			}
			else
			{
				if (result.m_impactDist < closestRagdollNodeHit.m_impactDist)
				{
					closestRagdollNodeHit = result;
				}
			}
		}

	}

	m_ragdollRaycastResult = closestRagdollNodeHit;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// HANDLE INPUT

void Game::HandleInput()
{
	g_theInput->SetCursorMode(DEBUG_isCameraMode, DEBUG_isCameraMode);

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE))
	{
		if (m_currentState != GameState::ATTRACT_MODE)
		{
			g_theGame->SwitchState(GameState::ATTRACT_MODE);
		}
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		DEBUG_isCameraMode = !DEBUG_isCameraMode;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F2))
	{
		ToggleDebugDraw();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F3))
	{
		DEBUG_breakable = !DEBUG_breakable;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F4))
	{
		DEBUG_raycast = !DEBUG_raycast;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F9))
	{
		DEBUG_demoMode = !DEBUG_demoMode;
	}
	if (g_theInput->WasKeyJustPressed('R'))
	{
		SwitchState(m_currentState);
	}
}

void Game::UpdateAllCurrentRagdolls()
{
	if (m_ragdolls.empty()) return;

	for (auto& r : m_ragdolls)
	{
		r->DEBUG_angleFixRate = DEBUG_angleFixRate;
		r->DEBUG_posFixRate = DEBUG_posFixRate;
		r->DEBUG_maxVelocity = DEBUG_maxVelocity;
		r->m_isBreakable = DEBUG_breakable;
		r->m_deadTimer = DEBUG_ragdoll_deadTimer;
		if (DEBUG_allRagdollLiveForever)
		{
			r->m_isDead = false;
		}
	}
}

void Game::SwitchState(GameState state)
{
	m_previousState = m_currentState;
	m_secondIntoMode = 0.f;
	m_currentState = state;
	Restart();
}

Object_AABB::Object_AABB(Game* game, DoubleAABB3 aabb)
	:GameObject(game), m_aabb(aabb)
{
	m_isFixed = true;

	AddVertsForAABB3D(m_vertexes, m_indexes, aabb, Rgba8::COLOR_WHITE);
	CreateBuffer(g_theRenderer);

	AddVertsForLineAABB3D(m_debugvertexes, GetBoundingBox(), Rgba8::COLOR_CYAN);
	m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	m_debugbuffer->SetIsLinePrimitive(true);
}

void Object_AABB::Render() const
{
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindShader(m_game->m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->BindTexture(m_textureD, 0);
	g_theRenderer->BindTexture(m_textureN, 1);
	g_theRenderer->BindTexture(m_textureS, 2);
	g_theRenderer->SetModelConstants(Mat44(), m_color);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);

	if (m_game->DEBUG_DebugDraw && m_game->DEBUG_DebugDrawOctree)
	{
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());
	}

}

DoubleAABB3 Object_AABB::GetBoundingBox() const
{
	return m_aabb;
}

bool Object_AABB::CollisionResolveVsRagdollNode(Node* node)
{
	if (!node->m_isNode) return false;
	bool result = node->NodeOverlapFixedAABB_Double(m_aabb);
	if (result)node->m_ragdoll->BreakNodeFromRagdoll(node);
	return result;
}

Object_OBB::Object_OBB(Game* game, DoubleOBB3 obb)
	:GameObject(game), m_obb(obb)
{
	m_isFixed = true;
	AddVertsForOBB3D(m_vertexes, m_indexes, obb, Rgba8::COLOR_WHITE);
	CreateBuffer(g_theRenderer);

	AddVertsForLineAABB3D(m_debugvertexes, GetBoundingBox(), Rgba8::COLOR_CYAN);
	m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	m_debugbuffer->SetIsLinePrimitive(true);
}

void Object_OBB::Render() const
{
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindTexture(m_textureD, 0);
	g_theRenderer->BindTexture(m_textureN, 1);
	g_theRenderer->BindTexture(m_textureS, 2);
	g_theRenderer->BindShader(m_game->m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->SetModelConstants(Mat44(), m_color);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);

	if (m_game->DEBUG_DebugDraw && m_game->DEBUG_DebugDrawOctree)
	{
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());
	}
}

DoubleAABB3 Object_OBB::GetBoundingBox() const
{
	return m_obb.GetBoundingBox();
}

bool Object_OBB::CollisionResolveVsRagdollNode(Node* node)
{
	if (!node->m_isNode) return false;
	bool result = node->NodeOverlapFixedOBB_Double(m_obb);
	if (result)node->m_ragdoll->BreakNodeFromRagdoll(node);
	return result;
}

Object_Sphere::Object_Sphere(Game* game, DoubleVec3 center, double radius)
	:GameObject(game), m_center(center), m_radius(radius)
{
	m_isFixed = true;
	AddVertsForSphere(m_vertexes, m_indexes, center, (float)radius, Rgba8::COLOR_WHITE);
	CreateBuffer(g_theRenderer);

	AddVertsForLineAABB3D(m_debugvertexes, GetBoundingBox(), Rgba8::COLOR_CYAN);
	m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	m_debugbuffer->SetIsLinePrimitive(true);
}

void Object_Sphere::Render() const
{
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindTexture(m_textureD, 0);
	g_theRenderer->BindTexture(m_textureN, 1);
	g_theRenderer->BindTexture(m_textureS, 2);
	g_theRenderer->BindShader(m_game->m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->SetModelConstants(Mat44(), m_color);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);

	if (m_game->DEBUG_DebugDraw && m_game->DEBUG_DebugDrawOctree)
	{
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());
	}
}

DoubleAABB3 Object_Sphere::GetBoundingBox() const
{
	DoubleVec3 Min(m_center.x - m_radius, m_center.y - m_radius, m_center.z - m_radius);
	DoubleVec3 Max(m_center.x + m_radius, m_center.y + m_radius, m_center.z + m_radius);
	return DoubleAABB3(Min, Max);
}

bool Object_Sphere::CollisionResolveVsRagdollNode(Node* node)
{
	if (!node->m_isNode) return false;
	bool result = node->NodeOverlapFixedSphere_Double(m_center, m_radius);
	if (result)node->m_ragdoll->BreakNodeFromRagdoll(node);
	return result;
}

Object_Capsule::Object_Capsule(Game* game, DoubleCapsule3 capsule)
	:GameObject(game), m_capsule(capsule)
{
	m_isFixed = true;
	AddVertsForCapsule3D(m_vertexes, m_indexes, capsule, Rgba8::COLOR_WHITE);
	CreateBuffer(g_theRenderer);

	AddVertsForLineAABB3D(m_debugvertexes, GetBoundingBox(), Rgba8::COLOR_CYAN);
	m_debugbuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCU) * (unsigned int)m_debugvertexes.size());
	g_theRenderer->CopyCPUToGPU(m_debugvertexes.data(), (int)(m_debugvertexes.size() * sizeof(Vertex_PCU)), m_debugbuffer);
	m_debugbuffer->SetIsLinePrimitive(true);
}

void Object_Capsule::Render() const
{
	g_theRenderer->SetDepthStencilMode(DepthMode::ENABLED);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	g_theRenderer->BindTexture(m_textureD, 0);
	g_theRenderer->BindTexture(m_textureN, 1);
	g_theRenderer->BindTexture(m_textureS, 2);
	g_theRenderer->BindShader(m_game->m_shader, VertexType::Vertex_PCUTBN);
	g_theRenderer->SetModelConstants(Mat44(), m_color);
	g_theRenderer->DrawIndexedBuffer(m_vbuffer, m_ibuffer, m_indexes.size(), 0, VertexType::Vertex_PCUTBN);

	if (m_game->DEBUG_DebugDraw && m_game->DEBUG_DebugDrawOctree)
	{
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr, 0);
		g_theRenderer->BindTexture(nullptr, 1);
		g_theRenderer->BindTexture(nullptr, 2);
		g_theRenderer->SetModelConstants();
		g_theRenderer->DrawVertexBuffer(m_debugbuffer, m_debugvertexes.size());
	}
}

DoubleAABB3 Object_Capsule::GetBoundingBox() const
{
	return m_capsule.GetBoundingBox();
}

bool Object_Capsule::CollisionResolveVsRagdollNode(Node* node)
{
	if (!node->m_isNode) return false;
	bool result = node->NodeOverlapFixedCapsule_Double(m_capsule);
	if (result)node->m_ragdoll->BreakNodeFromRagdoll(node);
	return result;
}

