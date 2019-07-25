#include "Server.h"


using namespace std;

#define BUFFER_SIZE 1024
#define PORT 3500


Server::~Server() {
	closesocket(mSockServer);
	WSACleanup();
}



bool Server::Init() {
	//���� �ʱ�ȭ
	if ((WSAStartup(MAKEWORD(2, 2), &mWsaData)) != 0) {
		return false;
	}

	//���� ����
	if ((mSockServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		return false;
	}

	//���� ����
	memset(&mAddrServer, 0x00, sizeof(mAddrServer));
	mAddrServer.sin_family = AF_INET;
	mAddrServer.sin_port = htons(PORT);
	mAddrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//���� ���ε�
	if ((bind(mSockServer, (sockaddr*)& mAddrServer, sizeof(mAddrServer))) == SOCKET_ERROR) {
		return false;
	}

	//��⿭ ����
	if ((listen(mSockServer, 5)) == SOCKET_ERROR) {
		return false;
	}

	return true;
}


void Server::AcceptClient() {

	while (true) {

		int addrSize = sizeof(mAddrClient);
		mSockClient = accept(mSockServer, (sockaddr*)& mAddrClient, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &mAddrClient.sin_addr, str, sizeof(str));

		while (true) {
			char buf[BUFFER_SIZE] = { 0, };
			char rBuf[BUFFER_SIZE] = { 0, };

			//���ڿ� ����
			if (recv(mSockClient, rBuf, sizeof(rBuf), 0) > 0) {

				memcpy(buf, rBuf, sizeof(buf));

				//���ڿ� �۽�
				send(mSockClient, buf, sizeof(buf), 0);
			}
			else {
				break;
			}

		}

		closesocket(mSockClient);

	}
}


