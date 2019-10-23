#include "MainWindow.h"
#include "D3DUtil.h"



int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		//파일 입출력
		FILE* config = fopen("server_config.txt", "rt");
		if (config == nullptr) {
			MessageBoxA(NULL, "Can't Open File", "Error", MB_OK);
			return -1;
		}

		char serverPort[20];
		char maxClientNum[20];
		char windowWidth[20];
		char windowHeight[20];

		unsigned short serverPort_short;
		UINT clientNum;
		UINT width, height;

		fscanf_s(config, "%*[Server_Port=]%s\n", serverPort, sizeof(serverPort));
		fscanf_s(config, "%*[MaxClientNum=]%s\n", maxClientNum, sizeof(maxClientNum));
		fscanf_s(config, "%*[WindowWidth=]%s\n", windowWidth, sizeof(windowWidth));
		fscanf_s(config, "%*[WindowHeight=]%s\n", windowHeight, sizeof(windowHeight));

		serverPort_short = atoi(serverPort);
		clientNum = atoi(maxClientNum);
		width = atoi(windowWidth);
		height = atoi(windowHeight);

		fclose(config);

		MainWindow theMain(hInst, width, height);

		if (!theMain.Initialize(clientNum, serverPort_short))
			return 0;

		return theMain.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}





}