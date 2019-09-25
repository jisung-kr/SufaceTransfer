#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <atomic>
#include "BitmapQueue.h"

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define PORT 45000

//#define SERVER_IP "127.0.0.1"
//#define SERVER_IP "61.73.65.218"
#define SERVER_IP "119.192.192.116"

enum COMMAND {
	COMMAND_REQ_FRAME = 0,
	COMMAND_RES_FRAME = 1,
	COMMAND_INPUT_KEY,
	COMMAND_MAX
};

enum INPUT_TYPE {
	INPUT_KEY_W = 0,
	INPUT_KEY_S = 1,
	INPUT_KEY_A = 2,
	INPUT_KEY_D = 3,
	INPUT_MAX
};

struct INPUT_DATA {
	INPUT_TYPE mInputType;
	float x;
	float y;
	float z;
};

struct HEADER {
	DWORD mDataLen;
	DWORD mCommand;
};

//헤더 생성 보조 구조체
struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = htonl(COMMAND::COMMAND_REQ_FRAME);
	}

	CHEADER(DWORD command) {
		mDataLen = 0;
		mCommand = htonl(command);
	}
	CHEADER(DWORD command, DWORD dataLen) {
		mDataLen = htonl(dataLen);
		mCommand = htonl(command);
	}
};

struct Packet {
	WSABUF mHeader;
	WSABUF mData;
	const DWORD headerSize = sizeof(HEADER);

	Packet(int dataSize = 0) {
		mHeader.buf = new char[headerSize];
		mHeader.len = headerSize;

		if (dataSize != 0) {
			mData.buf = new char[dataSize];
			mData.len = dataSize;
		}
	}

	Packet(HEADER* header, void* data = nullptr) {
		mHeader.buf = (char*)header;
		mHeader.len = headerSize;

		DWORD dataSize = ntohl(header->mDataLen);
		if (dataSize != 0 && data != nullptr) {
			mData.buf = (char*)data;
			mData.len = dataSize;
		}
	}

	void AllocDataBuffer(int size) {
		if (mData.buf != nullptr) {
			mData.buf = new char[size];
			mData.len = size;
		}
	}

};

class Client {
public:
	Client() = default;
	virtual ~Client();

public:
	QueueEX<Packet*> rQueue;
	QueueEX<Packet*> wQueue;

	std::atomic<bool> IsUsingRQueue = false;
	std::atomic<bool> IsUsingWQueue = false;

private:
	WSADATA wsaData;
	SOCKET serverSock;
	sockaddr_in serverAddr;

	DWORD headerSize = sizeof(HEADER);

public:
	bool Init();
	bool Connection();

	bool RecvMSG();
	bool SendMSG();

	char* GetData();
	void ReleaseBuffer();

private:
	bool RecvHeader(Packet& packet);
	bool RecvData(Packet& packet);

	bool SendHeader(Packet& packet);
	bool SendData(Packet& packet);
};