#include "Receiver.h"


Client::~Client() {
	
	closesocket(serverSock);

	WSACleanup();
}

bool Client::Init() {
	//윈속 초기화
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		return false;
	}

	//소켓 생성
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return false;
	}

	//소켓 설정
	memset(&serverAddr, 0x00, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	ULONG ulongAddr;
	InetPtonA(AF_INET, SERVER_IP, &ulongAddr);
	serverAddr.sin_addr.S_un.S_addr = ulongAddr;

	return true;
}

bool Client::Connection() {
	//서버와 커넥트
	if (connect(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr)) == INVALID_SOCKET) {
		return false;
	}

	return true;
}

bool Client::ReadData() {
	//문자열 수신
	if (recv(serverSock, rBuf, sizeof(rBuf), 0) > 0) {

	}
	else
		return false;

	return true;
}


char* Client::GetData() {
	return rBuf;
}
int Client::GetDataSize() {
	return BUFFER_SIZE;
}