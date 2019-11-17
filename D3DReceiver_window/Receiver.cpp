#include "Receiver.h"

using namespace std;

Client::Client(char* ip, short port) {
	strcpy_s(serverIP, sizeof(serverIP), ip);
	serverPort = port;
}

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
	serverAddr.sin_port = htons(serverPort);
	ULONG ulongAddr;
	InetPtonA(AF_INET, serverIP, &ulongAddr);
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

bool Client::RecvMSG() {
	if (isUsingRQueue == false) {
		isUsingRQueue = true;
		unique_ptr<Packet> packet = make_unique<Packet>();

		if (!RecvHeader(packet.get())) {
			return false;
		}
		if (!RecvData(packet.get())) {
			return false;
		}
		isUsingRQueue = false;
		rQueue.PushItem(std::move(packet));

		OutputDebugStringA("Queue�� Packet ����\n");
	}

	
	return true;
}

bool Client::RecvHeader(Packet* packet) {
	DWORD64 totSize = 0;
	DWORD64 nowSize = 0;

	//��� ����
	while (true) {

		nowSize = recv(serverSock, (char*)packet->mHeader.buf + totSize, headerSize - totSize, 0);
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
	HEADER* header = (HEADER*)packet->mHeader.buf;
	if (ntohl(header->mCommand) >= COMMAND::COMMAND_MAX) {
		OutputDebugStringA("��� ���� ����\n");
		return false;
	}
	OutputDebugStringA("��� ���� ����\n");
	return true;
}

bool Client::RecvData(Packet* packet) {
	const HEADER& header = *(HEADER*)packet->mHeader.buf;
	DWORD64 size = ntohl(header.mDataLen);
	DWORD64 totSize = 0;
	DWORD64 nowSize = 0;

	if (size > 0) {
		packet->AllocDataBuffer(size);

		while (true) {
			nowSize = recv(serverSock, (char*)packet->mData.buf + totSize, size - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "���� ���ŵ� ������ %I64u / %I64u\n", totSize, size);
				
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

bool Client::SendMSG() {

	while (inputWQueue.Size() > 0 && isUsingInputWQueue == false) {
		isUsingInputWQueue = true;
		unique_ptr<Packet> packet = std::move(inputWQueue.FrontItem());
		inputWQueue.PopItem();

		if (!SendHeader(packet.get()))
			return false;
		if (!SendData(packet.get())) {
			return false;
		}
		isUsingInputWQueue = false;
		delete packet.release();
		OutputDebugStringA("Queue���� InputPacket ����\n");
	}

	if (wQueue.Size() > 0  && isUsingWQueue == false) {
		isUsingWQueue = true;
		unique_ptr<Packet> packet = std::move(wQueue.FrontItem());
		wQueue.PopItem();

		if (!SendHeader(packet.get()))
			return false;
		if (!SendData(packet.get())) {
			return false;
		}
		isUsingWQueue = false;
		delete packet.release();
		OutputDebugStringA("Queue���� Packet ����\n");
	}

	return true;
}

bool Client::SendHeader(Packet* packet) {
	DWORD64 totSize = 0;
	DWORD64 nowSize = 0;

	//��� �۽�
	while (true) {
		nowSize = send(serverSock, (char*)packet->mHeader.buf + totSize, headerSize - totSize, 0);
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


bool Client::SendData(Packet* packet) {
	HEADER* header = (HEADER*)packet->mHeader.buf;
	WSABUF& data = packet->mData;
	const DWORD64 dataSize = ntohl(header->mDataLen);

	DWORD64 totSize = 0;
	DWORD64 nowSize = 0;

	if (data.buf != nullptr && dataSize > 0) {
		while (true) {
			nowSize = send(serverSock, (char*)data.buf + totSize, (int)(dataSize - totSize), 0);
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

void Client::PushPacketWQueue(unique_ptr<Packet> packet) {
	
	HEADER* header = (HEADER*)packet->mHeader.buf;

	if (ntohl(header->mCommand) == COMMAND::COMMAND_INPUT) {
		inputWQueue.PushItem(std::move(packet));
	}
	else if(reqFrameCount < 3){
		++reqFrameCount;
		wQueue.PushItem(std::move(packet));
	}

}
void Client::PopPacketRQueue() {
	--reqFrameCount;
	auto p = rQueue.FrontItem().release();
	if (p)
		delete p;
	rQueue.PopItem();
}

int Client::GetDataSize() {
	Packet* packet = rQueue.FrontItem().get();

	return (int)packet->mData.len;
}

char* Client::GetData() {
	Packet* packet = rQueue.FrontItem().get();
	
	return (char*)packet->mData.buf;
}

SOCKET Client::GetSocket() {
	return serverSock;
}


