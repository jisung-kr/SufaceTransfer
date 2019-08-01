#include <Windows.h>

HINSTANCE mhInst;	//인스턴스 핸들
HWND mhMainWnd;	//메인 윈도우 핸들

LPCWSTR clsName = TEXT("D3DReceiver");	//윈도우 쿨래스 네임

LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {
	WNDCLASS wndCls;
	ZeroMemory(&wndCls, sizeof(wndCls));

	//메인 윈도우[메인 윈도우]
	wndCls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndCls.hCursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
	wndCls.hIcon = (HICON)LoadIcon(NULL, IDI_APPLICATION);
	wndCls.hInstance = mhInst;
	wndCls.lpfnWndProc = WndProc;
	wndCls.lpszClassName = clsName;
	wndCls.lpszMenuName = NULL;
	wndCls.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wndCls);


	UINT mClientWidth = 1280;
	UINT mClientHeight = 720;

	mhMainWnd = CreateWindow(clsName,
		clsName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	//xPos
		CW_USEDEFAULT,	//yPos
		mClientWidth,	//Width
		mClientHeight,	//Height
		NULL,
		NULL,
		mhInst,
		NULL);

	ShowWindow(mhMainWnd, SW_SHOW);

	MSG msg = { 0 };

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

		}
	}


}


LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {

	switch (iMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);

}