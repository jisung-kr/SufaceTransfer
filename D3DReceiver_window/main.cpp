#include "Receiver.h"

#include "GameTimer.h"

#include <Windows.h>
#include <thread>
#include <string>

HINSTANCE mhInst;	//인스턴스 핸들
HWND mhMainWnd;	//메인 윈도우 핸들

LPCWSTR clsName = TEXT("D3DReceiver");	//윈도우 쿨래스 네임

Client* client = nullptr;	//클라이언트

//UINT mClientWidth = 1280;
//UINT mClientHeight = 720;

UINT mClientWidth = 640;
UINT mClientHeight = 480;

std::thread* mNetworkReadThread = nullptr;
std::thread* mNetworkWriteThread = nullptr;
std::thread* mRenderingThread = nullptr;

GameTimer mTimer;

LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
void Render();
void CalculateFrameStatus();
void Input();

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

	if (!client->Connection()) {
		::MessageBoxA(mhMainWnd, "네트워크 커넥션 오류", "오류", MB_OK);
		return 1;
	}

	mTimer.Reset();

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
			Input();

			/*				*/
			if (mNetworkReadThread == nullptr) {
				mNetworkReadThread = new std::thread([&]() -> void {
					while (true) {
						client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));

						if (!client->SendMSG()) {
							delete client;
							client = nullptr;
							break;
						}

					}

					});
			}


			if (mNetworkWriteThread == nullptr) {
				mNetworkWriteThread = new std::thread([&]() -> void {
					while (true) {
						if (!client->RecvMSG()) {
							delete client;
							client = nullptr;
							break;
						}

					}

					});
			}

			

			/*
			if (client->rQueue.Size() < 10) {
				client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));

				if (!client->SendMSG()) {
					delete client;
					client = nullptr;
					break;
				}

				if (!client->RecvMSG()) {
					delete client;
					client = nullptr;
					break;
				}
			}
			*/
			mTimer.Tick();

			if (client->rQueue.Size() > 0) {
				CalculateFrameStatus();

				Render();	//렌더링
				delete client->rQueue.FrontItem();
				client->rQueue.PopItem();
			}

		}
	}


	return 0;
}

void Input() {

	if (GetAsyncKeyState('W') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_W;

		OutputDebugStringA("W 입력\n");
		client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_INPUT_KEY, dataSize), data));
		client->SendMSG();
	}

	if (GetAsyncKeyState('S') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_S;

		OutputDebugStringA("S 입력\n");
		client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_INPUT_KEY, dataSize), data));
		client->SendMSG();
	}

	if (GetAsyncKeyState('A') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_A;

		OutputDebugStringA("A 입력\n");
		client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_INPUT_KEY, dataSize), data));
		client->SendMSG();
	}

	if (GetAsyncKeyState('D') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_D;

		OutputDebugStringA("D 입력\n");
		client->wQueue.PushItem(new Packet(new CHEADER(COMMAND::COMMAND_INPUT_KEY, dataSize), data));
		client->SendMSG();
	}
}

void Render() {

	if (client != nullptr) {

		HDC hdc, hMemDC;
		//PAINTSTRUCT ps;
		HBITMAP hBitmap, hOldBitmap;


		hdc = GetDC(mhMainWnd);
		//hdc = BeginPaint(mhMainWnd, &ps);

		hMemDC = CreateCompatibleDC(hdc);
		hBitmap = CreateBitmap(640, 441, 1, 32, client->GetData());	//비트맵 사이즈 중요!!!
		//hBitmap = CreateCompatibleBitmap(hdc, 640, 441);	//비트맵 사이즈 중요!!!
		hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
		BitBlt(hdc, 0, 0, mClientWidth, mClientHeight, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBitmap);
		DeleteDC(hMemDC);
		DeleteObject(hBitmap);


		ReleaseDC(mhMainWnd, hdc);
		//EndPaint(mhMainWnd, &ps);


		//client->ReleaseBuffer();
	}

}

void CalculateFrameStatus() {
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = std::wstring(clsName) +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
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
