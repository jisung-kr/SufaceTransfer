#include "IOCP.h"

using namespace std;

IOCPServer::~IOCPServer() {
	closesocket(listenSock);
	listenSock = INVALID_SOCKET;
	WSACleanup();
}

bool IOCPServer::Init() {
	//WSADATA 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		OutputDebugStringA("WSADATA 초기화 오류\n");
		return false;
	}

	//소켓 생성
	if ((listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		OutputDebugStringA("Socket 생성 오류\n");
		WSACleanup();
		return false;
	}
	//바인딩
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if ((::bind(listenSock, (sockaddr*)& serverAddr, sizeof(serverAddr))) == SOCKET_ERROR) {
		OutputDebugStringA("Binding 오류\n");
		closesocket(listenSock);
		WSACleanup();
		return false;
	}
	
	//대기열 생성
	if ((listen(listenSock, 5)) == SOCKET_ERROR) {
		OutputDebugStringA("대기열 생성 오류\n");
		closesocket(listenSock);
		WSACleanup();
		return false;
	}

	//CP생성
	mhIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (mhIOCP == NULL) {
		OutputDebugStringA("CP 생성 오류\n");
		closesocket(listenSock);
		WSACleanup();
		return false;
	}

	SetThread(&IOCP::RunNetwork);
	
	return true;
}

void IOCPServer::AcceptClient() {
	for (int i = 0; i < MAXCLIENT; ++i) {
		SOCKET tempClientSock;
		sockaddr_in tempClientAddr;
		INT addrLen = sizeof(tempClientAddr);

		tempClientSock = accept(listenSock, (sockaddr*)& tempClientAddr, &addrLen);
		if (tempClientSock == INVALID_SOCKET) {
			OutputDebugStringA("Client Accept 오류\n");
			--i;
			continue;
			//return;
		}

		OutputDebugStringA("Client Connect....\n");

		//접속한 클라이언트 SocketInfo 생성
		shared_ptr<SocketInfo> tempSInfo = make_shared<SocketInfo>();
		tempSInfo->socket = tempClientSock;
		memcpy(&(tempSInfo->clientAddr), &tempClientAddr, addrLen);

		//소켓을 CP에 등록
		CreateIoCompletionPort((HANDLE)tempClientSock, mhIOCP, (ULONG_PTR)&tempClientSock, 0);
		
		clients.push_back(tempSInfo);
	}
}



void IOCPServer::RequestSend(int sockIdx, HEADER header, void* data, bool overlapped) {
	auto curClient = clients[sockIdx];

	if (curClient->mIsWriteBufUsing == false) {
		curClient->mIsWriteBufUsing = true;

		curClient->writen = sizeof(HEADER);
		curClient->flag = IOCP_FLAG_WRITE;

		//헤더 설정
		curClient->wsaWriteBuf[0].len = sizeof(HEADER);
		curClient->wsaWriteBuf[0].buf = new char[sizeof(HEADER)];
		memcpy(curClient->wsaWriteBuf[0].buf, &header, sizeof(HEADER));

		//데이터 설정
		if (data != nullptr) {
			curClient->wsaWriteBuf[1].len = header.mDataLen;
			curClient->wsaWriteBuf[1].buf = new char[header.mDataLen];
			memcpy(curClient->wsaWriteBuf[1].buf, data, header.mDataLen);
		}

		if (overlapped)
			WSASend(curClient->socket, &(curClient->wsaWriteBuf[0]), 1, &(curClient->writen), curClient->flag, &(curClient->overlapped), NULL);
		else
			WSASend(curClient->socket, &(curClient->wsaWriteBuf[0]), 1, &(curClient->writen), curClient->flag, NULL, NULL);

	}
	
	
}

void IOCPServer::RequestRecv(int sockIdx, bool overlapped) {
	auto curClient = clients[sockIdx];

	if (curClient->mIsReadBufUsing == false) {
		curClient->mIsReadBufUsing = true;

		curClient->readn = sizeof(HEADER);
		curClient->flag = IOCP_FLAG_READ;
		curClient->wsaReadBuf[0].len = sizeof(HEADER);
		curClient->wsaReadBuf[0].buf = new char[sizeof(HEADER)];

		if (overlapped)
			WSARecv(curClient->socket, &(curClient->wsaReadBuf[0]), 1, (LPDWORD) & (curClient->readn), (LPDWORD) & (curClient->flag), &(curClient->overlapped), NULL);
		else
			WSARecv(curClient->socket, &(curClient->wsaReadBuf[0]), 1, (LPDWORD) & (curClient->readn), (LPDWORD) & (curClient->flag), NULL, NULL);
	}
}



void IOCPServer::RunNetwork(void* param) {
	while (true) {
		DWORD nowSize = 0;
		DWORD64 coKey = 0;
		SocketInfo* sInfo = nullptr;

		if (GetQueuedCompletionStatus(param, &nowSize, (PULONG_PTR)& coKey, (LPOVERLAPPED*)& sInfo, INFINITE) == 0) {
			OutputDebugStringA("Error - GetQueuedCompletionStatus Failure\n");
			closesocket(sInfo->socket);
			continue;
		}

		//읽어온 값이 0인 경우 클라이언트 종료
		if (nowSize == 0) {
			OutputDebugStringA("Error - Client Exit\n");
			closesocket(sInfo->socket);
			continue;
		}

		switch (sInfo->flag) {

		case IOCP_FLAG_READ:	//READ요청이었을 시
			if (!RecvHeader(sInfo, nowSize)) {
				continue;
			}
			if (!RecvData(sInfo)) {
				continue;
			}
			sInfo->mIsReadBufUsing = false;
			break;

		case IOCP_FLAG_WRITE:	//WRITE요청이었을 시
			if (!SendHeader(sInfo, nowSize)) {
				continue;
			}
			if (!SendData(sInfo)) {
				continue;
			}
			sInfo->mIsWriteBufUsing = false;
			break;
		}
	}
	
}

bool IOCPServer::RecvHeader(SocketInfo* sInfo, DWORD nowSize) {
	DWORD headerSize = sizeof(HEADER);
	DWORD totSize = 0;

	//헤더의 크기만큼 받아오기
	while (true) {
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("헤더 수신 실패\n");
			break;
		}
		nowSize = send(sInfo->socket, ((char*)& sInfo->wsaWriteBuf[0].buf) + totSize, headerSize - totSize, 0);
	}
	OutputDebugStringA("헤더 수신 성공\n");
	return true;
}

bool IOCPServer::SendHeader(SocketInfo* sInfo, DWORD nowSize) {
	DWORD headerSize = sizeof(HEADER);
	DWORD totSize = 0;

	//헤더의 크기만큼 전부 보내기
	while (true) {
		if (nowSize > 0) {
			totSize += nowSize;
			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("헤더 송신 실패\n");
			return false;
		}

		nowSize = recv(sInfo->socket, ((char*)& sInfo->wsaReadBuf[0].buf) + totSize, headerSize - totSize, 0);
	}

	OutputDebugStringA("헤더 송신 성공\n");
	return true;
}

bool IOCPServer::RecvData(SocketInfo* sInfo) {
	HEADER* header = (HEADER*)sInfo->wsaReadBuf[0].buf;

	//헤더에서 데이터 크기 가져옴
	const DWORD size = ntohl(header->mDataLen);

	if (size < 0)
		return false;

	//데이터가 있을 시
	if (size > 0) {
		DWORD totSize = 0;	//누적 크기
		DWORD nowSize = 0;	//recv로 읽어온 크기

		//data할당 및 size저장
		sInfo->wsaReadBuf[1].buf = new char[size];
		sInfo->wsaReadBuf[1].len = size;

		//데이터 크기만큼 읽어오기
		while (true) {
			nowSize = recv(sInfo->socket, (char*) & (sInfo->wsaReadBuf[1].buf) + totSize, size - totSize, 0);

			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "현재 수신된 데이터 %d / %d\n", totSize, size);
				OutputDebugStringA(str);

				if (totSize >= size)
					break;
			}
			else {
				OutputDebugStringA("Data 수신 실패\n");
				return false;
			}
		}
	}

	OutputDebugStringA("Data 수신 성공\n");
	return true;
}

bool IOCPServer::SendData(SocketInfo* sInfo) {
	HEADER* header = (HEADER*)sInfo->wsaWriteBuf[0].buf;

	//데이터 크기만큼 쓰기
	if (sInfo->wsaWriteBuf[1].buf != nullptr && header->mDataLen > 0) {
		HEADER* header = (HEADER*)sInfo->wsaWriteBuf[0].buf;

		const DWORD dataSize = ntohl(header->mDataLen);
		DWORD totSize = 0;
		DWORD nowSize = 0;

		while (true) {
			nowSize = send(sInfo->socket, ((char*)sInfo->wsaWriteBuf[1].buf) + totSize, dataSize - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				if (totSize >= dataSize)
					break;
			}
			else {
				OutputDebugStringA("Data 송신 실패\n");
				return false;
			}
		}
	}

	OutputDebugStringA("Data 송신 완료\n");
	return true;
}