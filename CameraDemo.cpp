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
 
CameraApp::CameraApp(HINSTANCE hInstance)
: D3DApp(hInstance)
{
	mMainWndCaption = L"Camera Demo";
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;
	mCam.SetPosition(0.0f, 10.0f, 20.0f);	
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
	skinnedmesh = new SkinnedMesh();
	skinnedmesh->Init(md3dDevice);
	skinnedmesh->LoadMesh("assert\\mesh\\cloud_all_action.DAE");

	AnimationFrame af;

	af.StartIndex = 0;
	af.Framenumber = 62;
	skinnedmesh->m_AnimationMaps["stand"] = af;

	af.StartIndex = 64;
	af.Framenumber = 81 - 64;
	skinnedmesh->m_AnimationMaps["run"] = af;

	af.StartIndex = 1820;
	af.Framenumber = 1861 - 1820;
	skinnedmesh->m_AnimationMaps["ºá¿³"] = af;

	af.StartIndex = 1862;
	af.Framenumber = 1906 - 1862;
	skinnedmesh->m_AnimationMaps["Ìø¿³"] = af;

	af.StartIndex = 2650;
	af.Framenumber = 1906 - 1862;
	skinnedmesh->m_AnimationMaps["´óÁ¦¿³µØ"] = af;

	af.StartIndex = 2702;
	af.Framenumber = 2801 - 2702;
	skinnedmesh->m_AnimationMaps["´óÕÐ_Ðý×ªµ¶"] = af;

	af.StartIndex = 752;
	af.Framenumber = 796 - 752;
	skinnedmesh->m_AnimationMaps["µØ¹ö"] = af;

	af.StartIndex = 1264;
	af.Framenumber = 1313 - 1264;
	skinnedmesh->m_AnimationMaps["±»×²»÷"] = af;

	af.StartIndex = 2513;
	af.Framenumber = 2604 - 2513;
	skinnedmesh->m_AnimationMaps["Á¬»÷1"] = af;

	af.StartIndex = 2962;
	af.Framenumber = 3204 - 2962;
	skinnedmesh->m_AnimationMaps["²åµ¶"] = af;

	skinnedmesh->m_CurrentAction = "stand";
	skinnedmesh->m_Camera = &mCam;

	/*background_mesh = new Mesh();
	background_mesh->Init(md3dDevice);
	background_mesh->m_Camera = &mCam;
	background_mesh->LoadMesh("assert\\mesh\\1V1.DAE");*/
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

	mCam.UpdateViewMatrix();
	float RunningTime = GetRunningTime();
	XMMATRIX world = XMMatrixIdentity();
	XMFLOAT4X4 worldpos;
	XMStoreFloat4x4(&worldpos, world);
	skinnedmesh->Update(dt, worldpos);
	if (GetAsyncKeyState('M') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "run";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('Z') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "stand";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('X') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "ºá¿³";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('C') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "Ìø¿³";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('V') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "´óÁ¦¿³µØ";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('B') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "´óÕÐ_Ðý×ªµ¶";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('1') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "µØ¹ö";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('2') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "±»×²»÷";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('3') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "Á¬»÷1";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	else if (GetAsyncKeyState('4') & 0x8000)
	{
		skinnedmesh->m_CurrentAction = "²åµ¶";
		RunningTime = 0;
		m_startTime = (double)GetCurrentTimeMillis();
	}
	
	skinnedmesh->BoneTransform(RunningTime, skinnedmesh->Transforms);
}
void CameraApp::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::Silver));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
    //md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//md3dImmediateContext->RSSetState(RenderStates::WireframeRS);
	//md3dImmediateContext->OMSetDepthStencilState(RenderStates::MarkMirrorDSS, 1);
	//md3dImmediateContext->OMSetDepthStencilState(0, 0);
	//background_mesh->Render(md3dImmediateContext);
	skinnedmesh->Render(md3dImmediateContext);

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

