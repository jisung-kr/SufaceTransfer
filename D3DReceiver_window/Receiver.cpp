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


bool Client::ReadData() {
	//���ڿ� ����
	int size = 0;
	int totSize = 0;
	int nowSize = 0;
	char str[256];

	//���� ũ�� �޾ƿ���
	while (true) {
		nowSize = recv(serverSock, ((char*)&size) + totSize, sizeof(unsigned int), 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= sizeof(unsigned int))
				break;
		}
		else {
			OutputDebugStringA("������ ����\n");
			return false;
		}
	}

	//���ۿ� ������ �޾ƿ���
	size = ntohl(size);

	if (size <= 0)
		return false;

	totSize = 0;
	nowSize = 0;

	data = new char[size];
	ZeroMemory(data, size);

	while (true) {
		nowSize = recv(serverSock, ((char*)data) + totSize, size - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			wsprintfA(str, "���� ���ŵ� ������ %d / %d\n", totSize, size);
			OutputDebugStringA(str);

			if (totSize >= size)
				break;
		}
		else {
			OutputDebugStringA("������ ����\n");
			return false;
		}
	}

	OutputDebugStringA("���� �Ϸ�!\n");
	return true;
}

bool Client::RecvResponse() {
	//RES����
	memset(&resHeader, -1, sizeof(HEADER));
	const INT headerSize = sizeof(HEADER);
	INT totSize = 0;
	INT nowSize = 0;

	//��� �޾ƿ���
	while (true) {
		nowSize = recv(serverSock, ((char*)& resHeader) + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("RES��� ���� ����\n");
			return false;
		}
	}

	//���ۿ� ������ �޾ƿ���
	const INT size = ntohl(resHeader.mDataLen);

	if (size < 0)
		return false;

	if (size != 0) {
		totSize = 0;
		nowSize = 0;

		data = new char[size];
		ZeroMemory(data, size);

		while (true) {
			nowSize = recv(serverSock, ((char*)data) + totSize, size - totSize, 0);
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

	OutputDebugStringA("���� �Ϸ�!\n");
	return true;
}

bool Client::Request(HEADER reqHeader, void* data) {
	INT headerSize = sizeof(HEADER);
	INT totSize = 0;
	INT nowSize = 0;

	//REQ����
	while (true) {
		nowSize = send(serverSock, ((char*)& reqHeader) + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ��� �۽� ����\n");
			return false;
		}
	}

	//Data�� ���� �� ���� ����
	if (data != nullptr) {
		const INT dataSize = ntohl(reqHeader.mDataLen);
		totSize = 0;
		nowSize = 0;

		while (true) {
			nowSize = send(serverSock, ((char*)data) + totSize, dataSize - totSize, 0);
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

	return true;
}

char* Client::GetData() {
	return (char*)data;
}

void Client::ReleaseBuffer() {
	 if (data != nullptr) {
		delete data;
		data = nullptr;
	} 
}


