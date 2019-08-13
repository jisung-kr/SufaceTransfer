#include "Network.h"


Server::~Server() {
	if (serverSock != INVALID_SOCKET)
		closesocket(serverSock);
	if(clientSock != INVALID_SOCKET)
		closesocket(clientSock);

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


//�ϴ� �̱۽������
void Server::WaitForClient() {
	int addrSize = sizeof(clientAddr);
	clientSock = accept(serverSock, (sockaddr*)& clientAddr, &addrSize);

	char str[256];
	InetNtopA(AF_INET, &clientAddr.sin_addr, str, sizeof(str));
	/*
	while (true) {
		int addrSize = sizeof(clientAddr);
		clientSock = accept(serverSock, (sockaddr*)& clientAddr, &addrSize);

		char str[256];
		InetNtopA(AF_INET, &clientAddr.sin_addr, str, sizeof(str));
	}
	*/
}

bool Server::IsInvalidClientSocket() {
	return clientSock == INVALID_SOCKET;
}


bool Server::SendData(void* data, int size) {

	//������ �۽�
	//���� ���� ������ ��ŭ�� ������ ����
	//�Ŀ� ���� ������ ��ŭ ������ ���� �������� �����ؾ���
	if (clientSock != INVALID_SOCKET) {
		unsigned int totSize = 0;
		unsigned int nowSize = 0;
		while (true) {
			nowSize = send(clientSock, ((char*)data) + totSize, size - totSize, 0);
			
			if (nowSize < 0) {
				//Ŭ���̾�Ʈ�� ���� �����
				closesocket(clientSock);
				clientSock = INVALID_SOCKET;
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