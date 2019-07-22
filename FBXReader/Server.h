#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")


class Server {
public:
	Server() = default;
	virtual ~Server();

private:
	struct WSAData mWsaData;
	
	SOCKET mSockServer;
	SOCKET mSockClient;	

	struct sockaddr_in mAddrServer;
	struct sockaddr_in mAddrClient;

public:
	bool Init();
	void AcceptClient();

};
