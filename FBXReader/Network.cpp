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

bool Server::RecvRequest(int sockIndex) {
	auto curClient = clients[sockIndex];
	auto curClientSock = clients[sockIndex]->GetClientSocket();

	//헤더 수신
	UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	while (true) {
		nowSize = recv(curClientSock, ((char*)& curClient->reqHeader) + totSize, headerSize - totSize, 0);

		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ헤더 수신 실패!\n");
		}
	}

	//버퍼에 데이터 받아오기
	auto& header = curClient->reqHeader;
	UINT size = (UINT)ntohl(header.mDataLen);

	if (size < 0)
		return false;

	OutputDebugStringA("헤더 수신 성공\n");

	if (size != 0) {
		//네트워크 사이즈 변수 리셋
		totSize = 0;
		nowSize = 0;

		//해당 클라이언트 클래스에 data할당 및 size저장
		curClient->AllocDataMem(size);

		while (true) {
			nowSize = recv(serverSock, ((char*)curClient->GetDataMem()) + totSize, size - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "현재 수신된 데이터 %d / %d\n", totSize, size);
				OutputDebugStringA(str);

				if (totSize >= size)
					break;
			}
			else {
				OutputDebugStringA("데이터 수신 실패\n");
				return false;
			}
		}
	}

	OutputDebugStringA("수신 완료!\n");
	return true;
}

bool Server::Response(int sockIndex) {

	auto curClient = clients[sockIndex];
	auto curClientSock = clients[sockIndex]->GetClientSocket();

	//헤더 수신
	UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	while (true) {
		nowSize = recv(curClientSock, ((char*)& curClient->reqHeader) + totSize, headerSize - totSize, 0);

		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ헤더 수신 실패!\n");
		}
	}
	
	

	switch (curClient->reqHeader.mCommand) {

	case COMMAND::COMMAND_REQ_FRAME:
		//여러가지 데이터를 조작한 후


		//송신
		if (!SendMSG(sockIndex,CHEADER::CHEADER(COMMAND::COMMAND_RES_FRAME, curClient->dataSize), curClient->data)) {
			OutputDebugStringA("데이터 송신 실패!\n");
			return false;
		}

		break;
	}


	return true;
}


bool Server::SendMSG(int sockIndex, HEADER resHeader, void* data) {
	auto curClientSock = clients[sockIndex]->GetClientSocket();


	//가져온 헤더 내용을 송신
	if (curClientSock != INVALID_SOCKET) {
		UINT headerSize = sizeof(HEADER);
		UINT totSize = 0;
		UINT nowSize = 0;
		while (true) {
			nowSize = send(curClientSock, ((char*)&resHeader) + totSize, headerSize - totSize, 0);

			if (nowSize < 0) {
				//클라이언트가 접속 종료됨
				closesocket(curClientSock);
				curClientSock = INVALID_SOCKET;
				OutputDebugStringA("클라이언트 소켓 종료\n");
				return false;
			}
			else {
				totSize += nowSize;
			}

			if (totSize >= headerSize)
				break;
		}
	}



	//데이터도 있을 시 같이 전송
	if (data != nullptr) {
		//데이터 송신
		if (curClientSock != INVALID_SOCKET) {
			UINT dataSize = ntohl(resHeader.mDataLen);
			UINT totSize = 0;
			UINT nowSize = 0;

			while (true) {
				nowSize = send(curClientSock, ((char*)data) + totSize, dataSize - totSize, 0);

				if (nowSize < 0) {
					//클라이언트가 접속 종료됨
					closesocket(curClientSock);
					curClientSock = INVALID_SOCKET;
					OutputDebugStringA("클라이언트 소켓 종료\n");
					return false;
				}
				else {
					totSize += nowSize;
				}

				if (totSize >= dataSize)
					break;
			}
		}
	}


	
	return true;
}
