#include "IOCP.h"

using namespace std;

IOCPServer::~IOCPServer() {
	closesocket(listenSock);
	listenSock = INVALID_SOCKET;
	WSACleanup();
}

bool IOCPServer::Init() {
	//WSADATA �ʱ�ȭ
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		OutputDebugStringA("WSADATA �ʱ�ȭ ����\n");
		return false;
	}

	//���� ����
	if ((listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		OutputDebugStringA("Socket ���� ����\n");
		WSACleanup();
		return false;
	}
	//���ε�
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if ((::bind(listenSock, (sockaddr*)& serverAddr, sizeof(serverAddr))) == SOCKET_ERROR) {
		OutputDebugStringA("Binding ����\n");
		closesocket(listenSock);
		WSACleanup();
		return false;
	}
	
	//��⿭ ����
	if ((listen(listenSock, 5)) == SOCKET_ERROR) {
		OutputDebugStringA("��⿭ ���� ����\n");
		closesocket(listenSock);
		WSACleanup();
		return false;
	}

	//CP����
	mhIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (mhIOCP == NULL) {
		OutputDebugStringA("CP ���� ����\n");
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
			OutputDebugStringA("Client Accept ����\n");
			--i;
			continue;
			//return;
		}

		OutputDebugStringA("Client Connect....\n");

		//������ Ŭ���̾�Ʈ SocketInfo ����
		SocketInfo* tempSInfo = new SocketInfo();
		tempSInfo->socket = tempClientSock;
		tempSInfo->mCamera.SetPosition(0.0f, 2.0f, -30.0f);
		tempSInfo->overlappedRead = new OVERLAPPEDEX();
		tempSInfo->overlappedWrite = new OVERLAPPEDEX();
		memcpy(&(tempSInfo->clientAddr), &tempClientAddr, addrLen);

		//������ CP�� ���
		CreateIoCompletionPort((HANDLE)tempClientSock, mhIOCP, (ULONG_PTR)tempSInfo, 0);
		
		clients.push_back(tempSInfo);
	}
}



void IOCPServer::RequestSend(int sockIdx, HEADER header, void* data, bool overlapped) {

	auto curClient = clients[sockIdx];
	auto overlappedEx = curClient->overlappedWrite;

	overlappedEx->numberOfByte = headerSize;
	overlappedEx->flag = IOCP_FLAG_WRITE;

	//��� ����
	overlappedEx->wsaBuf[0].len = headerSize;
	overlappedEx->wsaBuf[0].buf = new char[headerSize];
	memcpy(overlappedEx->wsaBuf[0].buf, &header, headerSize);

	//������ ����
	/*
	if (data != nullptr) {
		DWORD dataSize = ntohl(header.mDataLen);
		curClient->wsaWriteBuf[1].len = dataSize;
		curClient->wsaWriteBuf[1].buf = new char[dataSize];
		memcpy(curClient->wsaWriteBuf[1].buf, data, dataSize);
	}
	*/
	if (overlapped) {
		if (WSASend(curClient->socket, &(overlappedEx->wsaBuf[0]), 1, &(overlappedEx->numberOfByte), overlappedEx->flag, &(overlappedEx->overlapped), NULL) == 0) {
			char str[256];
			sprintf(str, "%d ��ŭ ����\n", overlappedEx->numberOfByte);
			OutputDebugStringA(str);
		}
	}
	else {
		if (!SendHeader(curClient, 0))
			return;
		if (!SendData(curClient))
			return;
	}


	
	
	
}

void IOCPServer::RequestRecv(int sockIdx, bool overlapped) {
	auto curClient = clients[sockIdx];
	auto overlappedEx = curClient->overlappedRead;


	overlappedEx->numberOfByte = headerSize;
	overlappedEx->flag = IOCP_FLAG_READ;
	overlappedEx->wsaBuf[0].len = headerSize;
	overlappedEx->wsaBuf[0].buf = new char[headerSize];

	if (overlapped)
		WSARecv(curClient->socket, &(overlappedEx->wsaBuf[0]), 1, (LPDWORD) & (overlappedEx->numberOfByte), (LPDWORD) & (overlappedEx->flag), &(overlappedEx->overlapped), NULL);
	else {
		if (!RecvHeader(curClient, 0))
			return;
		if (!RecvData(curClient))
			return;
	}


	
}



void IOCPServer::RunNetwork(void* param) {
	while (true) {
		DWORD nowSize = 0;
		DWORD64 coKey = 0;
		SocketInfo* sInfo = nullptr;
		OVERLAPPEDEX* overlappedEx;

		if (GetQueuedCompletionStatus(param, &nowSize, (PULONG_PTR)& sInfo, (LPOVERLAPPED*)& overlappedEx, INFINITE) == FALSE) {
			OutputDebugStringA("Error - GetQueuedCompletionStatus Failure\n");
			closesocket(sInfo->socket);
			continue;
		}

		//�о�� ���� 0�� ��� Ŭ���̾�Ʈ ����
		if (nowSize == 0) {
			OutputDebugStringA("Error - Client Exit\n");
			closesocket(sInfo->socket);
			continue;
		}

		switch (overlappedEx->flag) {

		case IOCP_FLAG_READ:	//READ��û�̾��� ��
			if (!RecvHeader(sInfo, nowSize)) {
				continue;
			}
			if (!RecvData(sInfo)) {
				continue;
			}
			break;

		case IOCP_FLAG_WRITE:	//WRITE��û�̾��� ��
			if (!SendHeader(sInfo, nowSize)) {
				continue;
			}
			if (!SendData(sInfo)) {
				continue;
			}
			break;
		}
	}
	
}

bool IOCPServer::RecvHeader(SocketInfo* sInfo, DWORD nowSize) {
	DWORD totSize = 0;
	auto overlapped = sInfo->overlappedRead;

	//����� ũ�⸸ŭ �޾ƿ���
	while (true) {
		if (nowSize >= 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� ���� ����\n");
			break;
		}
		nowSize = recv(sInfo->socket, overlapped->wsaBuf[0].buf + totSize, headerSize - totSize, 0);
	}
	OutputDebugStringA("��� ���� ����\n");
	return true;
}

bool IOCPServer::SendHeader(SocketInfo* sInfo, DWORD nowSize) {
	DWORD totSize = 0;
	auto overlapped = sInfo->overlappedWrite;

	//����� ũ�⸸ŭ ���� ������
	while (true) {
		if (nowSize >= 0) {
			totSize += nowSize;
			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� �۽� ����\n");
			return false;
		}

		nowSize = send(sInfo->socket, overlapped->wsaBuf[0].buf + totSize, headerSize - totSize, 0);
	}

	OutputDebugStringA("��� �۽� ����\n");
	return true;
}

bool IOCPServer::RecvData(SocketInfo* sInfo) {
	auto overlapped = sInfo->overlappedRead;
	HEADER* header = (HEADER*)overlapped->wsaBuf[0].buf;

	//������� ������ ũ�� ������
	const DWORD size = ntohl(header->mDataLen);

	if (size < 0)
		return false;

	//�����Ͱ� ���� ��
	if (size > 0) {
		DWORD totSize = 0;	//���� ũ��
		DWORD nowSize = 0;	//recv�� �о�� ũ��

		//data�Ҵ� �� size����
		overlapped->wsaBuf[1].buf = new char[size];
		overlapped->wsaBuf[1].len = size;

		//������ ũ�⸸ŭ �о����
		while (true) {
			nowSize = recv(sInfo->socket, (char*)(overlapped->wsaBuf[1].buf) + totSize, size - totSize, 0);

			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "���� ���ŵ� ������ %d / %d\n", totSize, size);
				OutputDebugStringA(str);

				if (totSize >= size)
					break;
			}
			else {
				OutputDebugStringA("Data ���� ����\n");
				return false;
			}
		}

		OutputDebugStringA("Data ���� ����\n");
	}

	return true;
}

bool IOCPServer::SendData(SocketInfo* sInfo) {
	auto overlapped = sInfo->overlappedWrite;
	HEADER* header = (HEADER*)overlapped->wsaBuf[0].buf;
	const DWORD dataSize = ntohl(header->mDataLen);

	//������ ũ�⸸ŭ ����
	if (overlapped->wsaBuf[1].buf != nullptr && dataSize > 0) {
		DWORD totSize = 0;
		DWORD nowSize = 0;

		while (true) {
			nowSize = send(sInfo->socket, overlapped->wsaBuf[1].buf + totSize, dataSize - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				if (totSize >= dataSize)
					break;
			}
			else {
				OutputDebugStringA("Data �۽� ����\n");
				return false;
			}
		}
	}

	OutputDebugStringA("Data �۽� �Ϸ�\n");
	return true;
}