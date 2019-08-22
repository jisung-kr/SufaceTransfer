#pragma once

#include <WS2tcpip.h>

#include "Camera.h"


#pragma comment(lib, "ws2_32.lib")
 
#define PORT 3500
#define BUFFER_SIZE 1024


#define CLIENT_MAX_NUM 1

enum COMMAND {
	//임시 명령
	COMMAND_REQUEST_FRAME = 0,
	COMMAND_REQUEST_FRAME_ACK = 1
};

struct HEADER {
	unsigned int dataLen;
	unsigned short command;
	unsigned char msgNum;
	unsigned char msgTotalNum;
};

struct NETWORK_MSG {
	HEADER header;
	char* data;
};

class Client {
public:
	Client() = default;
	Client(SOCKET socket) : clientSock(socket) { memset(&clientAddr, 0x00, sizeof(sockaddr_in)); mCamera.SetPosition(0.0f, 2.0f, -20.0f); }
	Client(SOCKET socket, sockaddr_in addr) : clientSock(socket), clientAddr(addr) { mCamera.SetPosition(0.0f, 2.0f, -20.0f); }

	virtual ~Client();

public:
	Camera mCamera;

private:
	SOCKET clientSock = INVALID_SOCKET;
	sockaddr_in clientAddr;

	char* data = nullptr;
	int dataSize = 0;

public:
	bool AllocDataMem(int size) {
		data = new char[size];

		if (data != nullptr)
			memset(data, 0x00, size);

		return data != nullptr;
	}
	char* GetDataMem() { return data; }
	SOCKET GetClientSocket() { return clientSock; }
	void SetDataSize(int size) { dataSize = size; }
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
	bool Init();
	void WaitForClient();
	bool SendMSG(int sockIndex, void* data, int size);

	bool RecvData(int sockIndex);


	UINT GetClientNum() { return (UINT)clients.size(); }
	std::vector<Client*>& GetClients() { return clients; }
};