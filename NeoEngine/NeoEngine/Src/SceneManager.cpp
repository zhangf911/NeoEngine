#include "stdafx.h"
#include "SceneManager.h"
#include "Camera.h"
#include "Water.h"
#include "Sky.h"
#include "Terrain.h"
#include "Scene.h"
#include "MeshLoader.h"
#include "D3D11RenderTarget.h"
#include "D3D11Texture.h"
#include "D3D11RenderSystem.h"
#include "Material.h"
#include "SSAO.h"
#include "ShadowMap.h"
#include "Tree.h"
#include "Mesh.h"


namespace Neo
{
	//------------------------------------------------------------------------------------
	SceneManager::SceneManager()
	:m_pRenderSystem(g_env.pRenderSystem)
	,m_camera(nullptr)
	,m_pCurScene(nullptr)
	,m_pTerrain(nullptr)
	,m_pWater(nullptr)
	,m_pSky(nullptr)
	,m_pMeshLoader(new MeshLoader)
	,m_debugRT(eDebugRT_None)
	,m_pShadowMap(new ShadowMap)
	,m_renderFlag(eRenderPhase_All)
	{
		
	}
	//------------------------------------------------------------------------------------
	bool SceneManager::Init()
	{
		m_camera = new Camera;
		m_camera->SetAspectRatio(m_pRenderSystem->GetWndWidth() / (float)m_pRenderSystem->GetWndHeight());

		m_sunLight.lightDir.Set(1, -1, 2);
		m_sunLight.lightDir.Normalize();
		m_sunLight.lightColor.Set(0.8f, 0.8f, 0.8f);

		m_pSSAO = new SSAO;

		{
			m_pDebugRTMesh = new Mesh;
			SubMesh* pSubMesh = new SubMesh;

			SVertex v[4] = 
			{
				SVertex(VEC3(0.5f,1.0f,0), VEC2(0,0)),
				SVertex(VEC3(1.0f,1.0f,0), VEC2(1,0)),
				SVertex(VEC3(0.5f,0.4f,0), VEC2(0,1)),
				SVertex(VEC3(1.0f,0.4f,0), VEC2(1,1))
			};
			DWORD index[6] = { 0,1,2, 1,3,2 };

			pSubMesh->InitVertData(eVertexType_General, v, ARRAYSIZE(v), true);
			pSubMesh->InitIndexData(index, ARRAYSIZE(index), true);

			m_pDebugRTMesh->AddSubMesh(pSubMesh);

			m_pDebugRTMaterial = new Material;
			m_pDebugRTMaterial->InitShader(GetResPath("DebugRT.hlsl"), GetResPath("DebugRT.hlsl"));

			pSubMesh->SetMaterial(m_pDebugRTMaterial);
		}

		_InitAllScene();

		return true;
	}
	//-------------------------------------------------------------------------------
	SceneManager::~SceneManager()
	{
		ClearScene();

		std::for_each(m_scenes.begin(), m_scenes.end(), std::default_delete<Scene>());
		m_scenes.clear();

		SAFE_DELETE(m_pShadowMap);
		SAFE_DELETE(m_camera);
		SAFE_DELETE(m_pDebugRTMesh);
		SAFE_DELETE(m_pMeshLoader);
		SAFE_DELETE(m_pSSAO);
		SAFE_RELEASE(m_pDebugRTMaterial);
	}
	//-------------------------------------------------------------------------------
	void SceneManager::CreateSky()
	{
		m_pSky = new Sky;
	}
	//-------------------------------------------------------------------------------
	void SceneManager::CreateTerrain()
	{
		m_pTerrain = new Terrain(GetResPath("terrain.raw"));
	}
	//-------------------------------------------------------------------------------
	void SceneManager::CreateWater(float waterHeight)
	{
		m_pWater = new Water(waterHeight);
	}
	//-------------------------------------------------------------------------------
	void SceneManager::Update()
	{
		if(m_pShadowMap)
			m_pShadowMap->Update();

		if (m_pWater)
			m_pWater->Update();

		if (m_pSky)
			m_pSky->Update();

		if(m_pCurScene)
			m_pCurScene->Update();
	}
	//------------------------------------------------------------------------------------
	void SceneManager::Render(Material* pMaterial)
	{
		// Render shadow map
		if (m_pShadowMap)
		{
			m_pShadowMap->Render();
		}

		RenderPipline(m_renderFlag, pMaterial);
	}
	//-------------------------------------------------------------------------------
	void SceneManager::RenderPipline(uint32 phaseFlag, Material* pMaterial)
	{
		//================================================================================
		/// Render sky
		//================================================================================
		if (m_pSky && phaseFlag&eRenderPhase_Sky)
		{
			m_pSky->Render();
		}

		//================================================================================
		/// Render terrain
		//================================================================================
		if (m_pTerrain && phaseFlag&eRenderPhase_Terrain)
		{
			m_pTerrain->Render(pMaterial);
		}
		else if (m_pTerrain && phaseFlag&eRenderPhase_ShadowMap)
		{
			m_pTerrain->Render(m_pTerrain->GetShadowMaterial());
		}

		//================================================================================
		/// Render SSAO map
		//================================================================================
		if (phaseFlag & eRenderPhase_SSAO)
		{
			m_pSSAO->Update();

			// Make use of early-Z
			D3D11_DEPTH_STENCIL_DESC& depthDesc = m_pRenderSystem->GetDepthStencilDesc();
			depthDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
			depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			m_pRenderSystem->SetDepthStencelState(depthDesc);
		}

		//================================================================================
		/// Render entities
		//================================================================================
		Scene::EntityList& lstEntity = m_pCurScene->GetEntityList();

		if (phaseFlag & eRenderPhase_Solid)
		{
			for (size_t i=0; i<lstEntity.size(); ++i)
			{
				lstEntity[i]->Render(pMaterial);
			}
		}
		else if (phaseFlag & eRenderPhase_ShadowMap)
		{
			for (size_t i=0; i<lstEntity.size(); ++i)
			{
				Entity* ent = lstEntity[i];
				if(ent->GetCastShadow())
					ent->Render();
			}
		}

		if (phaseFlag & eRenderPhase_SSAO)
		{
			D3D11_DEPTH_STENCIL_DESC& depthDesc = m_pRenderSystem->GetDepthStencilDesc();
			depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
			depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			m_pRenderSystem->SetDepthStencelState(depthDesc);
		}	
		
		//================================================================================
		/// Render water
		//================================================================================
		if (m_pWater && phaseFlag&eRenderPhase_Water)
		{
			m_pWater->Render();
		}

		//================================================================================
		/// Render UI
		//================================================================================
		if (phaseFlag & eRenderPhase_UI)
		{
			D3D11_DEPTH_STENCIL_DESC& depthDesc = m_pRenderSystem->GetDepthStencilDesc();
			depthDesc.DepthEnable = FALSE;
			depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			m_pRenderSystem->SetDepthStencelState(depthDesc);

			char szBuf[64];
			sprintf_s(szBuf, sizeof(szBuf), "lastFPS : %f", g_env.pFrameStat->lastFPS);
			m_pRenderSystem->DrawText(szBuf, IPOINT(10,10), Neo::SColor::YELLOW);

			// Debug RT
			if (m_debugRT == eDebugRT_SSAO)
			{
				m_pDebugRTMesh->Render();
			}
			else if (m_debugRT == eDebugRT_ShadowMap)
			{
				m_pDebugRTMesh->Render();
			}

			depthDesc.DepthEnable = TRUE;
			depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			m_pRenderSystem->SetDepthStencelState(depthDesc);
		}
	}
	//------------------------------------------------------------------------------------
	void SceneManager::ClearScene()
	{
		SAFE_DELETE(m_pTerrain);
		SAFE_DELETE(m_pWater);
		SAFE_DELETE(m_pSky);
	}
	//------------------------------------------------------------------------------------
	Entity* SceneManager::CreateEntity(eEntity type, const STRING& meshname)
	{
		auto iter = m_meshes.find(meshname);

		if (iter == m_meshes.end())
		{
			Mesh* mesh = MeshLoader::LoadMesh(meshname);
			iter = m_meshes.insert(std::make_pair(meshname, mesh)).first;
		}
		
		Mesh* mesh = iter->second;
		assert(mesh);

		Entity* pEntity = nullptr;

		switch (type)
		{
		case eEntity_StaticModel:	pEntity = new Entity(mesh); break;
		case eEntity_Tree:			pEntity = new Tree(mesh); break;
		default: assert(0); return nullptr;
		}

		return pEntity;
	}
	//------------------------------------------------------------------------------------
	void SceneManager::EnableDebugRT( eDebugRT type )
	{
		m_debugRT = type;

		switch (type)
		{
		case eDebugRT_ShadowMap:
			m_pDebugRTMaterial->SetTexture(0, m_pShadowMap->GetShadowTexture());
			break;

		case eDebugRT_SSAO:
			m_pDebugRTMaterial->SetTexture(0, m_pSSAO->GetSSAOMap());
			break;

		case eDebugRT_None:
			m_pDebugRTMaterial->SetTexture(0, nullptr);
			break;

		default: assert(0); break;
		}
		
	}
	//----------------------------------------------------------------------------------------
	void SceneManager::ToggleScene()
	{
		EnableDebugRT(eDebugRT_None);

		static int curScene = -1;
		++curScene;
		if(curScene == m_scenes.size())
			curScene = 0;

		m_pCurScene = m_scenes[curScene];
		m_pCurScene->Enter();
	}
	//------------------------------------------------------------------------------------
	void SceneManager::SetupSunLight( const VEC3& dir, const SColor& color )
	{
		m_sunLight.lightDir = dir;
		m_sunLight.lightColor = color;
	}
	//------------------------------------------------------------------------------------
	Mesh* SceneManager::CreateCubeMesh( const VEC3& minPt, const VEC3& maxPt )
	{
		SVertex vert[24];

		// front side
		vert[0].pos.Set(minPt.x, minPt.y, maxPt.z);
		vert[0].uv.Set(0, 1);
		vert[1].pos.Set(maxPt.x, minPt.y, maxPt.z);
		vert[1].uv.Set(1, 1);
		vert[2].pos.Set(maxPt.x, maxPt.y, maxPt.z);
		vert[2].uv.Set(1, 0);
		vert[3].pos.Set(minPt.x, maxPt.y, maxPt.z);
		vert[3].uv.Set(0, 0);

		vert[0].normal = vert[1].normal = vert[2].normal = vert[3].normal = VEC3::UNIT_Z;

		// back side
		vert[4].pos.Set(maxPt.x, minPt.y, minPt.z);
		vert[4].uv.Set(0, 1);
		vert[5].pos.Set(minPt.x, minPt.y, minPt.z);
		vert[5].uv.Set(1, 1);
		vert[6].pos.Set(minPt.x, maxPt.y, minPt.z);
		vert[6].uv.Set(1, 0);
		vert[7].pos.Set(maxPt.x, maxPt.y, minPt.z);
		vert[7].uv.Set(0, 0);

		vert[4].normal = vert[5].normal = vert[6].normal = vert[7].normal = VEC3::NEG_UNIT_Z;

		// left side
		vert[8].pos.Set(minPt.x, minPt.y, minPt.z);
		vert[9].pos.Set(minPt.x, minPt.y, maxPt.z);
		vert[10].pos.Set(minPt.x, maxPt.y, maxPt.z);
		vert[11].pos.Set(minPt.x, maxPt.y, minPt.z);
		vert[8].uv.Set(0, 1);
		vert[9].uv.Set(1, 1);
		vert[10].uv.Set(1, 0);
		vert[11].uv.Set(0, 0);

		vert[8].normal = vert[9].normal = vert[10].normal = vert[11].normal = VEC3::NEG_UNIT_X;

		// right side
		vert[12].pos.Set(maxPt.x, minPt.y, maxPt.z);
		vert[13].pos.Set(maxPt.x, minPt.y, minPt.z);
		vert[14].pos.Set(maxPt.x, maxPt.y, minPt.z);
		vert[15].pos.Set(maxPt.x, maxPt.y, maxPt.z);
		vert[12].uv.Set(0, 1);
		vert[13].uv.Set(1, 1);
		vert[14].uv.Set(1, 0);
		vert[15].uv.Set(0, 0);

		vert[12].normal = vert[13].normal = vert[14].normal = vert[15].normal = VEC3::UNIT_X;

		// up side
		vert[16].pos.Set(minPt.x, maxPt.y, maxPt.z);
		vert[17].pos.Set(maxPt.x, maxPt.y, maxPt.z);
		vert[18].pos.Set(maxPt.x, maxPt.y, minPt.z);
		vert[19].pos.Set(minPt.x, maxPt.y, minPt.z);
		vert[16].uv.Set(0, 1);
		vert[17].uv.Set(1, 1);
		vert[18].uv.Set(1, 0);
		vert[19].uv.Set(0, 0);

		vert[16].normal = vert[17].normal = vert[18].normal = vert[19].normal = VEC3::UNIT_Y;

		// down side
		vert[20].pos.Set(minPt.x, minPt.y, minPt.z);
		vert[21].pos.Set(maxPt.x, minPt.y, minPt.z);
		vert[22].pos.Set(maxPt.x, minPt.y, maxPt.z);
		vert[23].pos.Set(minPt.x, minPt.y, maxPt.z);
		vert[20].uv.Set(0, 1);
		vert[21].uv.Set(1, 1);
		vert[22].uv.Set(1, 0);
		vert[23].uv.Set(0, 0);

		vert[20].normal = vert[21].normal = vert[22].normal = vert[23].normal = VEC3::NEG_UNIT_Y;

		DWORD faces[6*2*3] = {
			// front
			0,1,2,
			0,2,3,

			// back
			4,5,6,
			4,6,7,

			// left
			8,9,10,
			8,10,11,

			// right
			12,13,14,
			12,14,15,

			// up
			16,17,18,
			16,18,19,

			// down
			20,21,22,
			20,22,23
		};

		Mesh* pCube =  new Mesh;
		SubMesh* pSubmesh = new SubMesh;

		pSubmesh->InitVertData(eVertexType_General, vert, 24, true);
		pSubmesh->InitIndexData(faces, 6*2*3, true);

		pCube->AddSubMesh(pSubmesh);

		return pCube;
	}
	//------------------------------------------------------------------------------------
	Mesh* SceneManager::CreateFrustumMesh( const VEC3& minBottom, const VEC3& maxBottom, const VEC3& minTop, const VEC3& maxTop )
	{
		Neo::SVertex vert[24];

		// front side
		vert[0].pos.Set(minBottom.x, minBottom.y, maxBottom.z);
		vert[0].uv.Set(0, 1);
		vert[1].pos.Set(maxBottom.x, minBottom.y, maxBottom.z);
		vert[1].uv.Set(1, 1);
		vert[2].pos.Set(maxTop.x, minTop.y, maxTop.z);
		vert[2].uv.Set(1, 0);
		vert[3].pos.Set(minTop.x, minTop.y, maxTop.z);
		vert[3].uv.Set(0, 0);

		// back side
		vert[4].pos.Set(maxBottom.x, minBottom.y, minBottom.z);
		vert[4].uv.Set(0, 1);
		vert[5].pos.Set(minBottom.x, minBottom.y, minBottom.z);
		vert[5].uv.Set(1, 1);
		vert[6].pos.Set(minTop.x, minTop.y, minTop.z);
		vert[6].uv.Set(1, 0);
		vert[7].pos.Set(maxTop.x, minTop.y, minTop.z);
		vert[7].uv.Set(0, 0);

		// left side
		vert[8].pos.Set(minBottom.x, minBottom.y, minBottom.z);
		vert[9].pos.Set(minBottom.x, minBottom.y, maxBottom.z);
		vert[10].pos.Set(minTop.x, minTop.y, maxTop.z);
		vert[11].pos.Set(minTop.x, minTop.y, minTop.z);
		vert[8].uv.Set(0, 1);
		vert[9].uv.Set(1, 1);
		vert[10].uv.Set(1, 0);
		vert[11].uv.Set(0, 0);

		// right side
		vert[12].pos.Set(maxBottom.x, minBottom.y, maxBottom.z);
		vert[13].pos.Set(maxBottom.x, minBottom.y, minBottom.z);
		vert[14].pos.Set(maxTop.x, minTop.y, minTop.z);
		vert[15].pos.Set(maxTop.x, minTop.y, maxTop.z);
		vert[12].uv.Set(0, 1);
		vert[13].uv.Set(1, 1);
		vert[14].uv.Set(1, 0);
		vert[15].uv.Set(0, 0);

		// up side
		vert[16].pos.Set(minTop.x, minTop.y, maxTop.z);
		vert[17].pos.Set(maxTop.x, minTop.y, maxTop.z);
		vert[18].pos.Set(maxTop.x, minTop.y, minTop.z);
		vert[19].pos.Set(minTop.x, minTop.y, minTop.z);
		vert[16].uv.Set(0, 1);
		vert[17].uv.Set(1, 1);
		vert[18].uv.Set(1, 0);
		vert[19].uv.Set(0, 0);

		// down side
		vert[20].pos.Set(minBottom.x, minBottom.y, minBottom.z);
		vert[21].pos.Set(maxBottom.x, minBottom.y, minBottom.z);
		vert[22].pos.Set(maxBottom.x, minBottom.y, maxBottom.z);
		vert[23].pos.Set(minBottom.x, minBottom.y, maxBottom.z);
		vert[20].uv.Set(0, 1);
		vert[21].uv.Set(1, 1);
		vert[22].uv.Set(1, 0);
		vert[23].uv.Set(0, 0);

		DWORD faces[6*2*3] = {
			// front
			0,1,2,
			0,2,3,

			// back
			4,5,6,
			4,6,7,

			// left
			8,9,10,
			8,10,11,

			// right
			12,13,14,
			12,14,15,

			// up
			16,17,18,
			16,18,19,

			// down
			20,21,22,
			20,22,23
		};

		Mesh* pMesh =  new Mesh;
		SubMesh* pSubmesh = new SubMesh;

		pSubmesh->InitVertData(eVertexType_General, vert, 24, true);
		pSubmesh->InitIndexData(faces, 6*2*3, true);

		pMesh->AddSubMesh(pSubmesh);

		return pMesh;
	}
	//------------------------------------------------------------------------------------
	Mesh* SceneManager::CreatePlaneMesh( float w, float h )
	{
		float halfW = w / 2;
		float halfH = h / 2;

		SVertex vert[4] =
		{
			SVertex(VEC3(-w,0,+h), VEC2(0,0), VEC3::UNIT_Y),
			SVertex(VEC3(+w,0,+h), VEC2(1,0), VEC3::UNIT_Y),
			SVertex(VEC3(+w,0,-h), VEC2(1,1), VEC3::UNIT_Y),
			SVertex(VEC3(-w,0,-h), VEC2(0,1), VEC3::UNIT_Y),
		};

		DWORD dwIndex[6] = {0,1,3,1,2,3};

		Mesh* pMesh =  new Mesh;
		SubMesh* pSubmesh = new SubMesh;

		pSubmesh->InitVertData(eVertexType_General, vert, 4, true);
		pSubmesh->InitIndexData(dwIndex, 6, true);

		pMesh->AddSubMesh(pSubmesh);

		return pMesh;
	}
}