#include "Receiver.h"


Client::~Client() {
	
	closesocket(serverSock);

	WSACleanup();
}

bool Client::Init() {
	//���� �ʱ�ȭ
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		return false;
	}

	//���� ����
	if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		return false;
	}

	//���� ����
	memset(&serverAddr, 0x00, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	ULONG ulongAddr;
	InetPtonA(AF_INET, SERVER_IP, &ulongAddr);
	serverAddr.sin_addr.S_un.S_addr = ulongAddr;

	return true;
}


bool Client::Connection() {
	//������ Ŀ��Ʈ
	if (connect(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		return false;
	}

	return true;
}




bool Client::RecvHeader() {
	DWORD totSize = 0;
	DWORD nowSize = 0;

	//��� ����
	while (true) {
		DWORD flag = 0;
		//WSARecv(serverSock, &wsaReadBuf[0], 1, &headerSize, &flag, NULL, NULL);
		nowSize = recv(serverSock, (char*)wsaReadBuf[0].buf + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� ���� ����\n");
			return false;
		}
	}
	HEADER* header = (HEADER*)wsaReadBuf[0].buf;
	if (ntohl(header->mCommand) >= COMMAND::COMMAND_MAX) {
		OutputDebugStringA("��� ���� ����\n");
		return false;
	}
	OutputDebugStringA("��� ���� ����\n");
	return true;
}

bool Client::SendHeader() {
	DWORD totSize = 0;
	DWORD nowSize = 0;

	//��� �۽�
	while (true) {
		nowSize = send(serverSock, (char*)wsaWriteBuf[0].buf + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("��� �۽� ����\n");
			return false;
		}
	}

	OutputDebugStringA("��� �۽� ����\n");
	return true;
}

bool Client::RecvData() {
	HEADER* header = (HEADER*)wsaReadBuf[0].buf;
	DWORD size = ntohl(header->mDataLen);
	DWORD totSize = 0;
	DWORD nowSize = 0;

	if (size > 0) {
		wsaReadBuf[1].buf = new char[size];
		wsaReadBuf[1].len = size;

		while (true) {
			nowSize = recv(serverSock, (char*)wsaReadBuf[1].buf + totSize, size - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "���� ���ŵ� ������ %d / %d\n", totSize, size);
				OutputDebugStringA(str);

				if (totSize >= size)
					break;
			}
			else {
				OutputDebugStringA("������ ���� ����\n");
				return false;
			}
		}
	}

	OutputDebugStringA("������ ���� �Ϸ�\n");
	return true;
}

bool Client::SendData() {
	HEADER* header = (HEADER*)wsaWriteBuf[0].buf;
	const DWORD dataSize = ntohl(header->mDataLen);
	DWORD totSize = 0;
	DWORD nowSize = 0;

	if (wsaWriteBuf[1].buf != nullptr && dataSize > 0) {
		while (true) {
			nowSize = send(serverSock, (char*)wsaWriteBuf[1].buf + totSize, dataSize - totSize, 0);
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
		OutputDebugStringA("Data �۽� ����\n");
	}

	return true;
}



bool Client::RecvMSG() {
	wsaReadBuf[0].buf = new char[headerSize];
	wsaReadBuf[0].len = headerSize;
	if (!RecvHeader()) {
		return false;
	}
	if (!RecvData()) {
		return false;
	}

	return true;
}
bool Client::SendMSG(HEADER header, void* data) {

	wsaWriteBuf[0].buf = new char[headerSize];
	wsaWriteBuf[0].len = headerSize;
	memcpy(wsaWriteBuf[0].buf, &header, headerSize);

	if (data != nullptr) {
		DWORD dataSize = ntohl(header.mDataLen);
		wsaWriteBuf[1].buf = new char[dataSize];
		wsaWriteBuf[1].len = dataSize;
		memcpy(wsaWriteBuf[1].buf, data, dataSize);
	}

	if (!SendHeader())
		return false;
	if (!SendData()) {
		return false;
	}

	return true;
}



char* Client::GetData() {
	return (char*)wsaReadBuf[1].buf;
}

void Client::ReleaseBuffer() {
	 if (wsaReadBuf[1].buf != nullptr) {
		delete wsaReadBuf[1].buf;
		wsaReadBuf[1].buf = nullptr;
	} 
}


