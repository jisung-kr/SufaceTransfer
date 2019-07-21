#pragma once

#include "D3D12WND.h"
#include <Windowsx.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class MainWindow {
public:
	MainWindow(HINSTANCE hInst) : mhInst(hInst) { instance = this; }
	virtual ~MainWindow() = default;
	MainWindow& operator=(MainWindow& rhs) = delete;
	MainWindow(const MainWindow& rhs) = delete;

private:
	static MainWindow* instance;

	HINSTANCE mhInst;
	HWND mhMainWnd;	// D3D 표시메인 윈도우

	UINT mClientWidth = 800;
	UINT mClientHeight = 600;

	LPCWSTR clsName = TEXT("D3D App");	//Window Class Name
	std::wstring mMainWndCaption = TEXT("D3D App");	//Window Name

	MSG mMsg = {};	//Msg

	D3D12WND* d3dApp = nullptr;

public:
	static MainWindow* GetInstance();

	bool Initialize();
	int Run();

	LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
};