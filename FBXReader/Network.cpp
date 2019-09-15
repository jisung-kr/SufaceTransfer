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

bool Server::RecvRequest(int sockIndex) {
	auto curClient = clients[sockIndex];
	auto curClientSock = clients[sockIndex]->GetClientSocket();

	//��� ����
	UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	while (true) {
		nowSize = recv(curClientSock, ((char*)& curClient->reqHeader) + totSize, headerSize - totSize, 0);

		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ��� ���� ����!\n");
		}
	}

	//���ۿ� ������ �޾ƿ���
	auto& header = curClient->reqHeader;
	UINT size = (UINT)ntohl(header.mDataLen);

	if (size < 0)
		return false;

	OutputDebugStringA("��� ���� ����\n");

	if (size != 0) {
		//��Ʈ��ũ ������ ���� ����
		totSize = 0;
		nowSize = 0;

		//�ش� Ŭ���̾�Ʈ Ŭ������ data�Ҵ� �� size����
		curClient->AllocDataMem(size);

		while (true) {
			nowSize = recv(serverSock, ((char*)curClient->GetDataMem()) + totSize, size - totSize, 0);
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

bool Server::Response(int sockIndex) {

	auto curClient = clients[sockIndex];
	auto curClientSock = clients[sockIndex]->GetClientSocket();

	//��� ����
	UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	while (true) {
		nowSize = recv(curClientSock, ((char*)& curClient->reqHeader) + totSize, headerSize - totSize, 0);

		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ��� ���� ����!\n");
		}
	}
	
	

	switch (curClient->reqHeader.mCommand) {

	case COMMAND::COMMAND_REQ_FRAME:
		//�������� �����͸� ������ ��


		//�۽�
		if (!SendMSG(sockIndex,CHEADER::CHEADER(COMMAND::COMMAND_RES_FRAME, curClient->dataSize), curClient->data)) {
			OutputDebugStringA("������ �۽� ����!\n");
			return false;
		}

		break;
	}


	return true;
}


bool Server::SendMSG(int sockIndex, HEADER resHeader, void* data) {
	auto curClientSock = clients[sockIndex]->GetClientSocket();


	//������ ��� ������ �۽�
	if (curClientSock != INVALID_SOCKET) {
		UINT headerSize = sizeof(HEADER);
		UINT totSize = 0;
		UINT nowSize = 0;
		while (true) {
			nowSize = send(curClientSock, ((char*)&resHeader) + totSize, headerSize - totSize, 0);

			if (nowSize < 0) {
				//Ŭ���̾�Ʈ�� ���� �����
				closesocket(curClientSock);
				curClientSock = INVALID_SOCKET;
				OutputDebugStringA("Ŭ���̾�Ʈ ���� ����\n");
				return false;
			}
			else {
				totSize += nowSize;
			}

			if (totSize >= headerSize)
				break;
		}
	}



	//�����͵� ���� �� ���� ����
	if (data != nullptr) {
		//������ �۽�
		if (curClientSock != INVALID_SOCKET) {
			UINT dataSize = ntohl(resHeader.mDataLen);
			UINT totSize = 0;
			UINT nowSize = 0;

			while (true) {
				nowSize = send(curClientSock, ((char*)data) + totSize, dataSize - totSize, 0);

				if (nowSize < 0) {
					//Ŭ���̾�Ʈ�� ���� �����
					closesocket(curClientSock);
					curClientSock = INVALID_SOCKET;
					OutputDebugStringA("Ŭ���̾�Ʈ ���� ����\n");
					return false;
				}
				else {
					totSize += nowSize;
				}

				if (totSize >= dataSize)
					break;
			}
		}
	}


	
	return true;
}
