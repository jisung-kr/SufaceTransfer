#pragma once

#include <WS2tcpip.h>

#include "Camera.h"


#pragma comment(lib, "ws2_32.lib")
 
#define PORT 3500
#define BUFFER_SIZE 1024


#define CLIENT_MAX_NUM 0


enum COMMAND {
	//임시 명령
	COMMAND_REQ_FRAME = 0,
	COMMAND_RES_FRAME = 1,
	COMMAND_INPUT_KEY,
	COMMAND_MAX
};

enum INPUT_TYPE {
	INPUT_KEY_W = 0,
	INPUT_KEY_S = 1,
	INPUT_KEY_A = 2,
	INPUT_KEY_D = 3,
	INPUT_MAX 
};

struct INPUT_DATA {
	INPUT_TYPE mInputType;
	float x;
	float y;
	float z;
};

struct HEADER {
	INT mDataLen;
	USHORT mCommand;
};

struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = COMMAND::COMMAND_REQ_FRAME;
	}

	CHEADER(USHORT command) {
		mDataLen = 0;
		mCommand = command;
	}
	CHEADER(USHORT command, UINT dataLen) {
		mDataLen = dataLen;
		mCommand = command;
	}
};

struct NETWORK_MSG {
	HEADER header;
	char* data;
};

class Client {
public:
	Client() = default;
	Client(SOCKET socket) : clientSock(socket) { memset(&clientAddr, 0x00, sizeof(sockaddr_in)); mCamera.SetPosition(0.0f, 2.0f, -30.0f); }
	Client(SOCKET socket, sockaddr_in addr) : clientSock(socket), clientAddr(addr) { mCamera.SetPosition(0.0f, 2.0f, -30.0f); }

	virtual ~Client();

public:
	Camera mCamera;

private:
	SOCKET clientSock = INVALID_SOCKET;
	sockaddr_in clientAddr;

public:
	HEADER reqHeader = { -1, };
	char* data = nullptr;
	int dataSize = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;	//CmdList Allocator
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;	//Command List

public:
	bool AllocDataMem(int size) {
		data = new char[size];
		dataSize = size;

		if (data != nullptr)
			memset(data, 0x00, size);

		return data != nullptr;
	}
	HEADER& GetHeader() { return reqHeader; }
	char* GetDataMem() { return data; }
	SOCKET GetClientSocket() { return clientSock; }
};


class Server {
	
public:
	Server() = default;
	virtual ~Server();


private:
	WSAData wsaData;

	SOCKET serverSock = INVALID_SOCKET;
	sockaddr_in serverAddr;

	std::vector<Client*> clients;

public:

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetBuffer;	//RenderTarget Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;	//DepthStencil Buffer

	bool Init();
	void WaitForClient();
	void CreateRTVDSV();

	bool SendMSG(int sockIndex, HEADER resHeader, void* data);


	bool RecvRequest(int sockIndex);
	bool Response(int sockIndex);


	UINT GetClientNum() { return (UINT)clients.size(); }
	std::vector<Client*>& GetClients() { return clients; }
	Client* GetClient(int index) { return clients[index]; }


};