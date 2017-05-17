#include "mesh.h"
#include "util.h"
#include "StringComparison.h"
#define POSITION_LOCATION    0
#define TEX_COORD_LOCATION   1
#define NORMAL_LOCATION      2

extern int FindValidPath(aiString* p_szString);
extern bool TryLongerPath(char* szTemp, aiString* p_szString);

Mesh::MeshTexture::MeshTexture(const std::string& FileName)
{
	//	m_textureTarget = TextureTarget;
	m_fileName = FileName;
}

bool Mesh::MeshTexture::Load(ID3D11Device* device, aiScene* pScene, aiMaterial* material)
{
	ID3D11Texture2D *pTexture = NULL;
	// first get a valid path to the texture
	aiString szPath(m_fileName);
	const aiMaterial* pcMat = material;
	// DIFFUSE TEXTURE ------------------------------------------------
	if (AI_SUCCESS == aiGetMaterialString(pcMat, AI_MATKEY_TEXTURE_DIFFUSE(0), &szPath))
	{
		if (5 == FindValidPath(&szPath))
		{
			std::string sz = "[ERROR] Unable to load embedded texture (#1): ";
			sz.append(szPath.data);
			MessageBoxA(NULL, sz.c_str(), "EE", MB_OK);
			// embedded file. Find its index
		}
		else
		{
			HR(D3DX11CreateShaderResourceViewFromFileA(device,
				szPath.C_Str(), 0, 0, &mDiffuseMapSRV, 0));
		}
	}
	return true;
}
void Mesh::MeshEntry::Init(ID3D11Device* device)
{
	mVertexStride = sizeof(MeshVertex);

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(MeshVertex) * m_Vertex.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &m_Vertex[0];// &indices[0]

	HR(device->CreateBuffer(&vbd, &vinitData, &mVB));

	//ReleaseCOM(mIB);
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * m_Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &m_Indices[0];//.data();

	HR(device->CreateBuffer(&ibd, &iinitData, &mIB));
}
Mesh::Mesh()
{
	m_pScene = NULL;
}
Mesh::~Mesh()
{
	Clear();
}
void Mesh::Clear()
{
	for (int i = 0; i < m_Textures.size(); i++)
	{
		SAFE_DELETE(m_Textures[i]);
	}
	for (int i = 0; i < m_Entries.size(); i++)
	{
		ReleaseCOM(m_Entries[i].mIB);
		ReleaseCOM(m_Entries[i].mVB);
		m_Entries[i].m_Vertex.clear();
		m_Entries[i].m_Indices.clear();
	}
}
char g_szFileName_mesh[MAX_PATH];

bool Mesh::LoadMesh(const std::string& Filename)
{
	strcpy(g_szFileName_mesh, Filename.c_str());
	// Release the previously loaded mesh (if it exists)
	Clear();
	bool Ret = false;
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices)
	m_pScene = m_Importer.ReadFile(Filename.c_str(), ASSIMP_LOAD_FLAGS);
	if (m_pScene)
	{
		Ret = InitMeshFromScene(m_pScene, Filename);
	}
	else
	{
		char* b = (char*)aiGetErrorString();
		printf("Error parsing '%s':'%s'\n", Filename.c_str(), b);
	}
	return Ret;
}

bool Mesh::InitMeshFromScene(const aiScene* pScene, const std::string& Filename)
{
	m_Entries.resize(pScene->mNumMeshes);
	m_Textures.resize(pScene->mNumMaterials);
	// Initialize the meshes in the scene one by one
	for (int i = 0; i < m_Entries.size(); i++)
	{
		const aiMesh* paiMesh = pScene->mMeshes[i];
		InitMesh(i, paiMesh);
	}
	if (!InitMaterials(pScene, Filename))
	{
		return false;
	}
	return true;
}

void Mesh::InitMesh(unsigned int MeshIndex, const aiMesh* paiMesh)
{
	m_Entries[MeshIndex].MaterialIndex = paiMesh->mMaterialIndex;
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
	for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
	{
		const aiVector3D* pPos = &(paiMesh->mVertices[i]);
		//const aiVector3D* pNormal = &(paiMesh->mNormals[i]);
		const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &(paiMesh->mTextureCoords[0][i]) : &Zero3D;
		MeshVertex v(Vector3f(pPos->x,pPos->z , pPos->y),
			Vector2f(pTexCoord->x, pTexCoord->y));

		m_Entries[MeshIndex].m_Vertex.push_back(v);
	}
	for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
	{
		const aiFace& Face = paiMesh->mFaces[i];
		assert(Face.mNumIndices == 3);
		m_Entries[MeshIndex].m_Indices.push_back(Face.mIndices[0]);
		m_Entries[MeshIndex].m_Indices.push_back(Face.mIndices[1]);
		m_Entries[MeshIndex].m_Indices.push_back(Face.mIndices[2]);
	}
	m_Entries[MeshIndex].Init(device);
}

bool Mesh::InitMaterials(const aiScene* pScene, const std::string& Filename)
{
	// Extract the directory part from the file name
	std::string::size_type SlashIndex = Filename.find_last_of("\\");
	std::string Dir;

	if (SlashIndex == std::string::npos)
	{
		Dir = ".";
	}
	else if (SlashIndex == 0)
	{
		Dir = "/";
	}
	else
	{
		Dir = Filename.substr(0, SlashIndex);
	}
	bool Ret = true;
	// Initialize the materials
	for (unsigned int i = 0; i < pScene->mNumMaterials; i++)
	{
		const aiMaterial* pMaterial = pScene->mMaterials[i];
		m_Textures[i] = NULL;
		if (aiGetMaterialTextureCount(pMaterial, aiTextureType_DIFFUSE) > 0)
		{
			aiString Path;
			if (aiGetMaterialTexture(pMaterial, aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
			{
				std::string FullPath = /*Dir + "/" +*/ Path.data;
				m_Textures[i] = new MeshTexture(FullPath.c_str());
				if (!m_Textures[i]->Load(device, (aiScene*)pScene, (aiMaterial*)pMaterial))
				{
					MessageBoxA(NULL, "Error loading texture", FullPath.c_str(), MB_OK);
					//printf("Error loading texture '%s'\n", Filename.c_str());
					delete m_Textures[i];
					m_Textures[i] = NULL;
					Ret = false;
				}
				else
				{
					printf("Loaded texture '%s'\n", Filename.c_str());
				}
			}
		}
	}
	return Ret;
}

void Mesh::Render(ID3DX11Effect* mFX, ID3D11DeviceContext*& md3dImmediateContext)
{
	for (unsigned int i = 0; i < m_Entries.size(); i++)
	{
		UINT stride = sizeof(MeshVertex);
		UINT offset = 0;
		md3dImmediateContext->IASetVertexBuffers(0, 1, &m_Entries[i].mVB, &stride, &offset);
		md3dImmediateContext->IASetIndexBuffer(m_Entries[i].mIB, DXGI_FORMAT_R32_UINT, 0);
		D3DX11_TECHNIQUE_DESC techDesc;
		ID3DX11EffectTechnique* mTech;
		ID3DX11EffectShaderResourceVariable* DiffuseMap;
		mTech = mFX->GetTechniqueByName("ColorTech");
		DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
		ID3D11ShaderResourceView* tex = m_Textures[m_Entries[i].MaterialIndex]->mDiffuseMapSRV;
		DiffuseMap->SetResource(tex);
		mTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			// 36 indices for the box.
			md3dImmediateContext->DrawIndexed(m_Entries[i].m_Indices.size(), 0, 0);
		}
	}
}
