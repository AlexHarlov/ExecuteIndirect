#pragma once
//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "DirectXHelper.h"
#include "GameTimer.h"
#include "Renderer.h"
#include "Camera.h"
#include <windows.h>
#include <windowsx.h>

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

namespace ExecuteIndirect {
	class D3DApp
	{
	public:

		D3DApp(HINSTANCE hInstance);
		D3DApp(const D3DApp& rhs) = delete;
		D3DApp& operator=(const D3DApp& rhs) = delete;
		virtual ~D3DApp();

	public:

		static D3DApp* GetApp();

		HINSTANCE AppInst()const;
		HWND      MainWnd()const;

		int Run();
		bool Initialize();

		LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void OnResize();
		void Update(const GameTimer& gt);
		void Draw(const GameTimer& gt);

		void OnMouseDown(WPARAM btnState, int x, int y);
		void OnMouseUp(WPARAM btnState, int x, int y);
		void OnMouseMove(WPARAM btnState, int x, int y);

	protected:

		bool InitMainWindow();
		void CalculateFrameStats();

		void OnKeyboardInput(const GameTimer & gt);

	private:

		static D3DApp* mApp;

		HINSTANCE mhAppInst = nullptr; // application instance handle
		HWND      mhMainWnd = nullptr; // main window handle
		bool      mAppPaused = false;  // is the application paused?
		bool      mMinimized = false;  // is the application minimized?
		bool      mMaximized = false;  // is the application maximized?
		bool      mResizing = false;   // are the resize bars being dragged?
		bool      mFullscreenState = false;// fullscreen enabled
		POINT mLastMousePos;
										 
		std::unique_ptr<Renderer> m_sceneRenderer;
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<Camera> m_Camera;
		GameTimer m_timer;

		// Derived class should set these in derived constructor to customize starting values.
		std::wstring mMainWndCaption = L"Execute Indirect";
		int mClientWidth = 1024;
		int mClientHeight = 768;
		UINT m_totalDrawnInstances;
	};
}


