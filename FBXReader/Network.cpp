#include "Network.h"


Client::~Client() {
	if (clientSock != INVALID_SOCKET)
		closesocket(clientSock);
}


Server::~Server() {
	if (serverSock != INVALID_SOCKET)
		closesocket(serverSock);

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


void Server::WaitForClient() {

	SOCKET tempSock = INVALID_SOCKET;
	sockaddr_in tempAddr;
	/*
	int addrSize = sizeof(tempAddr);
	tempSock = accept(serverSock, (sockaddr*)&tempAddr, &addrSize);

	char str[256];
	InetNtopA(AF_INET, &tempAddr.sin_addr, str, sizeof(str));

	clients.push_back(new Client(tempSock, tempAddr));

	*/
	int i = 0;
	while (i < CLIENT_MAX_NUM) {
		int addrSize = sizeof(tempAddr);
		tempSock = accept(serverSock, (sockaddr*)& tempAddr, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &tempAddr.sin_addr, str, sizeof(str));

		clients.push_back(new Client(tempSock, tempAddr));
		++i;
	}

}


bool Server::RecvData(int sockIndex) {
	HEADER header = { 0, };
	unsigned int totSize = 0;
	unsigned int nowSize = 0;
	char str[256];

	//헤더 받아오기
	while (true) {
		nowSize = recv(serverSock, ((char*)&header) + totSize, sizeof(HEADER), 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= sizeof(HEADER))
				break;
		}
		else {
			OutputDebugStringA("헤더 수신 실패\n");
			return false;
		}
	}

	//버퍼에 데이터 받아오기
	unsigned int size = (unsigned int)ntohl(header.dataLen);

	if (size < 0)
		return false;

	OutputDebugStringA("헤더 수신 성공\n");

	//네트워크 사이즈 변수 리셋
	totSize = 0;
	nowSize = 0;

	//해당 클라이언트 클래스에 data할당 및 size저장
	clients[sockIndex]->AllocDataMem(size);
	clients[sockIndex]->SetDataSize(size);

	while (true) {
		nowSize = recv(serverSock, ((char*)clients[sockIndex]->GetDataMem()) + totSize, size - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			wsprintfA(str, "현재 수신된 데이터 %d / %d\n", totSize, size);
			OutputDebugStringA(str);

			if (totSize >= size)
				break;
		}
		else {
			OutputDebugStringA("데이터 실패\n");
			return false;
		}
	}

	OutputDebugStringA("수신 완료!\n");
	return true;
}


bool Server::SendMSG(int sockIndex, void* data, int size) {
	/*	*/
	//데이터 송신
	//현재 버퍼 사이즈 만큼만 데이터 전송
	//후에 버퍼 사이즈 만큼 보내기 위해 프로토콜 생성해야함
	auto curClientSock = clients[sockIndex]->GetClientSocket();
	if (curClientSock != INVALID_SOCKET) {
		unsigned int totSize = 0;
		unsigned int nowSize = 0;
		while (true) {
			nowSize = send(curClientSock, ((char*)data) + totSize, size - totSize, 0);
			
			if (nowSize < 0) {
				//클라이언트가 접속 종료됨
				closesocket(curClientSock);
				curClientSock = INVALID_SOCKET;
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