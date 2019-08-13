#pragma once

#include <WS2tcpip.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

#define PORT 3500
#define BUFFER_SIZE 1024

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



class Server {
	
public:
	Server() = default;
	virtual ~Server();


private:
	WSAData wsaData;

	SOCKET serverSock = INVALID_SOCKET;
	SOCKET clientSock = INVALID_SOCKET;

	sockaddr_in serverAddr;
	sockaddr_in clientAddr;

	char buf[BUFFER_SIZE] = { 0, };
	char rBuf[BUFFER_SIZE] = { 0, };

public:
	bool Init();
	void WaitForClient();
	bool SendData(void* data, int size);
	bool IsInvalidClientSocket();

};