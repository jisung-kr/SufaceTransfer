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
		cerr << "WSAStartup Error" << endl;
		return false;
	}

	//���� ����
	if ((mSockServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		cerr << "Socket Error" << endl;
		return false;
	}

	//���� ����
	memset(&mAddrServer, 0x00, sizeof(mAddrServer));
	mAddrServer.sin_family = AF_INET;
	mAddrServer.sin_port = htons(PORT);
	mAddrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//���� ���ε�
	if ((bind(mSockServer, (sockaddr*)& mAddrServer, sizeof(mAddrServer))) == SOCKET_ERROR) {
		cerr << "Binding Error" << endl;
		return false;
	}

	//��⿭ ����
	if ((listen(mSockServer, 5)) == SOCKET_ERROR) {
		cerr << "Listen Error" << endl;
		return false;
	}

	return true;
}


void Server::AcceptClient() {

	while (true) {
		cout << "��� ....." << endl;

		int addrSize = sizeof(mAddrClient);
		mSockClient = accept(mSockServer, (sockaddr*)& mAddrClient, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &mAddrClient.sin_addr, str, sizeof(str));
		cout << "���� : " << str << endl;

		while (true) {
			char buf[BUFFER_SIZE] = { 0, };
			char rBuf[BUFFER_SIZE] = { 0, };

			//���ڿ� ����
			if (recv(mSockClient, rBuf, sizeof(rBuf), 0) > 0) {
				//ǥ��
				cout << "���� : " << rBuf << endl;

				memcpy(buf, rBuf, sizeof(buf));

				//���ڿ� �۽�
				send(mSockClient, buf, sizeof(buf), 0);
			}
			else {
				cout << "Ŭ���̾�Ʈ ���� ����" << endl;
				break;
			}

		}

		closesocket(mSockClient);

	}
}


