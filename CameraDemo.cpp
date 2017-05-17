//***************************************************************************************
// CameraDemo.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Demonstrates first person camera.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//      Press '1', '2', '3' for 1, 2, or 3 lights enabled.
//
//***************************************************************************************

#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "Effects.h"
#include "RenderStates.h"
#include "Vertex.h"
#include "Camera.h"
#include "skinnedmesh.h"
#include "mesh.h"
#include "D3DCompiler.h"

long long m_startTime;
bool start = false;
long long GetCurrentTimeMillis()
{
#ifdef WIN32    
	return GetTickCount();
#else
	timeval t;
	gettimeofday(&t, NULL);
	long long ret = t.tv_sec * 1000 + t.tv_usec / 1000;
	return ret;
#endif    
}

float GetRunningTime()
{
	float RunningTime = (float)((double)GetCurrentTimeMillis() - (double)m_startTime) / 1000.0f;
	return RunningTime;
}

class CameraApp : public D3DApp 
{
public:
	SkinnedMesh* skinnedmesh;
	Mesh* background_mesh;
	ID3DX11Effect* mFX;
	ID3DX11EffectTechnique* mTech;
	ID3DX11Effect* mStaticMeshFX;
	ID3DX11EffectTechnique* mStaticMeshTech;
	ID3DX11EffectMatrixVariable* mfxWorldViewProj;
	ID3DX11EffectShaderResourceVariable* DiffuseMap;
	ID3DX11EffectMatrixVariable* m_StaticMesh_fxWorldViewProj;
	ID3DX11EffectShaderResourceVariable* StaticMesh_DiffuseMap;
	ID3DX11EffectMatrixVariable* BoneTransforms;

	CameraApp(HINSTANCE hInstance);
	~CameraApp();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene(); 

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:
	void BuildShapeGeometryBuffers();
	void BuildSkullGeometryBuffers();

private:
	/*ID3D11Buffer* mShapesVB;
	ID3D11ShaderResourceView* mFloorTexSRV;
*/
	Camera mCam;
	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	CameraApp theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}
 
ID3D11InputLayout* mInputLayout;
const D3D11_INPUT_ELEMENT_DESC PosTexSkinned[4] =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{"WEIGHTS",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"BONEINDICES",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
};


ID3D11InputLayout* mInputLayout_staticmesh;
const D3D11_INPUT_ELEMENT_DESC PosTex[2] =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};


CameraApp::CameraApp(HINSTANCE hInstance)
: D3DApp(hInstance)
{
	mMainWndCaption = L"Camera Demo";
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;
	mCam.SetPosition(0.0f, 30.0f, -0.0f);	
}

CameraApp::~CameraApp()
{
	/*
	ReleaseCOM(mBrickTexSRV);*/

	//Effects::DestroyAll();
	//InputLayouts::DestroyAll(); 
}

bool CameraApp::Init()
{
	if(!D3DApp::Init())
		return false;

	// Must init Effects first since InputLayouts depend on shader signatures.
	//Effects::InitAll(md3dDevice);
	//InputLayouts::InitAll(md3dDevice);
	RenderStates::InitAll(md3dDevice);
/*	HR(D3DX11CreateShaderResourceViewFromFile(md3dDevice, 
		L"Textures/floor.dds", 0, 0, &mFloorTexSRV, 0 ));
	BuildSkullGeometryBuffers();
	*/
	DWORD shaderFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3D10Blob* compiledShader = 0;
	ID3D10Blob* compilationMsgs = 0;
	HRESULT hr;
	hr = D3DX11CompileFromFile(L"FX/color.fx", 0, 0, 0, "fx_5_0", shaderFlags,0, 0, &compiledShader, &compilationMsgs, 0);
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
	HR(D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(),
		0, md3dDevice, &mFX));

	// Done with compiled shader.
	ReleaseCOM(compiledShader);

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
	HR(D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(),0, md3dDevice, &mStaticMeshFX));

	// Done with compiled shader.
	ReleaseCOM(compiledShader);

	mTech = mFX->GetTechniqueByName("ColorTech");
	mfxWorldViewProj = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
	BoneTransforms = mFX->GetVariableByName("gBoneTransforms")->AsMatrix();

	mStaticMeshTech = mStaticMeshFX->GetTechniqueByName("StaticMeshTech");
	m_StaticMesh_fxWorldViewProj = mStaticMeshFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();

	D3DX11_PASS_DESC passDesc;
	mTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(PosTexSkinned, 4, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout));


	mStaticMeshTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(PosTex, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout_staticmesh));

	skinnedmesh = new SkinnedMesh();
	skinnedmesh->device = md3dDevice;
	const char filename[256] = "assert\\mesh\\DAORU.DAE";
	skinnedmesh->LoadMesh(filename);
	background_mesh = new Mesh();
	background_mesh->device = md3dDevice;
	const char filename1[256] = "assert\\mesh\\1V1.DAE";
	background_mesh->LoadMesh(filename1);
	return true;
}

void CameraApp::OnResize()
{
	D3DApp::OnResize();

	mCam.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void CameraApp::UpdateScene(float dt)
{
	// Control the camera.
	if( GetAsyncKeyState('W') & 0x8000 )
		mCam.Walk(10.0f*dt);

	if( GetAsyncKeyState('S') & 0x8000 )
		mCam.Walk(-10.0f*dt);

	if( GetAsyncKeyState('A') & 0x8000 )
		mCam.Strafe(-10.0f*dt);

	if( GetAsyncKeyState('D') & 0x8000 )
		mCam.Strafe(10.0f*dt);

	float RunningTime = GetRunningTime();

	skinnedmesh->BoneTransform(RunningTime, skinnedmesh->Transforms);
}

void CameraApp::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::Silver));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	
    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	md3dImmediateContext->RSSetState(RenderStates::WireframeRS);
	//md3dImmediateContext->OMSetDepthStencilState(RenderStates::MarkMirrorDSS, 1);
	//md3dImmediateContext->OMSetDepthStencilState(0, 0);
	UINT stride = sizeof(Vertex::Basic32);
    UINT offset = 0;

	mCam.UpdateViewMatrix();
 
	XMMATRIX view     = mCam.View();
	XMMATRIX proj     = mCam.Proj();
	XMMATRIX viewProj = mCam.ViewProj();

		//绘制背景场景环境
		D3DX11_TECHNIQUE_DESC techDesc;
		mStaticMeshTech = mStaticMeshFX->GetTechniqueByName("StaticMeshTech");
		m_StaticMesh_fxWorldViewProj = mStaticMeshFX->GetVariableByName("gWorldViewProj")->AsMatrix();
		StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
		md3dImmediateContext->IASetInputLayout(mInputLayout_staticmesh);
		for (int i = 0; i < background_mesh->m_Entries.size(); i++)
		{
			mStaticMeshTech->GetDesc(&techDesc);
			for (UINT p = 0; p < techDesc.Passes; ++p)
			{
				UINT stride = sizeof(Vertex::Basic32);
				UINT offset = 0;
				stride = sizeof(MeshVertex);
				offset = 0;
				XMMATRIX world = XMMatrixIdentity();
				XMMATRIX rotation = XMMatrixRotationX(-0);
				XMMATRIX translation = XMMatrixTranslation(-0, -0, -0);
				world = world * rotation * translation;
				XMMATRIX worldViewProj = world*view*proj;
				m_StaticMesh_fxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));
				StaticMesh_DiffuseMap = mStaticMeshFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
				ID3D11ShaderResourceView* tex = background_mesh->m_Textures[background_mesh->m_Entries[i].MaterialIndex]->mDiffuseMapSRV;
				StaticMesh_DiffuseMap->SetResource(tex);
				md3dImmediateContext->IASetVertexBuffers(0, 1, &background_mesh->m_Entries[i].mVB, &stride, &offset);
				md3dImmediateContext->IASetIndexBuffer(background_mesh->m_Entries[i].mIB, DXGI_FORMAT_R32_UINT, 0);
				mStaticMeshTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
				md3dImmediateContext->DrawIndexed(background_mesh->m_Entries[i].m_Indices.size(), 0, 0);
			}
		}
		md3dImmediateContext->RSSetState(0);
		//绘制人物动画
		md3dImmediateContext->IASetInputLayout(mInputLayout);
		BoneTransforms->SetMatrixArray(reinterpret_cast<const float*>(&skinnedmesh->Transforms[0]), 0, skinnedmesh->Transforms.size());
		for (int i = 0; i < skinnedmesh->m_Entries.size(); i++)
		{
			mTech->GetDesc(&techDesc);
			for (UINT p = 0; p < techDesc.Passes; ++p)
			{
				UINT stride = sizeof(Vertex::Basic32);
				UINT offset = 0;
				stride = sizeof(SkinnedVertex);
				offset = 0;
				XMMATRIX world = XMMatrixIdentity();
				XMMATRIX worldViewProj = world*view*proj;
				mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));
				DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
				ID3D11ShaderResourceView* tex = skinnedmesh->m_Textures[skinnedmesh->m_Entries[i].MaterialIndex]->mDiffuseMapSRV;
				DiffuseMap->SetResource(tex);
				md3dImmediateContext->IASetVertexBuffers(0, 1, &skinnedmesh->m_Entries[i].mVB, &stride, &offset);
				md3dImmediateContext->IASetIndexBuffer(skinnedmesh->m_Entries[i].mIB, DXGI_FORMAT_R32_UINT, 0);
				mTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
				md3dImmediateContext->DrawIndexed(skinnedmesh->m_Entries[i].m_Indices.size(), 0, 0);
			}
		}

	HR(mSwapChain->Present(0, 0));
}

void CameraApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void CameraApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void CameraApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if( (btnState & MK_LBUTTON) != 0 )
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		mCam.Pitch(dy);
		mCam.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void CameraApp::BuildShapeGeometryBuffers()
{

}
 
void CameraApp::BuildSkullGeometryBuffers()
{
	
}

