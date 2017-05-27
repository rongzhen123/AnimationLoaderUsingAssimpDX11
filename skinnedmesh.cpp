#include "skinnedmesh.h"
#include "util.h"
#include "StringComparison.h"
#include "D3DCompiler.h"
#include "Camera.h"

#define POSITION_LOCATION    0
#define TEX_COORD_LOCATION   1
#define NORMAL_LOCATION      2
#define BONE_ID_LOCATION     3
#define BONE_WEIGHT_LOCATION 4
int FindValidPath(aiString* p_szString);
bool TryLongerPath(char* szTemp, aiString* p_szString);

const D3D11_INPUT_ELEMENT_DESC PosTexSkinned[4] =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "WEIGHTS",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONEINDICES",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

void VertexBoneData::AddBoneData(float BoneID, float Weight)
{
	for (int i = 0; i < ARRAY_SIZE_IN_ELEMENTS(IDs); i++)
	{
		if (Weights[i] == 0.0)
		{
			IDs[i] = BoneID;
			Weights[i] = Weight;
			return;
		}
	}
	// should never get here - more bones than we have space for
	assert(0);
}

SkinnedMesh::Texture::Texture(const std::string& FileName)
{
//	m_textureTarget = TextureTarget;
	m_fileName = FileName;
}

bool SkinnedMesh::Texture::Load(ID3D11Device* device, aiScene* pScene, aiMaterial* material)
{
	ID3D11Texture2D *pTexture = NULL;
	// first get a valid path to the texture
	aiString szPath(m_fileName);
	const aiMaterial* pcMat = material;
	//
	// DIFFUSE TEXTURE ------------------------------------------------
	//
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

void SkinnedMesh::SkinnedMeshEntry::Init(ID3D11Device* device)
{
	mVertexStride = sizeof(SkinnedVertex);
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(SkinnedVertex) * m_Vertex.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &m_Vertex[0];// &indices[0]
	HR(device->CreateBuffer(&vbd, &vinitData, &mVB));
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

SkinnedMesh::SkinnedMesh()
{
	m_NumBones = 0;
	m_pScene = NULL;
	
}

SkinnedMesh::~SkinnedMesh()
{
	Clear();
}

bool SkinnedMesh::Init(ID3D11Device* d3d11device)
{
	device = d3d11device;
	DWORD shaderFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ID3D10Blob* compiledShader = 0;
	ID3D10Blob* compilationMsgs = 0;
	HRESULT hr;
	hr = D3DX11CompileFromFile(L"FX/color.fx", 0, 0, 0, "fx_5_0", shaderFlags, 0, 0, &compiledShader, &compilationMsgs, 0);
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
		return false;
	}
	hr = D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(),0, device, &mFX);
	if (FAILED(hr))
	{
		return false;
	}
	// Done with compiled shader.
	ReleaseCOM(compiledShader);
	mTech = mFX->GetTechniqueByName("ColorTech");
	mfxWorldViewProj = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
	BoneTransforms = mFX->GetVariableByName("gBoneTransforms")->AsMatrix();
	D3DX11_PASS_DESC passDesc;
	mTech->GetPassByIndex(0)->GetDesc(&passDesc);
	hr = device->CreateInputLayout(PosTexSkinned, 4, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout);
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

	//worldviewproj = XMMatrixIdentity();
	return true;
}

bool SkinnedMesh::Update(float dt, const XMFLOAT4X4& world)
{
	mWorldMatrix = world;
	return false;
}
std::vector<SkinnedMesh*>  SkinnedMesh::renderQueue;
void SkinnedMesh::Clear()
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

char g_szFileName[MAX_PATH];
bool SkinnedMesh::LoadMesh(const std::string& Filename)
{
	strcpy(g_szFileName, Filename.c_str());
	// Release the previously loaded mesh (if it exists)
	Clear();
	bool Ret = false;
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices)
		m_pScene = m_Importer.ReadFile(Filename.c_str(), ASSIMP_LOAD_FLAGS);
	if (m_pScene)
	{
		m_GlobalInverseTransform = m_pScene->mRootNode->mTransformation;
		m_GlobalInverseTransform.Inverse();
		Ret = InitSkinnedMeshFromScene(m_pScene, Filename);
	}
	else
	{
		char* b = (char*)aiGetErrorString();
		printf("Error parsing '%s':'%s'\n", Filename.c_str(), b);
		return false;
	}
	return Ret;
}

bool SkinnedMesh::InitSkinnedMeshFromScene(const aiScene* pScene, const std::string& Filename)
{
	m_Entries.resize(pScene->mNumMeshes);
	m_Textures.resize(pScene->mNumMaterials);
	// Initialize the meshes in the scene one by one
	for (int i = 0; i < m_Entries.size(); i++)
	{
		const aiMesh* paiMesh = pScene->mMeshes[i];
		InitSkinnedMesh(i, paiMesh);
	}
	if (!InitMaterials(pScene, Filename))
	{
		return false;
	}
	return true;
}
void SkinnedMesh::InitSkinnedMesh(unsigned int MeshIndex,const aiMesh* paiMesh)
{
	m_Entries[MeshIndex].MaterialIndex = paiMesh->mMaterialIndex;
	std::vector<VertexBoneData> Bones;
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
	Bones.resize(paiMesh->mNumVertices);
	LoadBones(MeshIndex, paiMesh, Bones);
	for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
	{
		const aiVector3D* pPos = &(paiMesh->mVertices[i]);
		//const aiVector3D* pNormal = &(paiMesh->mNormals[i]);
		const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &(paiMesh->mTextureCoords[0][i]) : &Zero3D;
		SkinnedVertex v(Vector3f(pPos->x, pPos->y, pPos->z),Vector2f(pTexCoord->x, pTexCoord->y),VertexBoneData(Bones[i]));
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
void SkinnedMesh::LoadBones(unsigned int MeshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& Bones)
{
	for (unsigned int i = 0; i < pMesh->mNumBones; i++)
	{
		unsigned int BoneIndex = 0;
		std::string BoneName(pMesh->mBones[i]->mName.data);
		if (m_BoneMapping.find(BoneName) == m_BoneMapping.end())
		{
			// Allocate an index for a new bone
			BoneIndex = m_NumBones;
			m_NumBones++;
			BoneInfo bi;
			m_BoneInfo.push_back(bi);
			m_BoneInfo[BoneIndex].BoneOffset = pMesh->mBones[i]->mOffsetMatrix;
			m_BoneMapping[BoneName] = BoneIndex;
		}
		else
		{
			BoneIndex = m_BoneMapping[BoneName];
		}
		for (int j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
		{
			unsigned int VertexID =/* m_Entries[MeshIndex].BaseVertex +*/ pMesh->mBones[i]->mWeights[j].mVertexId;
			float Weight = pMesh->mBones[i]->mWeights[j].mWeight;
			Bones[VertexID].AddBoneData(BoneIndex, Weight);
		}
	}
}
bool TryLongerPath(char* szTemp, aiString* p_szString)
{
	char szTempB[MAX_PATH];
	strcpy(szTempB, szTemp);
	// go to the beginning of the file name
	char* szFile = strrchr(szTempB, '\\');
	if (!szFile)szFile = strrchr(szTempB, '/');
	char* szFile2 = szTemp + (szFile - szTempB) + 1;
	szFile++;
	char* szExt = strrchr(szFile, '.');
	if (!szExt)return false;
	szExt++;
	*szFile = 0;
	strcat(szTempB, "*.*");
	const unsigned int iSize = (const unsigned int)(szExt - 1 - szFile);
	HANDLE          h;
	WIN32_FIND_DATAA info;
	// build a list of files
	h = FindFirstFileA(szTempB, &info);
	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(strcmp(info.cFileName, ".") == 0 || strcmp(info.cFileName, "..") == 0))
			{
				char* szExtFound = strrchr(info.cFileName, '.');
				if (szExtFound)
				{
					++szExtFound;
					if (0 == Assimp::ASSIMP_stricmp(szExtFound, szExt))
					{
						const unsigned int iSizeFound = (const unsigned int)(
							szExtFound - 1 - info.cFileName);

						for (unsigned int i = 0; i < iSizeFound; ++i)
							info.cFileName[i] = (CHAR)tolower(info.cFileName[i]);

						if (0 == memcmp(info.cFileName, szFile2, min(iSizeFound, iSize)))
						{
							// we have it. Build the full path ...
							char* sz = strrchr(szTempB, '*');
							*(sz - 2) = 0x0;

							strcat(szTempB, info.cFileName);

							// copy the result string back to the aiString
							const size_t iLen = strlen(szTempB);
							size_t iLen2 = iLen + 1;
							iLen2 = iLen2 > MAXLEN ? MAXLEN : iLen2;
							memcpy(p_szString->data, szTempB, iLen2);
							p_szString->length = iLen;
							return true;
						}
					}
					// check whether the 8.3 DOS name is matching
					if (0 == Assimp::ASSIMP_stricmp(info.cAlternateFileName, p_szString->data))
					{
						strcat(szTempB, info.cAlternateFileName);
						// copy the result string back to the aiString
						const size_t iLen = strlen(szTempB);
						size_t iLen2 = iLen + 1;
						iLen2 = iLen2 > MAXLEN ? MAXLEN : iLen2;
						memcpy(p_szString->data, szTempB, iLen2);
						p_szString->length = iLen;
						return true;
					}
				}
			}
		} while (FindNextFileA(h, &info));

		FindClose(h);
	}
	return false;
}
int FindValidPath(aiString* p_szString)
{
	ai_assert(NULL != p_szString);
	aiString pcpy = *p_szString;
	if ('*' == p_szString->data[0])
	{
		// '*' as first character indicates an embedded file
		return 5;
	}
	// first check whether we can directly load the file
	FILE* pFile = fopen(p_szString->data, "rb");
	if (pFile)fclose(pFile);
	else
	{
		// check whether we can use the directory of  the asset as relative base
		char szTemp[MAX_PATH * 2], tmp2[MAX_PATH * 2];
		strcpy(szTemp, g_szFileName);
		strcpy(tmp2, szTemp);
		char* szData = p_szString->data;
		if (*szData == '\\' || *szData == '/')++szData;
		char* szEnd = strrchr(szTemp, '\\');
		if (!szEnd)
		{
			szEnd = strrchr(szTemp, '/');
			if (!szEnd)szEnd = szTemp;
		}
		szEnd++;
		*szEnd = 0;
		strcat(szEnd, szData);
		pFile = fopen(szTemp, "rb");
		if (!pFile)
		{
			// convert the string to lower case
			for (unsigned int i = 0;; ++i)
			{
				if ('\0' == szTemp[i])break;
				szTemp[i] = (char)tolower(szTemp[i]);
			}
			if (TryLongerPath(szTemp, p_szString))return 1;
			*szEnd = 0;
			// search common sub directories
			strcat(szEnd, "tex\\");
			strcat(szEnd, szData);
			pFile = fopen(szTemp, "rb");
			if (!pFile)
			{
				if (TryLongerPath(szTemp, p_szString))return 1;
				*szEnd = 0;
				strcat(szEnd, "textures\\");
				strcat(szEnd, szData);
				pFile = fopen(szTemp, "rb");
				if (!pFile)
				{
					if (TryLongerPath(szTemp, p_szString))return 1;
				}
				// patch by mark sibly to look for textures files in the asset's base directory.
				const char *path = pcpy.data;
				const char *p = strrchr(path, '/');
				if (!p) p = strrchr(path, '\\');
				if (p) 
				{
					char *q = strrchr(tmp2, '/');
					if (!q) q = strrchr(tmp2, '\\');
					if (q) 
					{
						strcpy(q + 1, p + 1);
						if ((pFile = fopen(tmp2, "r"))) 
						{
							fclose(pFile);
							strcpy(p_szString->data, tmp2);
							p_szString->length = strlen(tmp2);
							return 1;
						}
					}
				}
				return 0;
			}
		}
		fclose(pFile);

		// copy the result string back to the aiString
		const size_t iLen = strlen(szTemp);
		size_t iLen2 = iLen + 1;
		iLen2 = iLen2 > MAXLEN ? MAXLEN : iLen2;
		memcpy(p_szString->data, szTemp, iLen2);
		p_szString->length = iLen;
	}
	return 1;
}
bool SkinnedMesh::InitMaterials(const aiScene* pScene, const std::string& Filename)
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
				std::string FullPath = Dir + "/" + Path.data;
				m_Textures[i] = new Texture(FullPath.c_str());
				if (!m_Textures[i]->Load(device, (aiScene*)pScene, (aiMaterial*)pMaterial))
				{
					MessageBoxA(NULL,"Error loading texture", FullPath.c_str(),MB_OK);
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
 void SkinnedMesh::BatchRender(ID3D11DeviceContext*& md3dImmediateContext)
{
	for (int i = 0; i < renderQueue.size(); i++)
	{

	}
}
void SkinnedMesh::Render(ID3D11DeviceContext*& md3dImmediateContext)
{
	XMMATRIX viewProj = m_Camera->ViewProj();

	md3dImmediateContext->IASetPrimitiveTopology(primitive_type);
	//md3dImmediateContext->RSSetState(WireframeRS);
	//绘制人物动画
	md3dImmediateContext->IASetInputLayout(mInputLayout);
	D3DX11_TECHNIQUE_DESC techDesc;
	BoneTransforms->SetMatrixArray(reinterpret_cast<const float*>(&Transforms[0]), 0, Transforms.size());
	for (int i = 0; i < m_Entries.size(); i++)
	{
		mTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			UINT stride = sizeof(SkinnedVertex);
			UINT offset = 0;
			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX rotation = XMMatrixRotationX(-0);
			XMMATRIX scale = XMMatrixScaling(0.24, 0.24, 0.24);
			XMMATRIX translation = XMMatrixTranslation(-180, 12, 100);
			world = world * rotation * translation * scale;
			XMMATRIX worldViewProj = world*viewProj;
			mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));
			DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
			ID3D11ShaderResourceView* tex = m_Textures[m_Entries[i].MaterialIndex]->mDiffuseMapSRV;
			DiffuseMap->SetResource(tex);
			md3dImmediateContext->IASetVertexBuffers(0, 1, &m_Entries[i].mVB, &stride, &offset);
			md3dImmediateContext->IASetIndexBuffer(m_Entries[i].mIB, DXGI_FORMAT_R32_UINT, 0);
			mTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->DrawIndexed(m_Entries[i].m_Indices.size(), 0, 0);
		}
	}
	md3dImmediateContext->RSSetState(0);
}
unsigned int SkinnedMesh::FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
	{
		if (AnimationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
		{
			return i;
		}
	}
	assert(0);
	return 0;
}
unsigned int SkinnedMesh::FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumRotationKeys > 0);
	for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
	{
		if (AnimationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
		{
			return i;
		}
	}
	assert(0);
	return 0;
}
unsigned int SkinnedMesh::FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumScalingKeys > 0);
	for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
	{
		if (AnimationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
		{
			return i;
		}
	}
	assert(0);
	return 0;
}
void SkinnedMesh::CalcInterpolatedPosition(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumPositionKeys == 1)
	{
		Out = pNodeAnim->mPositionKeys[0].mValue;
		return;
	}
	unsigned int PositionIndex = FindPosition(AnimationTime, pNodeAnim);
	unsigned int NextPositionIndex = (PositionIndex + 1);
	assert(NextPositionIndex < pNodeAnim->mNumPositionKeys);
	float DeltaTime = (float)(pNodeAnim->mPositionKeys[NextPositionIndex].mTime - pNodeAnim->mPositionKeys[PositionIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mPositionKeys[PositionIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
	const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}
void SkinnedMesh::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	// we need at least two values to interpolate...
	if (pNodeAnim->mNumRotationKeys == 1)
	{
		Out = pNodeAnim->mRotationKeys[0].mValue;
		return;
	}
	unsigned int RotationIndex = FindRotation(AnimationTime, pNodeAnim);
	unsigned int NextRotationIndex = (RotationIndex + 1);
	assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);
	float DeltaTime = (float)(pNodeAnim->mRotationKeys[NextRotationIndex].mTime - pNodeAnim->mRotationKeys[RotationIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;
	aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
	Out = Out.Normalize();
}
void SkinnedMesh::CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumScalingKeys == 1)
	{
		Out = pNodeAnim->mScalingKeys[0].mValue;
		return;
	}
	unsigned int ScalingIndex = FindScaling(AnimationTime, pNodeAnim);
	unsigned int NextScalingIndex = (ScalingIndex + 1);
	assert(NextScalingIndex < pNodeAnim->mNumScalingKeys);
	float DeltaTime = (float)(pNodeAnim->mScalingKeys[NextScalingIndex].mTime - pNodeAnim->mScalingKeys[ScalingIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
	const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}
static bool readaniminfo = false;
void SkinnedMesh::ReadNodeHeirarchy(float AnimationTime, const aiNode* pNode, const Matrix4f& ParentTransform)
{
	std::string NodeName(pNode->mName.data);
	std::string filename(pNode->mName.data);
	const aiAnimation* pAnimation = m_pScene->mAnimations[0];
	Matrix4f NodeTransformation(pNode->mTransformation);
	const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, NodeName);
	filename.append(".txt");
	if (pNodeAnim)
	{
		if (readaniminfo == false)
		{
			//WriteAnimInfo(filename.c_str(), pNodeAnim);
		}
		// Interpolate scaling and generate scaling transformation matrix
		aiVector3D Scaling;
		CalcInterpolatedScaling(Scaling, AnimationTime, pNodeAnim);
		Matrix4f ScalingM;
		ScalingM.InitScaleTransform(Scaling.x, Scaling.y, Scaling.z);
		// Interpolate rotation and generate rotation transformation matrix
		aiQuaternion RotationQ;
		CalcInterpolatedRotation(RotationQ, AnimationTime, pNodeAnim);
		Matrix4f RotationM = Matrix4f(RotationQ.GetMatrix());
		// Interpolate translation and generate translation transformation matrix
		aiVector3D Translation;
		CalcInterpolatedPosition(Translation, AnimationTime, pNodeAnim);
		Matrix4f TranslationM;
		TranslationM.InitTranslationTransform(Translation.x, Translation.y, Translation.z);
		// Combine the above transformations
		NodeTransformation = TranslationM * RotationM * ScalingM;
	}
	Matrix4f GlobalTransformation = ParentTransform * NodeTransformation;
	if (m_BoneMapping.find(NodeName) != m_BoneMapping.end())
	{
		unsigned int BoneIndex = m_BoneMapping[NodeName];
		m_BoneInfo[BoneIndex].FinalTransformation = m_GlobalInverseTransform * GlobalTransformation * m_BoneInfo[BoneIndex].BoneOffset;
	}
	for (unsigned int i = 0; i < pNode->mNumChildren; i++)
	{
		ReadNodeHeirarchy(AnimationTime, pNode->mChildren[i], GlobalTransformation);
	}
}
bool SkinnedMesh::WriteAnimInfo(const char * filename, const aiNodeAnim* animinfo)
{
	FILE * file;
	file = fopen(filename, "w");
	char buf[MAXLEN];
	if (file == NULL)
	{
		return false;
	}
	memset(buf, 0, MAXLEN);
	unsigned int positionkey_num = animinfo->mNumPositionKeys;
	unsigned int scalingkey_num = animinfo->mNumScalingKeys;
	unsigned int rotationkey_num = animinfo->mNumRotationKeys;
	unsigned int saclesize;
	//写入mesh个数
	sprintf(buf, "positionkey_num=%d,scalingkey_num=%d,rotationkey_num=%d\r\n", positionkey_num, scalingkey_num, rotationkey_num);
	fwrite(buf, strlen(buf), 1, file);
	//char* str = (char*)malloc();
	double keytime;
	aiVector3D keyposition;
	aiQuaternion keyquaternion;
	aiVector3D keyscaling;
	for (int i = 0; i < positionkey_num; i++)
	{
		memset(buf, 0, MAXLEN);
		keytime = animinfo->mPositionKeys[i].mTime;
		keyposition = animinfo->mPositionKeys[i].mValue;
		sprintf(buf, "keytime=%f,keyposition.x=%f,keyposition.y=%f,keyposition.z=%f\r\n", keytime, keyposition.x, keyposition.y, keyposition.z);
		fwrite(buf, strlen(buf), 1, file);
	}
	for (int i = 0; i < positionkey_num; i++)
	{
		memset(buf, 0, MAXLEN);
		keytime = animinfo->mRotationKeys[i].mTime;
		keyquaternion = animinfo->mRotationKeys[i].mValue;
		sprintf(buf, "keytime=%f,keyquaternion.w=%f,keyquaternion.x=%f,keyquaternion.y=%f,keyquaternion.z=%f\r\n", keytime, keyquaternion.w, keyposition.x, keyposition.y, keyposition.z);
		fwrite(buf, strlen(buf), 1, file);
	}
	for (int i = 0; i < positionkey_num; i++)
	{
		memset(buf, 0, MAXLEN);
		keytime = animinfo->mScalingKeys[i].mTime;
		keyscaling = animinfo->mScalingKeys[i].mValue;
		sprintf(buf, "keytime=%f,keyscaling.x=%f,keyscaling.y=%f,keyscaling.z=%f\r\n", keytime, keyscaling.x, keyscaling.y, keyscaling.z);
		fwrite(buf, strlen(buf), 1, file);
	}
	//写入顶点数量
	fclose(file);
	return false;
}
void SkinnedMesh::BoneTransform(float TimeInSeconds, std::vector<Matrix4f>& Transforms)
{
	Matrix4f Identity;
	Identity.InitIdentity();
	float TicksPerSecond = (float)(m_pScene->mAnimations[0]->mTicksPerSecond != 0 ? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f);
	float TimeInTicks = TimeInSeconds * TicksPerSecond;
	float start_frame = m_AnimationMaps[m_CurrentAction].StartIndex;
	float end_frame = start_frame + m_AnimationMaps[m_CurrentAction].Framenumber;

	float Animation_start_point = start_frame /4308.0*float(m_pScene->mAnimations[0]->mDuration);
	float Animation_end_point = end_frame /4308.0*float(m_pScene->mAnimations[0]->mDuration);
	float Animation_duration = Animation_end_point - Animation_start_point;
	float AnimationTime = fmod(TimeInTicks, Animation_duration);
	AnimationTime = AnimationTime + Animation_start_point;
	//float AnimationTime = fmod(TimeInTicks, /*float(m_pScene->mAnimations[0]->mDuration)*/10);
	ReadNodeHeirarchy(AnimationTime, m_pScene->mRootNode, Identity);
	readaniminfo = true;
	Transforms.resize(m_NumBones);
	for (int i = 0; i < m_NumBones; i++)
	{
		Transforms[i] =  m_BoneInfo[i].FinalTransformation;
	}
}
const aiNodeAnim* SkinnedMesh::FindNodeAnim(const aiAnimation* pAnimation, const std::string NodeName)
{
	char title[100];
	for (int i = 0; i < pAnimation->mNumChannels; i++)
	{
		const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
		//sprintf(title,"channel%d.txt",i);
		if (readaniminfo == false)
		{
			//WriteAnimInfo(title,pNodeAnim);
		}
		if (std::string(pNodeAnim->mNodeName.data) == NodeName)
		{
			return pNodeAnim;
		}
	}
	return NULL;
}