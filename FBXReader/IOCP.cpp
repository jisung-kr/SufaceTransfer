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
		SocketInfo* tempSInfo = new SocketInfo();
		tempSInfo->socket = tempClientSock;
		tempSInfo->mCamera.SetPosition(0.0f, 2.0f, -30.0f);
		memcpy(&(tempSInfo->clientAddr), &tempClientAddr, addrLen);

		//소켓을 CP에 등록
		CreateIoCompletionPort((HANDLE)tempClientSock, mhIOCP, (ULONG_PTR)tempSInfo, 0);
		
		clients.push_back(tempSInfo);
	}
}


void IOCPServer::RunNetwork(void* param) {
	while (true) {
		DWORD nowSize = 0;
		SocketInfo* sInfo = nullptr;
		OVERLAPPEDEX* overlappedEx;

		if (GetQueuedCompletionStatus(param, &nowSize, (PULONG_PTR)& sInfo, (LPOVERLAPPED*)& overlappedEx, INFINITE) == FALSE) {
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

		switch (overlappedEx->mFlag) {

		case IOCP_FLAG_READ:	//READ요청이었을 시
			if (!RecvHeader(sInfo, *overlappedEx, nowSize)) {
				continue;
			}
			if (!RecvData(sInfo, *overlappedEx)) {
				continue;
			}
			
			rQueue.PushItem(overlappedEx->mPacket);
			OutputDebugStringA("Queue에 Packet 저장\n");
			break;

		case IOCP_FLAG_WRITE:	//WRITE요청이었을 시
			if (!SendHeader(sInfo, *overlappedEx, nowSize)) {
				continue;
			}
			if (!SendData(sInfo, *overlappedEx)) {
				continue;
			}

			wQueue.PopItem();
			OutputDebugStringA("Queue에서 Packet 삭제\n");

			break;
		}
	}
	
}

void IOCPServer::RequestRecv(int sockIdx, bool overlapped) {
	auto curClient = clients[sockIdx];

	//수신용 오버랩드 생성
	//OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX(new Packet(headerSize), IOCP_FLAG_READ);
	OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX();
	overlappedEx->mFlag = IOCP_FLAG_READ;
	overlappedEx->mNumberOfByte = headerSize;
	overlappedEx->mPacket = new Packet(headerSize);

	if (overlapped)
		WSARecv(curClient->socket, &(overlappedEx->mPacket->mHeader), 1, (LPDWORD) & (overlappedEx->mNumberOfByte), (LPDWORD) & (overlappedEx->mFlag), &(overlappedEx->mOverlapped), NULL);
	else {
		//동기 처리
		if (!RecvHeader(curClient, *overlappedEx, 0)) {
			OutputDebugStringA("Error: RequestRecv - Recv Header\n");
			return;
		}

		if (!RecvData(curClient, *overlappedEx)) {
			OutputDebugStringA("Error: RequestRecv -Recv Data\n");
			return;
		}

		rQueue.PushItem(overlappedEx->mPacket);
		OutputDebugStringA("Queue에 Packet 저장\n");
	}

}


bool IOCPServer::RecvHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize) {
	DWORD totSize = 0;

	//헤더의 크기만큼 받아오기
	while (true) {
		if (nowSize >= 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("헤더 수신 실패\n");
			break;
		}
		nowSize = recv(sInfo->socket, overlappedEx.mPacket->mHeader.buf + totSize, headerSize - totSize, 0);
	}
	OutputDebugStringA("헤더 수신 성공\n");
	return true;
}

bool IOCPServer::RecvData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx) {

	HEADER* header = (HEADER*)overlappedEx.mPacket->mHeader.buf;

	//헤더에서 데이터 크기 가져옴
	const DWORD size = ntohl(header->mDataLen);

	if (size < 0)
		return false;

	//데이터가 있을 시
	if (size > 0) {
		DWORD totSize = 0;	//누적 크기
		DWORD nowSize = 0;	//recv로 읽어온 크기

		//data할당 및 size저장
		overlappedEx.mPacket->AllocDataBuffer(size);

		//데이터 크기만큼 읽어오기
		while (true) {
			nowSize = recv(sInfo->socket, (char*)(overlappedEx.mPacket->mData.buf) + totSize, size - totSize, 0);

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

		OutputDebugStringA("Data 수신 성공\n");
	}

	return true;
}


void IOCPServer::RequestSend(int sockIdx, bool overlapped) {
	if (wQueue.Size() > 0) {
		auto curClient = clients[sockIdx];

		Packet* packet = wQueue.FrontItem();

		//패킷 전송용 오버랩드 생성
		//OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX(packet, IOCP_FLAG_WRITE);
		
		OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX();
		overlappedEx->mFlag = IOCP_FLAG_WRITE;
		overlappedEx->mNumberOfByte = headerSize;
		overlappedEx->mPacket = packet;

		if (overlapped) {
			if (WSASend(curClient->socket, &(overlappedEx->mPacket->mHeader), 1, &(overlappedEx->mNumberOfByte), overlappedEx->mFlag, &(overlappedEx->mOverlapped), NULL) == 0) {
				char str[256];
				sprintf(str, "%d 만큼 보냄\n", overlappedEx->mNumberOfByte);
				OutputDebugStringA(str);
			}
		}
		else {
			if (!SendHeader(curClient, *overlappedEx, 0))
				return;
			if (!SendData(curClient, *overlappedEx))
				return;

			wQueue.PopItem();
			OutputDebugStringA("Queue에서 Packet 삭제\n");
		}
	}


}

bool IOCPServer::SendHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize) {
	DWORD totSize = 0;

	//헤더의 크기만큼 전부 보내기
	while (true) {
		if (nowSize >= 0) {
			totSize += nowSize;
			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("헤더 송신 실패\n");
			return false;
		}

		nowSize = send(sInfo->socket, overlappedEx.mPacket->mHeader.buf + totSize, headerSize - totSize, 0);
	}

	OutputDebugStringA("헤더 송신 성공\n");
	return true;
}

bool IOCPServer::SendData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx) {

	HEADER* header = (HEADER*)overlappedEx.mPacket->mHeader.buf;
	WSABUF& data = overlappedEx.mPacket->mData;

	const DWORD dataSize = ntohl(header->mDataLen);

	//데이터 크기만큼 쓰기
	if (data.buf != nullptr && dataSize > 0) {
		DWORD totSize = 0;
		DWORD nowSize = 0;

		while (true) {
			nowSize = send(sInfo->socket, data.buf + totSize, dataSize - totSize, 0);
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