#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define PORT 3500
//#define SERVER_IP "127.0.0.1"
#define SERVER_IP "61.73.65.218"

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
	virtual ~Client();

private:
	WSAData wsaData;

	SOCKET serverSock;
	sockaddr_in serverAddr;

	void* data = nullptr;

public:
	bool Init();
	bool Connection();
	bool ReadData();
	char* GetData();

	void ReleaseBuffer();
};