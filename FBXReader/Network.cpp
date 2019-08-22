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
	//���� �ʱ�ȭ
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
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
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//���� ���ε�
	if ((bind(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr))) == SOCKET_ERROR) {
		return false;
	}

	//��⿭ ����
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

	//��� �޾ƿ���
	while (true) {
		nowSize = recv(serverSock, ((char*)&header) + totSize, sizeof(HEADER), 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= sizeof(HEADER))
				break;
		}
		else {
			OutputDebugStringA("��� ���� ����\n");
			return false;
		}
	}

	//���ۿ� ������ �޾ƿ���
	unsigned int size = (unsigned int)ntohl(header.dataLen);

	if (size < 0)
		return false;

	OutputDebugStringA("��� ���� ����\n");

	//��Ʈ��ũ ������ ���� ����
	totSize = 0;
	nowSize = 0;

	//�ش� Ŭ���̾�Ʈ Ŭ������ data�Ҵ� �� size����
	clients[sockIndex]->AllocDataMem(size);
	clients[sockIndex]->SetDataSize(size);

	while (true) {
		nowSize = recv(serverSock, ((char*)clients[sockIndex]->GetDataMem()) + totSize, size - totSize, 0);
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


bool Server::SendMSG(int sockIndex, void* data, int size) {
	/*	*/
	//������ �۽�
	//���� ���� ������ ��ŭ�� ������ ����
	//�Ŀ� ���� ������ ��ŭ ������ ���� �������� �����ؾ���
	auto curClientSock = clients[sockIndex]->GetClientSocket();
	if (curClientSock != INVALID_SOCKET) {
		unsigned int totSize = 0;
		unsigned int nowSize = 0;
		while (true) {
			nowSize = send(curClientSock, ((char*)data) + totSize, size - totSize, 0);
			
			if (nowSize < 0) {
				//Ŭ���̾�Ʈ�� ���� �����
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