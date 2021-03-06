#include "stdafx.h"
#include "Entity.h"
#include "D3D11RenderSystem.h"
#include "Mesh.h"

namespace Neo
{
	//------------------------------------------------------------------------------------
	Entity::Entity(Mesh* pMesh, bool bUpdateAABB)
		:m_position(VEC3::ZERO)
		,m_rotation(QUATERNION::IDENTITY)
		,m_scale(1,1,1)
		,m_bMatrixInvalid(true)
		,m_pMesh(pMesh)
		,m_bCastShadow(true)
		,m_bReceiveShadow(true)
		,m_bUpdateAABB(bUpdateAABB)
	{
		if(m_bUpdateAABB)
			_ComputeAABB();
	}
	//------------------------------------------------------------------------------------
	Entity::~Entity()
	{
	}
	//------------------------------------------------------------------------------------
	void Entity::SetPosition( const VEC3& pos )
	{
		m_position = pos;
		m_bMatrixInvalid = true;
	}
	//------------------------------------------------------------------------------------
	void Entity::SetRotation( const QUATERNION& quat )
	{
		m_rotation = quat;
		m_bMatrixInvalid = true;
	}
	//------------------------------------------------------------------------------------
	void Entity::SetScale( float scale )
	{
		m_scale.Set(scale, scale, scale);
		m_bMatrixInvalid = true;
	}
	//------------------------------------------------------------------------------------
	const MAT44& Entity::GetWorldMatrix()
	{
		_UpdateTransform();
		return m_matWorld;
	}
	//------------------------------------------------------------------------------------
	const MAT44& Entity::GetWorldITMatrix()
	{
		_UpdateTransform();
		return m_matWorldIT;
	}
	//------------------------------------------------------------------------------------
	void Entity::_UpdateTransform()
	{
		if (m_bMatrixInvalid)
		{
			// Compose S-R-T
			MAT44 s, r, t;

			s.SetScale(m_scale);
			r.FromQuaternion(m_rotation);
			t.SetTranslation(m_position);

			m_matWorld = Common::Multiply_Mat44_By_Mat44(s, r);
			m_matWorld = Common::Multiply_Mat44_By_Mat44(m_matWorld, t);

			m_matWorldIT = m_matWorld.Inverse();
			m_matWorldIT = m_matWorldIT.Transpose();

			m_bMatrixInvalid = false;
		}
	}
	//------------------------------------------------------------------------------------
	void Entity::Update()
	{
		//更新世界包围盒
		if (m_bUpdateAABB)
		{
			m_worldAABB = m_localAABB;
			m_worldAABB.Transform(m_matWorld);
		}
	}
	//------------------------------------------------------------------------------------
	void Entity::_ComputeAABB()
	{
		AABB aabb;

		for (uint32 iSub=0; iSub<m_pMesh->GetSubMeshCount(); ++iSub)
		{
			const SubMesh* pSubMesh = m_pMesh->GetSubMesh(iSub);
			const VertexData::PosData& posData = pSubMesh->GetVertData().GetPosData();
			const DWORD nVert = pSubMesh->GetVertData().GetVertCount();

			//AABB start with the first pos
			aabb.m_minCorner = posData[0];
			aabb.m_maxCorner = posData[0];

			for (DWORD i=1; i<nVert; ++i)
			{
				aabb.Merge(posData[i]);
			}

			// Fix: In case of AABB become a plane [2/10/2014 mavaL]
			const float fDist = 0.5f;

			if (Equal(aabb.m_minCorner.x, aabb.m_maxCorner.x))
			{
				aabb.m_minCorner.x -= fDist;
				aabb.m_maxCorner.x += fDist;
			}
			if (Equal(aabb.m_minCorner.y, aabb.m_maxCorner.y))
			{
				aabb.m_minCorner.y -= fDist;
				aabb.m_maxCorner.y += fDist;
			}
			if (Equal(aabb.m_minCorner.z, aabb.m_maxCorner.z))
			{
				aabb.m_minCorner.z -= fDist;
				aabb.m_maxCorner.z += fDist;
			}
		}
		

		SetLocalAABB(aabb);
	}
	//------------------------------------------------------------------------------------
	void Entity::Render( Material* pMaterial /*= nullptr*/ )
	{
		g_env.pRenderSystem->SetTransform(eTransform_World, GetWorldMatrix(), false);
		g_env.pRenderSystem->SetTransform(eTransform_WorldIT, GetWorldITMatrix(), true);

		m_pMesh->Render(pMaterial);
	}
	//------------------------------------------------------------------------------------
	void Entity::SetMaterial( uint32 iSubMesh, Material* pMaterial )
	{
		m_pMesh->GetSubMesh(iSubMesh)->SetMaterial(pMaterial);
	}
	//------------------------------------------------------------------------------------
	void Entity::SetMaterial( Material* pMaterial )
	{
		for (uint32 i=0; i<m_pMesh->GetSubMeshCount(); ++i)
		{
			m_pMesh->GetSubMesh(i)->SetMaterial(pMaterial);
		}
	}
}

