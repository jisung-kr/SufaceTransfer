#include "Receiver.h"

#include "GameTimer.h"

#include <WindowsX.h>
#include <thread>
#include <string>
#include <DirectXMath.h>

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
void Input(GameTimer& timer);
void OnMouseDown(WPARAM btnState, int x, int y);
void OnMouseUp(WPARAM btnState, int x, int y);
void OnMouseMove(WPARAM btnState, int x, int y);

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

	client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));
	//client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));

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

			/*						*/
			if (mNetworkReadThread == nullptr) {
				mNetworkReadThread = new std::thread([&]() -> void {
					while (true) {

						if (!client->SendMSG()) {
							delete client;
							client = nullptr;
							OutputDebugStringA("SendMSG Error\n");
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
							OutputDebugStringA("RecvMSG Error\n");
							break;
						}

					}

				});
			}
	
			if (mRenderingThread == nullptr) {
				mRenderingThread = new std::thread([&]() -> void {
					while (true) {
						mTimer.Tick();

						if (client->SizeRQueue() > 0) {
							Input(mTimer);

							CalculateFrameStatus();

							Render();	//렌더링

							client->PopPacketRQueue();

							client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));
						}
					}
					
				});
			}


		}
	}


	return 0;
}

void Input(GameTimer& timer) {

	if (GetAsyncKeyState('W') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_W;
		data->deltaTime = timer.DeltaTime() ;

		OutputDebugStringA("W 입력\n");
		client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('S') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_S;
		data->deltaTime = timer.DeltaTime();

		OutputDebugStringA("S 입력\n");
		client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('A') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_A;
		data->deltaTime = timer.DeltaTime() ;

		OutputDebugStringA("A 입력\n");
		client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('D') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_D;
		data->deltaTime = timer.DeltaTime();

		OutputDebugStringA("D 입력\n");
		client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
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
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);

}
POINT lastMousePos;

void OnMouseDown(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		lastMousePos.x = x;
		lastMousePos.y = y;

		SetCapture(mhMainWnd);
	}
}
void OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos.y));

		//dx, dy각도 만큼 카메라 회전
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_MOUSE_MOVE;
		data->x = dx;
		data->y = dy;

		//data->deltaTime = timer.DeltaTime();

		OutputDebugStringA("Mouse 입력\n");
		client->PushPacketWQueue(new Packet(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	lastMousePos.x = x;
	lastMousePos.y = y;
}