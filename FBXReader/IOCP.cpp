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
		shared_ptr<SocketInfo> tempSInfo = make_shared<SocketInfo>();
		tempSInfo->socket = tempClientSock;
		memcpy(&(tempSInfo->clientAddr), &tempClientAddr, addrLen);

		//������ CP�� ���
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

		//��� ����
		curClient->wsaWriteBuf[0].len = sizeof(HEADER);
		curClient->wsaWriteBuf[0].buf = new char[sizeof(HEADER)];
		memcpy(curClient->wsaWriteBuf[0].buf, &header, sizeof(HEADER));

		//������ ����
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

		//�о�� ���� 0�� ��� Ŭ���̾�Ʈ ����
		if (nowSize == 0) {
			OutputDebugStringA("Error - Client Exit\n");
			closesocket(sInfo->socket);
			continue;
		}

		switch (sInfo->flag) {

		case IOCP_FLAG_READ:	//READ��û�̾��� ��
			if (!RecvHeader(sInfo, nowSize)) {
				continue;
			}
			if (!RecvData(sInfo)) {
				continue;
			}
			sInfo->mIsReadBufUsing = false;
			break;

		case IOCP_FLAG_WRITE:	//WRITE��û�̾��� ��
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

	//����� ũ�⸸ŭ �޾ƿ���
	while (true) {
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� ���� ����\n");
			break;
		}
		nowSize = send(sInfo->socket, ((char*)& sInfo->wsaWriteBuf[0].buf) + totSize, headerSize - totSize, 0);
	}
	OutputDebugStringA("��� ���� ����\n");
	return true;
}

bool IOCPServer::SendHeader(SocketInfo* sInfo, DWORD nowSize) {
	DWORD headerSize = sizeof(HEADER);
	DWORD totSize = 0;

	//����� ũ�⸸ŭ ���� ������
	while (true) {
		if (nowSize > 0) {
			totSize += nowSize;
			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� �۽� ����\n");
			return false;
		}

		nowSize = recv(sInfo->socket, ((char*)& sInfo->wsaReadBuf[0].buf) + totSize, headerSize - totSize, 0);
	}

	OutputDebugStringA("��� �۽� ����\n");
	return true;
}

bool IOCPServer::RecvData(SocketInfo* sInfo) {
	HEADER* header = (HEADER*)sInfo->wsaReadBuf[0].buf;

	//������� ������ ũ�� ������
	const DWORD size = ntohl(header->mDataLen);

	if (size < 0)
		return false;

	//�����Ͱ� ���� ��
	if (size > 0) {
		DWORD totSize = 0;	//���� ũ��
		DWORD nowSize = 0;	//recv�� �о�� ũ��

		//data�Ҵ� �� size����
		sInfo->wsaReadBuf[1].buf = new char[size];
		sInfo->wsaReadBuf[1].len = size;

		//������ ũ�⸸ŭ �о����
		while (true) {
			nowSize = recv(sInfo->socket, (char*) & (sInfo->wsaReadBuf[1].buf) + totSize, size - totSize, 0);

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
	}

	OutputDebugStringA("Data ���� ����\n");
	return true;
}

bool IOCPServer::SendData(SocketInfo* sInfo) {
	HEADER* header = (HEADER*)sInfo->wsaWriteBuf[0].buf;

	//������ ũ�⸸ŭ ����
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
				OutputDebugStringA("Data �۽� ����\n");
				return false;
			}
		}
	}

	OutputDebugStringA("Data �۽� �Ϸ�\n");
	return true;
}