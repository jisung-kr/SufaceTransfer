#pragma once
#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>


#pragma comment(lib, "ws2_32.lib")


class Server {
public:
	Server() = default;
	virtual ~Server();

private:
	WSAData mWsaData;	//윈속용
	
	SOCKET mSockServer;	//듣기용 소켓
	SOCKET mSockClient;	//클라이언트 연결용 소켓

	sockaddr_in mAddrServer;	//듣기 소켓용 주소
	sockaddr_in mAddrClient;	//클라 소켓용 주소

public:
	bool Init();
	void AcceptClient();

};
