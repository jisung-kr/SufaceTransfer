#pragma once

#include "D3D12WND.h"

class MainWindow {
public:
	MainWindow(HINSTANCE hInst, UINT width, UINT height) : mhInst(hInst), mClientWidth(width), mClientHeight(height) { instance = this; }
	virtual ~MainWindow() = default;
	MainWindow& operator=(MainWindow& rhs) = delete;
	MainWindow(const MainWindow& rhs) = delete;

private:
	static MainWindow* instance;

	HINSTANCE mhInst;
	HWND mhMainWnd;	// D3D ǥ�ø��� ������

	
	UINT mClientWidth;
	UINT mClientHeight;

	LPCWSTR clsName = TEXT("D3D App");	//Window Class Name
	std::wstring mMainWndCaption = TEXT("D3D App");	//Window Name

	MSG mMsg = {};	//Msg

	//D3D12 ���� ȭ��
	D3D12WND* d3dApp = nullptr;


public:
	static MainWindow* GetInstance();

	bool Initialize(UINT clientNum, USHORT port);
	int Run();

	LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
};