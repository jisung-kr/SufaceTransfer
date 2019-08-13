#include "Network.h"


Server::~Server() {
	if (serverSock != INVALID_SOCKET)
		closesocket(serverSock);
	if(clientSock != INVALID_SOCKET)
		closesocket(clientSock);

	WSACleanup();
}

bool Server::Init() {

	//윈속 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		return false;
	}

	//소켓 생성
	if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		return false;
	}

	//소켓 설정
	memset(&serverAddr, 0x00, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//소켓 바인딩
	if ((bind(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr))) == SOCKET_ERROR) {
		return false;
	}

	//대기열 생성
	if ((listen(serverSock, 5)) == SOCKET_ERROR) {
		return false;
	}

	return true;
}


//일단 싱글스레드로
void Server::WaitForClient() {
	int addrSize = sizeof(clientAddr);
	clientSock = accept(serverSock, (sockaddr*)& clientAddr, &addrSize);

	char str[256];
	InetNtopA(AF_INET, &clientAddr.sin_addr, str, sizeof(str));
	/*
	while (true) {
		int addrSize = sizeof(clientAddr);
		clientSock = accept(serverSock, (sockaddr*)& clientAddr, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &clientAddr.sin_addr, str, sizeof(str));
	}
	*/
}

bool Server::IsInvalidClientSocket() {
	return clientSock == INVALID_SOCKET;
}


bool Server::SendData(void* data, int size) {

	//데이터 송신
	//현재 버퍼 사이즈 만큼만 데이터 전송
	//후에 버퍼 사이즈 만큼 보내기 위해 프로토콜 생성해야함
	if (clientSock != INVALID_SOCKET) {
		unsigned int totSize = 0;
		unsigned int nowSize = 0;
		while (true) {
			nowSize = send(clientSock, ((char*)data) + totSize, size - totSize, 0);
			
			if (nowSize < 0) {
				//클라이언트가 접속 종료됨
				closesocket(clientSock);
				clientSock = INVALID_SOCKET;
				return false;
			}
			else {
				totSize += nowSize;
			}

			if (totSize >= size)
				break;
		}
	}

	return true;

}