#include "IOCP.h"

using namespace std;

IOCPServer::~IOCPServer() {
	closesocket(listenSock);
	listenSock = INVALID_SOCKET;
	WSACleanup();
}

bool IOCPServer::Init(USHORT port) {
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
	serverAddr.sin_port = htons(port);
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
	for (int i = 0; i < maxClientCount; ++i) {
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
		tempSInfo->mCamera.SetPosition(30.0f, 0.0f, -160.0f);
		memcpy(&(tempSInfo->clientAddr), &tempClientAddr, addrLen);

		//������ Ŭ���̾�Ʈ�� ����̽� ������ �޾ƿ�
		recv(tempSInfo->socket, (char*)&tempSInfo->mDeviceInfo, sizeof(DeviceInfo), 0);

		//������ CP�� ���
		CreateIoCompletionPort((HANDLE)tempClientSock, mhIOCP, (ULONG_PTR)tempSInfo, 0);

		//Ŭ���̾�Ʈ �ڷᱸ���� ����
		clients.push_back(tempSInfo);
	}
}


void IOCPServer::RunNetwork(void* param) {
	while (true) {
		DWORD nowSize = 0;
		SocketInfo* sInfo = nullptr;
		OVERLAPPEDEX* overlappedEx;
		HEADER* header = nullptr;

		if (GetQueuedCompletionStatus(param, &nowSize, (PULONG_PTR)& sInfo, (LPOVERLAPPED*)& overlappedEx, INFINITE) == FALSE) {
			OutputDebugStringA("Error - GetQueuedCompletionStatus Failure\n");
			//closesocket(sInfo->socket);
			continue;
		}

		//�о�� ���� 0�� ��� Ŭ���̾�Ʈ ����
		/*
		if (nowSize == 0) {
			OutputDebugStringA("Error - Client Exit\n");
			closesocket(sInfo->socket);
			continue;
		}
		*/
		switch (overlappedEx->mFlag) {

		case IOCP_FLAG_READ:	//READ��û�̾��� ��
			if (!RecvHeader(sInfo, *overlappedEx, nowSize)) {
				continue;
			}
			if (!RecvData(sInfo, *overlappedEx)) {
				continue;
			}

			header = (HEADER*)overlappedEx->mPacket->mHeader.buf;

			//������ Packet�� Input�̶��
			if (ntohl(header->mCommand) == COMMAND::COMMAND_INPUT) {
				sInfo->inputRQueue.PushItem(std::move(overlappedEx->mPacket));
			}
			else {
				sInfo->rQueue.PushItem(std::move(overlappedEx->mPacket));
			}
			
			sInfo->isUsingRecv = false;
			OutputDebugStringA("Queue�� Packet ����\n");
			break;

		case IOCP_FLAG_WRITE:	//WRITE��û�̾��� ��
			if (!SendHeader(sInfo, *overlappedEx, nowSize)) {
				continue;
			}
			if (!SendData(sInfo, *overlappedEx)) {
				continue;
			}

			sInfo->isUsingSend = false;
			OutputDebugStringA("Queue���� Packet ����\n");
			break;
		}
		delete overlappedEx;
	}
	
}

void IOCPServer::RequestRecv(int sockIdx, bool overlapped) {
	auto curClient = clients[sockIdx];
	if (curClient->isUsingRecv == false) {
		curClient->isUsingRecv = true;
		//���ſ� �������� ����
		OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX();
		overlappedEx->mFlag = IOCP_FLAG_READ;
		overlappedEx->mNumberOfByte = 0;
		overlappedEx->mPacket = make_unique<Packet>(0);

		if (overlapped)
			WSARecv(curClient->socket, &(overlappedEx->mPacket->mHeader), 1, (LPDWORD) & (overlappedEx->mNumberOfByte), (LPDWORD) & (overlappedEx->mFlag), &(overlappedEx->mOverlapped), NULL);
		else {
			//���� ó��
			if (!RecvHeader(curClient, *overlappedEx, 0)) {
				OutputDebugStringA("Error: RequestRecv - Recv Header\n");
				return;
			}

			if (!RecvData(curClient, *overlappedEx)) {
				OutputDebugStringA("Error: RequestRecv -Recv Data\n");
				return;
			}
			curClient->rQueue.PushItem(std::move(overlappedEx->mPacket));
			OutputDebugStringA("Queue�� Packet ����\n");
		}
	}
	

	
}


bool IOCPServer::RecvHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize) {
	DWORD64 totSize = 0;

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
		nowSize = recv(sInfo->socket, overlappedEx.mPacket->mHeader.buf + totSize, headerSize - totSize, 0);
	}
	OutputDebugStringA("��� ���� ����\n");
	return true;
}

bool IOCPServer::RecvData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx) {

	HEADER* header = (HEADER*)overlappedEx.mPacket->mHeader.buf;

	//������� ������ ũ�� ������
	const DWORD64 size = ntohl(header->mDataLen);

	if (size < 0)
		return false;

	//�����Ͱ� ���� ��
	if (size > 0) {
		DWORD64 totSize = 0;	//���� ũ��
		DWORD64 nowSize = 0;	//recv�� �о�� ũ��

		//data�Ҵ� �� size����
		overlappedEx.mPacket->AllocDataBuffer(size);

		//������ ũ�⸸ŭ �о����
		while (true) {
			nowSize = recv(sInfo->socket, (char*)(overlappedEx.mPacket->mData.buf) + totSize, size - totSize, 0);

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


void IOCPServer::RequestSend(int sockIdx, bool overlapped) {
	auto curClient = clients[sockIdx];

	if (curClient->wQueue.Size() > 0 && curClient->isUsingSend == false) {
		curClient->isUsingSend = true;
		OVERLAPPEDEX* overlappedEx = new OVERLAPPEDEX();
		overlappedEx->mFlag = IOCP_FLAG_WRITE;
		overlappedEx->mNumberOfByte = 0;
		overlappedEx->mPacket = std::move(curClient->wQueue.FrontItem());
		curClient->wQueue.PopItem();

		if (overlapped) {
			if (WSASend(curClient->socket, &(overlappedEx->mPacket->mHeader), 1, &(overlappedEx->mNumberOfByte), overlappedEx->mFlag, &(overlappedEx->mOverlapped), NULL) == 0) {

			}
		}
		else {	//not overlapped
			if (!SendHeader(curClient, *overlappedEx, 0))
				return;
			if (!SendData(curClient, *overlappedEx))
				return;
		}
	}
}

bool IOCPServer::SendHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize) {
	DWORD64 totSize = 0;

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

		nowSize = send(sInfo->socket, overlappedEx.mPacket->mHeader.buf + totSize, headerSize - totSize, 0);
	}

	OutputDebugStringA("��� �۽� ����\n");
	return true;
}

bool IOCPServer::SendData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx) {

	HEADER* header = (HEADER*)overlappedEx.mPacket->mHeader.buf;
	WSABUF& data = overlappedEx.mPacket->mData;

	const DWORD64 dataSize = ntohl(header->mDataLen);

	//������ ũ�⸸ŭ ����
	if (data.buf != nullptr && dataSize > 0) {
		DWORD64 totSize = 0;
		DWORD64 nowSize = 0;

		while (true) {
			nowSize = send(sInfo->socket, data.buf + totSize, dataSize - totSize, 0);
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