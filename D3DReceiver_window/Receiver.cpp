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
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
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
	if (connect(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr)) == INVALID_SOCKET) {
		return false;
	}

	return true;
}

void Client::SendMSG(HEADER& header, char** data) {

	//��� ������
	if (send(serverSock, (char*)&header, sizeof(HEADER), 0) < 0) {
		//send �Լ� ����
		OutputDebugStringA("��� �۽� ����\n");
		closesocket(serverSock);
		serverSock = INVALID_SOCKET;
	}
	else {
		OutputDebugStringA("��� �۽� ����\n");

		/*		*/
		//��� ���� �� ������ �ޱ�
		if (recv(serverSock, (char*)&header, sizeof(HEADER), 0) > 0) {
			OutputDebugStringA("��� ���� ����\n");

			//recv �Լ� ����
			*data = (char*)malloc(sizeof(char) * header.dataLen);
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
			case COMMAND::COMMAND_REQUEST_FRAME_ACK:
				int size = 0;
				if ((size = recv(serverSock, (char*)*data, header.dataLen, 0)) > 0) {
					OutputDebugStringA("������ ���� ����, size\n" + size);

				}
				else {
					//recv �Լ� ����
					OutputDebugStringA("������ ���� ����\n");
					closesocket(serverSock);
					serverSock = INVALID_SOCKET;
				}
				break;
			}
		}
		else {
			//recv�Լ� ����
			OutputDebugStringA("��� ���� ����\n");
			closesocket(serverSock);
			serverSock = INVALID_SOCKET;
			
		}

	}

}

bool Client::ReadData() {
	//���ڿ� ����
	ZeroMemory(rBuf, sizeof(rBuf));
	unsigned int size = 0;

	if (recv(serverSock, (char*)&size, sizeof(unsigned int), 0) > 0) {
		size = (unsigned int)ntohl(size);
		//������ ���� ����
		if (size != 0) {
			data = new char[size];
			if (recv(serverSock, (char*)data, size, 0) > 0) {
				OutputDebugStringA("������ ����\n");
			}
			else {
				OutputDebugStringA("������ ����\n");
			}
		}

	}
	else
		return false;

	return true;
}


char* Client::GetData() {
	return (char*)data;
}
int Client::GetDataSize() {
	return BUFFER_SIZE;
}

