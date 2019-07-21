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

	WSAData mWsaData;	//���ӿ�
	
	SOCKET mSockServer;	//���� ����
	SOCKET mSockClient;	//Ŭ���̾�Ʈ ����� ����

	sockaddr_in mAddrServer;	//��� ���Ͽ� �ּ�
	sockaddr_in mAddrClient;	//Ŭ�� ���Ͽ� �ּ�

public:
	bool Init();
	void AcceptClient();

};