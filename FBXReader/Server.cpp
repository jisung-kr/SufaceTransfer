#include "Server.h"


using namespace std;

#define BUFFER_SIZE 1024
#define PORT 3500


Server::~Server() {
	closesocket(mSockServer);
	WSACleanup();
}



bool Server::Init() {
	//윈속 초기화
	if ((WSAStartup(MAKEWORD(2, 2), &mWsaData)) != 0) {
		cerr << "WSAStartup Error" << endl;
		return false;
	}

	//소켓 생성
	if ((mSockServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		cerr << "Socket Error" << endl;
		return false;
	}

	//소켓 설정
	memset(&mAddrServer, 0x00, sizeof(mAddrServer));
	mAddrServer.sin_family = AF_INET;
	mAddrServer.sin_port = htons(PORT);
	mAddrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//소켓 바인딩
	if ((bind(mSockServer, (sockaddr*)& mAddrServer, sizeof(mAddrServer))) == SOCKET_ERROR) {
		cerr << "Binding Error" << endl;
		return false;
	}

	//대기열 생성
	if ((listen(mSockServer, 5)) == SOCKET_ERROR) {
		cerr << "Listen Error" << endl;
		return false;
	}

	return true;
}


void Server::AcceptClient() {

	while (true) {
		cout << "대기 ....." << endl;

		int addrSize = sizeof(mAddrClient);
		mSockClient = accept(mSockServer, (sockaddr*)& mAddrClient, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &mAddrClient.sin_addr, str, sizeof(str));
		cout << "접속 : " << str << endl;

		while (true) {
			char buf[BUFFER_SIZE] = { 0, };
			char rBuf[BUFFER_SIZE] = { 0, };

			//문자열 수신
			if (recv(mSockClient, rBuf, sizeof(rBuf), 0) > 0) {
				//표시
				cout << "수신 : " << rBuf << endl;

				memcpy(buf, rBuf, sizeof(buf));

				//문자열 송신
				send(mSockClient, buf, sizeof(buf), 0);
			}
			else {
				cout << "클라이언트 연결 종료" << endl;
				break;
			}

		}

		closesocket(mSockClient);

	}
}


