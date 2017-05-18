#include "mesh.h"
#include "util.h"
#include "StringComparison.h"
#include "D3DCompiler.h"

#define POSITION_LOCATION    0
#define TEX_COORD_LOCATION   1
#define NORMAL_LOCATION      2

extern int FindValidPath(aiString* p_szString);
extern bool TryLongerPath(char* szTemp, aiString* p_szString);

const D3D11_INPUT_ELEMENT_DESC PosTex[2] =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};


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
			HR(D3DX11CreateShaderResourceViewFromFileA(device,szPath.C_Str(), 0, 0, &mDiffuseMapSRV, 0));
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

bool Mesh::Init(ID3D11Device * d3d11device)
{
	device = d3d11device;
	DWORD shaderFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3D10Blob* compiledShader = 0;
	ID3D10Blob* compilationMsgs = 0;
	HRESULT hr;
	hr = D3DX11CompileFromFile(L"FX/staticMesh.fx", 0, 0, 0, "fx_5_0", shaderFlags, 0, 0, &compiledShader, &compilationMsgs, 0);
	// compilationMsgs can store errors or warnings.
	if (compilationMsgs != 0)
	{
		MessageBoxA(0, (char*)compilationMsgs->GetBufferPointer(), 0, 0);
		ReleaseCOM(compilationMsgs);
	}
	// Even if there are no compilationMsgs, check to make sure there were no other errors.
	if (FAILED(hr))
	{
		DXTrace(__FILE__, (DWORD)__LINE__, hr, L"D3DX11CompileFromFile", true);
	}
	hr = D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), 0, device, &mStaticMeshFX);
	if (FAILED(hr))
	{
		return false;
	}
	// Done with compiled shader.
	ReleaseCOM(compiledShader);
	mStaticMeshTech = mStaticMeshFX->GetTechniqueByName("StaticMeshTech");
	m_StaticMesh_fxWorldViewProj = mStaticMeshFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
	D3DX11_PASS_DESC passDesc;
	mStaticMeshTech->GetPassByIndex(0)->GetDesc(&passDesc);
	hr = device->CreateInputLayout(PosTex, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout_staticmesh);
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_RASTERIZER_DESC wireframeDesc;
	ZeroMemory(&wireframeDesc, sizeof(D3D11_RASTERIZER_DESC));
	wireframeDesc.FillMode = D3D11_FILL_SOLID;
	wireframeDesc.CullMode = D3D11_CULL_FRONT;
	wireframeDesc.FrontCounterClockwise = false;
	wireframeDesc.DepthClipEnable = true;

	hr = device->CreateRasterizerState(&wireframeDesc, &WireframeRS);
	if (FAILED(hr))
	{
		return false;
	}

	primitive_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	return true;
}

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

bool Mesh::Update(float dt, const XMMATRIX& worldViewProj)
{
	worldviewproj = worldViewProj;
	return false;
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

void Mesh::Render(ID3D11DeviceContext*& md3dImmediateContext)
{
	md3dImmediateContext->IASetPrimitiveTopology(primitive_type);
	md3dImmediateContext->RSSetState(WireframeRS);
	md3dImmediateContext->IASetInputLayout(mInputLayout_staticmesh);
	//绘制背景场景环境
	D3DX11_TECHNIQUE_DESC techDesc;
	mStaticMeshTech = mStaticMeshFX->GetTechniqueByName("StaticMeshTech");
	m_StaticMesh_fxWorldViewProj = mStaticMeshFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();

	for (int i = 0; i < m_Entries.size(); i++)
	{
		mStaticMeshTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			UINT stride = sizeof(MeshVertex);
			UINT offset = 0;
		
			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX rotation = XMMatrixRotationX(-0);
			XMMATRIX translation = XMMatrixTranslation(-0, -0, -0);
			world = world * rotation * translation;
			XMMATRIX worldViewProj = world*worldviewproj;
			m_StaticMesh_fxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));
			StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
			ID3D11ShaderResourceView* tex = m_Textures[m_Entries[i].MaterialIndex]->mDiffuseMapSRV;
			StaticMesh_DiffuseMap->SetResource(tex);
			md3dImmediateContext->IASetVertexBuffers(0, 1, &m_Entries[i].mVB, &stride, &offset);
			md3dImmediateContext->IASetIndexBuffer(m_Entries[i].mIB, DXGI_FORMAT_R32_UINT, 0);
			mStaticMeshTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->DrawIndexed(m_Entries[i].m_Indices.size(), 0, 0);
		}
	}
	md3dImmediateContext->RSSetState(0);
}
