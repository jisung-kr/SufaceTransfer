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
	struct WSAData mWsaData;	//윈속용
	
	SOCKET mSockServer;	//듣기용 소켓
	SOCKET mSockClient;	//클라이언트 연결용 소켓

	struct sockaddr_in mAddrServer;	//서버 듣기용 주소
	struct sockaddr_in mAddrClient;	//클라 연결용 주소

public:
	bool Init();
	void AcceptClient();

};
