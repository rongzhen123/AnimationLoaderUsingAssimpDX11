#pragma once

#ifndef MESH_H
#define	MESH_H

#include <map>
#include <vector>

#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/ai_assert.h>
#include <assimp/cfileio.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include "util.h"
#include "ogldev_math_3d.h"
#include <d3dx11.h>
#include "d3dx11Effect.h"
#include <xnamath.h>
#include <dxerr.h>
#include <cassert>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>

//using namespace std;

struct MeshVertex
{
	Vector3f m_pos;
	Vector2f m_tex;
	//	nv::vec3f m_pos;
	//	nv::vec2f m_tex;
	//nv::vec3f m_normal;
	MeshVertex() { }
	MeshVertex(const Vector3f& pos, const Vector2f& tex /*const nv::vec3f& normal*/)
	{
		m_pos = pos;
		m_tex = tex;
		//m_normal = normal;
	}
};

class Mesh
{
public:

	ID3D11Device* device;

	Mesh();

	~Mesh();

	bool LoadMesh(const std::string& Filename);
	void Render(ID3DX11Effect* mFX, ID3D11DeviceContext*& md3dImmediateContext);
	bool InitMeshFromScene(const aiScene* pScene, const std::string& Filename);
	void InitMesh(unsigned int MeshIndex,	const aiMesh* paiMesh);
	bool InitMaterials(const aiScene* pScene, const std::string& Filename);
	void Clear();

	//	void InitLocation();
#define INVALID_MATERIAL 0xFFFFFFFF

	enum VB_TYPES
	{
		INDEX_BUFFER,
		POS_VB,
		NORMAL_VB,
		TEXCOORD_VB,
		BONE_VB,
		NUM_VBs
	};

	struct MeshEntry
	{
		MeshEntry()
		{
			NumIndices = 0;
			MaterialIndex = INVALID_MATERIAL;
		}
		void Init(ID3D11Device* device);

		ID3D11Buffer* mVB;
		ID3D11Buffer* mIB;

		DXGI_FORMAT mIndexBufferFormat; // Always 16-bit
		UINT mVertexStride;
		std::vector<MeshVertex> m_Vertex;
		std::vector<unsigned int> m_Indices;
		unsigned int NumIndices;
		unsigned int MaterialIndex;
	};
	struct MeshTexture
	{
		MeshTexture() {};
		MeshTexture(const std::string& FileName);
		bool Load(ID3D11Device* device, aiScene* pScene, aiMaterial* material);

		std::string m_fileName;
		//ID3D11Texture2D *pTexture = NULL;
		ID3D11ShaderResourceView* mDiffuseMapSRV;

	};
	std::vector<MeshEntry> m_Entries;
	std::vector<MeshTexture*> m_Textures;

	const aiScene* m_pScene;
	Assimp::Importer m_Importer;
};


#endif	/* OGLDEV_SKINNED_MESH_H */

