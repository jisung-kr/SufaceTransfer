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

void Server::ReceiveMSG(char* data, int dataLen) {
	//����� �޾� �� ��ɿ� �°� ����
	HEADER header;

	//��� ����
	if (recv(clientSock, (char*)&header, sizeof(header), 0) > 0) {
		OutputDebugStringA("��� ����\n");
		char str[256];

		wsprintfA(str, "���: %d\n", header.command);
		OutputDebugStringA(str);
		wsprintfA(str, "������ ����: %d\n", header.dataLen);
		OutputDebugStringA(str);
		wsprintfA(str, "msgNum: %d\n", header.msgNum);
		OutputDebugStringA(str);
		wsprintfA(str, "msgTotalNum: %d\n", header.msgTotalNum);
		OutputDebugStringA(str);

		switch (header.command) {
		case COMMAND::COMMAND_REQUEST_FRAME:
			header.command += 1;
			header.dataLen = dataLen;
			header.msgNum = 1;
			header.msgTotalNum = 1;

			if (send(clientSock, (char*)&header, sizeof(header), 0) < 0) {
				//Ŭ���̾�Ʈ�� ���� �����
				OutputDebugStringA("��� ���� ����\n");
				closesocket(clientSock);
				clientSock = INVALID_SOCKET;
			}
			else {
				OutputDebugStringA("��� ���� ����\n");
				if (send(clientSock, (char*)data, dataLen, 0) < 0) {
					//Ŭ���̾�Ʈ�� ���� �����
					OutputDebugStringA("������ ���� ����\n");
					closesocket(clientSock);
					clientSock = INVALID_SOCKET;
				}
				else {
					OutputDebugStringA("������ ���� ����\n");
				}
			}

			break;
		}
		
	}
	else {
		OutputDebugStringA("��� ���� ����\n");
	}
	
}

void Server::SendData(void* data, int size) {

	//������ �۽�
	//���� ���� ������ ��ŭ�� ������ ����
	//�Ŀ� ���� ������ ��ŭ ������ ���� �������� �����ؾ���
	if (clientSock != INVALID_SOCKET) {
		if (send(clientSock, (char*)data, size, 0) < 0) {
			//Ŭ���̾�Ʈ�� ���� �����
			closesocket(clientSock);
			clientSock = INVALID_SOCKET;
		}
		else {
			OutputDebugStringA((char*)data);
		}
	}

}