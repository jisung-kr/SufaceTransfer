#include "MainWindow.h"



MainWindow* MainWindow::instance = nullptr;

LRESULT SendProc_M(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

MainWindow* MainWindow::GetInstance() {
	return instance;
}

bool MainWindow::Initialize(UINT clientNum, USHORT port) {
	WNDCLASS wndCls;
	ZeroMemory(&wndCls, sizeof(wndCls));

	//메인 윈도우[메인 윈도우]
	wndCls.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	wndCls.hCursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
	wndCls.hIcon = (HICON)LoadIcon(NULL, IDI_APPLICATION);
	wndCls.hInstance = mhInst;
	wndCls.lpfnWndProc = SendProc_M;
	wndCls.lpszClassName = clsName;
	wndCls.lpszMenuName = NULL;
	wndCls.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wndCls);

	//윈도우 작업영역을 지정한 값만큼 만들기 위해 비작업영억의 크기 구하기
	RECT Rect;
	ZeroMemory(&Rect, sizeof(RECT));
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, false);
	unsigned int additionalWidth = Rect.right - Rect.left;
	unsigned int additionalHeight = Rect.bottom - Rect.top;

	mhMainWnd = CreateWindow(clsName,
		mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	//xPos
		CW_USEDEFAULT,	//yPos
		mClientWidth + additionalWidth,	//Width
		mClientHeight + additionalHeight,	//Height
		NULL,
		NULL,
		mhInst,
		NULL);

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	d3dApp = new D3D12WND(mhMainWnd);
	d3dApp->InitDirect3D(clientNum, port);
	d3dApp->OnResize();


	return true;
}

int MainWindow::Run() {
	MSG msg = { 0 };

	d3dApp->mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			/**/
			d3dApp->mTimer.Tick();

			if (!d3dApp->mAppPaused)
			{
				//클라이언트로부터 요청 수신
				d3dApp->RecvRequest();

				//클라이언트로부터 프레임 요청이 오기 전 까지 입력 펌프를 만들어 입력 처리
				d3dApp->InputPump(d3dApp->mTimer);

				d3dApp->CalculateFrameStatus();
				d3dApp->Update(d3dApp->mTimer);
				d3dApp->Draw(d3dApp->mTimer);
				
				//요청에 따라서 프레임 송신
				d3dApp->SendFrame();

			}
			else
			{
				Sleep(100);
			}
			
		}
	}

	return (int)msg.wParam;
}


LRESULT SendProc_M(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	return MainWindow::GetInstance()->WndProc(hWnd, iMsg, wParam, lParam);
}


LRESULT MainWindow::WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {

	switch (iMsg)
	{
	case WM_CREATE:
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// WM_ACTIVATE is sent when the window is activated or deactivated.  
	// We pause the game when the window is deactivated and unpause it 
	// when it becomes active.  
	case WM_ACTIVATE:
		/**/
		if (d3dApp) {
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				d3dApp->mAppPaused = true;
				d3dApp->mTimer.Stop();
			}
			else
			{
				d3dApp->mAppPaused = false;
				d3dApp->mTimer.Start();
			}
		}

		
		return 0;
		
		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		/*		*/
		if (d3dApp && d3dApp->GetD3DDevice().Get())
		{
			if (wParam == SIZE_MINIMIZED)
			{
				d3dApp->mAppPaused = true;
				d3dApp->mMinimized = true;
				d3dApp->mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				d3dApp->mAppPaused = false;
				d3dApp->mMinimized = false;
				d3dApp->mMaximized = true;
				d3dApp->OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (d3dApp->mMinimized)
				{
					d3dApp->mAppPaused = false;
					d3dApp->mMinimized = false;
					d3dApp->OnResize();
				}

				// Restoring from maximized state?
				else if (d3dApp->mMaximized)
				{
					d3dApp->mAppPaused = false;
					d3dApp->mMaximized = false;
					d3dApp->OnResize();
				}
				else if (d3dApp->mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					d3dApp->OnResize();
				}
			}
		}

		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		d3dApp->mAppPaused = true;
		d3dApp->mResizing = true;
		d3dApp->mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		d3dApp->mAppPaused = false;
		d3dApp->mResizing = false;
		d3dApp->mTimer.Start();
		d3dApp->OnResize();
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		d3dApp->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		d3dApp->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		d3dApp->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}

		return 0;
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);

 }