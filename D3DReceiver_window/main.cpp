#include "Receiver.h"
#include "BitmapQueue.h"

#include <Windows.h>



HINSTANCE mhInst;	//�ν��Ͻ� �ڵ�
HWND mhMainWnd;	//���� ������ �ڵ�

LPCWSTR clsName = TEXT("D3DReceiver");	//������ �𷡽� ����

Client* client = nullptr;	//Ŭ���̾�Ʈ

UINT mClientWidth = 1280;
UINT mClientHeight = 720;


LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
void Render();


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {
	WNDCLASS wndCls;
	ZeroMemory(&wndCls, sizeof(wndCls));

	//���� ������[���� ������]
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

	//Ŭ���̾�Ʈ �ʱ�ȭ
	client = new Client();
	if (!client->Init()) {
		::MessageBoxA(mhMainWnd, "��Ʈ��ũ �ʱ�ȭ ����", "����", MB_OK);
		return 1;
	}

	//������ ����
	if (!client->Connection()) {
		::MessageBoxA(mhMainWnd, "��Ʈ��ũ Ŀ�ؼ� ����", "����", MB_OK);
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
			Render();	//������
		}
	}

	return 0;
}

void Render() {
	if (client != nullptr) {
		//�������� �����͸� �޾ƿ�
		HEADER header;
		header.command = COMMAND::COMMAND_REQUEST_FRAME;
		header.dataLen = 0;
		header.msgNum = 1;
		header.msgTotalNum = 1;

		unsigned char* data = nullptr;
		client->SendMSG(header, (char**)&data);

		//�����͸� �޾ƿ� ������
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