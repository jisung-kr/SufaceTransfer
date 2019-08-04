#include "Receiver.h"
#include "BitmapQueue.h"

#include <Windows.h>



HINSTANCE mhInst;	//인스턴스 핸들
HWND mhMainWnd;	//메인 윈도우 핸들

LPCWSTR clsName = TEXT("D3DReceiver");	//윈도우 쿨래스 네임

Client* client = nullptr;	//클라이언트

UINT mClientWidth = 1280;
UINT mClientHeight = 720;


LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
void Render();


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

	//클라이언트 초기화
	client = new Client();
	if (!client->Init()) {
		::MessageBoxA(mhMainWnd, "네트워크 초기화 오류", "오류", MB_OK);
		return 1;
	}

	//서버에 접속
	if (!client->Connection()) {
		::MessageBoxA(mhMainWnd, "네트워크 커넥션 오류", "오류", MB_OK);
		return 1;
	}


	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();	//렌더링
		}
	}

	return 0;
}

void Render() {
	if (client != nullptr) {
		//서버에서 데이터를 받아옴
		HEADER header;
		header.command = COMMAND::COMMAND_REQUEST_FRAME;
		header.dataLen = 0;
		header.msgNum = 1;
		header.msgTotalNum = 1;

		unsigned char* data = nullptr;
		client->SendMSG(header, (char**)&data);

		//데이터를 받아온 상태임
		HDC hdc, hMemDC;
		//PAINTSTRUCT ps;
		HBITMAP hBitmap, hOldBitmap;


		hdc = GetDC(mhMainWnd);
		//hdc = BeginPaint(mhMainWnd, &ps);


		hMemDC = CreateCompatibleDC(hdc);
		hBitmap = CreateBitmap(mClientWidth, mClientHeight, 1, 32, data);
		hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
		BitBlt(hdc, 0, 0, mClientWidth, mClientHeight, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBitmap);
		DeleteDC(hMemDC);
		DeleteObject(hBitmap);
		

		ReleaseDC(mhMainWnd, hdc);

		//EndPaint(mhMainWnd, &ps);

		//delete data;
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