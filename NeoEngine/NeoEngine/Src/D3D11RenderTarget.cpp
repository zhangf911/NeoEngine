#include "stdafx.h"
#include "D3D11RenderTarget.h"
#include "D3D11RenderSystem.h"
#include "D3D11Texture.h"
#include "SceneManager.h"
#include "Mesh.h"
#include "Camera.h"
#include "Entity.h"


namespace Neo
{
	Mesh* D3D11RenderTarget::m_pQuadMesh = nullptr;
	Entity* D3D11RenderTarget::m_pQuadEntity = nullptr;

	//----------------------------------------------------------------------------------------
	D3D11RenderTarget::D3D11RenderTarget()
	:m_pRenderSystem(g_env.pRenderSystem)
	,m_pRenderTexture(nullptr)
	,m_clearColor(SColor::BLACK)
	,m_bClearColor(true)
	,m_bClearZBuffer(true)
	,m_bHasDepthBuffer(false)
	,m_bNoFrameBuffer(false)
	,m_bUpdateRatioAspect(true)
	,m_phaseFlag(eRenderPhase_Geometry)
	,m_pDepthStencil(nullptr)
	,m_sizeRatio(0, 0)
	{
		// Create screen quad
		static bool bCreate = false;
		if (!bCreate)
		{
			m_pQuadMesh = new Mesh;
			SubMesh* pSubMesh = new SubMesh;

			SVertex v[4] = 
			{
				SVertex(VEC3(-1,1,0), VEC2(0,0)),
				SVertex(VEC3(1,1,0), VEC2(1,0)),
				SVertex(VEC3(-1,-1,0), VEC2(0,1)),
				SVertex(VEC3(1,-1,0), VEC2(1,1))
			};
			DWORD index[6] = { 0,1,2, 1,3,2 };

			// Store index to frustum far corner
			v[0].normal.x = 0;
			v[1].normal.x = 1;
			v[2].normal.x = 2;
			v[3].normal.x = 3;

			pSubMesh->InitVertData(eVertexType_General, v, ARRAYSIZE(v), true);
			pSubMesh->InitIndexData(index, ARRAYSIZE(index), true);

			m_pQuadMesh->AddSubMesh(pSubMesh);

			m_pQuadEntity = new Entity(m_pQuadMesh);

			m_pQuadEntity->SetCastShadow(false);
			m_pQuadEntity->SetReceiveShadow(false);

			bCreate = true;
		}
	}
	//----------------------------------------------------------------------------------------
	D3D11RenderTarget::~D3D11RenderTarget()
	{
		Destroy();
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::Init( uint32 width, uint32 height, ePixelFormat format, bool bOwnDepthBuffer /*= true*/, bool bUpdateRatioAspect, bool bNoFrameBuffer )
	{
		// Setup the viewport
		m_viewport.Width = (float)width;
		m_viewport.Height = (float)height;
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;
		m_viewport.TopLeftX = 0;
		m_viewport.TopLeftY = 0;

		const uint32 screenW = m_pRenderSystem->GetWndWidth();
		const uint32 screenH = m_pRenderSystem->GetWndHeight();

		m_sizeRatio.Set(width / (float)screenW, height / (float)screenH);

		m_bHasDepthBuffer = bOwnDepthBuffer;
		m_bNoFrameBuffer = bNoFrameBuffer;
		m_bUpdateRatioAspect = bUpdateRatioAspect;

		// Create render texture
		m_pRenderTexture = new D3D11Texture(width, height, nullptr, format, eTextureUsage_RenderTarget, false);

		// Create depth stencil buffer
		if (m_bHasDepthBuffer)
			_CreateDepthBuffer(width, height);
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::Destroy()
	{
		SAFE_RELEASE(m_pDepthStencil);
		SAFE_RELEASE(m_pRenderTexture);
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::_CreateDepthBuffer( uint32 width, uint32 height )
	{
		m_pDepthStencil = new D3D11Texture(width, height, nullptr, ePF_Unknown, eTextureUsage_Depth, false);	
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::_BeforeRender()
	{
		// Update aspect ratio and viewport
		if (m_bUpdateRatioAspect)
		{
			g_env.pSceneMgr->GetCamera()->SetAspectRatio(m_viewport.Width / m_viewport.Height);
			m_pRenderSystem->SetTransform(eTransform_Proj, g_env.pSceneMgr->GetCamera()->GetProjMatrix(), true);
		}		

		m_pRenderSystem->SetViewport(m_viewport);
		m_pRenderSystem->SetRenderTarget(this, m_bClearColor, m_bClearZBuffer, &m_clearColor);
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::_AfterRender()
	{
		// Restore
		if (m_bUpdateRatioAspect)
		{
			g_env.pSceneMgr->GetCamera()->SetAspectRatio(m_pRenderSystem->GetWndWidth() / (float)m_pRenderSystem->GetWndHeight());
			m_pRenderSystem->SetTransform(eTransform_Proj, g_env.pSceneMgr->GetCamera()->GetProjMatrix(), true);
		}		

		m_pRenderSystem->RestoreViewport();
		m_pRenderSystem->SetRenderTarget(nullptr, false, false);
	}
	//----------------------------------------------------------------------------------------
	void D3D11RenderTarget::Update(Material* pMaterial)
	{
		_BeforeRender();

		g_env.pSceneMgr->RenderPipline(m_phaseFlag, pMaterial);

		_AfterRender();
	}
	//----------------------------------------------------------------------------------------
	void D3D11RenderTarget::SetClearColor( const SColor& color )
	{
		m_clearColor = color;
	}
	//----------------------------------------------------------------------------------------
	void D3D11RenderTarget::SetClearEveryFrame( bool bColor, bool bZBuffer )
	{
		m_bClearColor = bColor;
		m_bClearZBuffer = bZBuffer;
	}
	//------------------------------------------------------------------------------------
	ID3D11DepthStencilView* D3D11RenderTarget::GetDSView()
	{
		return m_pDepthStencil ? m_pDepthStencil->GetDSV() : m_pRenderSystem->GetDSView();
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::RenderScreenQuad( Material* pMaterial )
	{
		_BeforeRender();

		// Turn off z buffer
		D3D11_DEPTH_STENCIL_DESC& depthDesc = m_pRenderSystem->GetDepthStencilDesc();
		depthDesc.DepthEnable = FALSE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		m_pRenderSystem->SetDepthStencelState(depthDesc);

		m_pQuadEntity->Render(pMaterial);	

		_AfterRender();

		// Restore render state
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		m_pRenderSystem->SetDepthStencelState(depthDesc);
	}
	//------------------------------------------------------------------------------------
	void D3D11RenderTarget::OnWindowResized()
	{
		const uint32 screenW = m_pRenderSystem->GetWndWidth();
		const uint32 screenH = m_pRenderSystem->GetWndHeight();

		const uint32 newWidth = (uint32)(screenW * m_sizeRatio.x);
		const uint32 newHeight = (uint32)(screenH * m_sizeRatio.y);

		// Resize render texture
		m_pRenderTexture->Resize(newWidth, newHeight);
		m_pDepthStencil->Resize(newWidth, newHeight);
	}
}
