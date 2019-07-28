#pragma once
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 3500
#define BUFFER_SIZE 1024

class Server {
	
public:
	Server() = default;
	virtual ~Server();


private:
	WSAData wsaData;

	SOCKET serverSock;
	SOCKET clientSock;

	sockaddr_in serverAddr;
	sockaddr_in clientAddr;

	char buf[BUFFER_SIZE] = { 0, };
	char rBuf[BUFFER_SIZE] = { 0, };

public:
	bool Init();
	void WaitForClient();
	void SendData(void* data, int size);

};