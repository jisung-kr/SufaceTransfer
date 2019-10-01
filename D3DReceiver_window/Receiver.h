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
#define SERVER_IP "121.131.167.123"

enum COMMAND {
	COMMAND_REQ_FRAME = 0,
	COMMAND_RES_FRAME = 1,
	COMMAND_INPUT = 2,
	COMMAND_MAX
};

enum INPUT_TYPE {
	INPUT_KEY_W = 0,
	INPUT_KEY_S = 1,
	INPUT_KEY_A = 2,
	INPUT_KEY_D = 3,
	INPUT_AXIS_CAMERA_MOVE,
	INPUT_AXIS_CAMERA_ROT,
	INPUT_MAX
};

struct INPUT_DATA {
	INPUT_TYPE mInputType;
	float deltaTime;
	float x;
	float y;
	float z;
	float w;
};

struct HEADER {
	DWORD64 mDataLen;
	DWORD64 mCommand;
};

//헤더 생성 보조 구조체
struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = htonl(COMMAND::COMMAND_REQ_FRAME);
	}

	CHEADER(DWORD64 command) {
		mDataLen = 0;
		mCommand = htonl(command);
	}
	CHEADER(DWORD64 command, DWORD64 dataLen) {
		mDataLen = htonl(dataLen);
		mCommand = htonl(command);
	}
};

struct Packet {
	WSABUF mHeader;
	WSABUF mData;
	const DWORD64 headerSize = sizeof(HEADER);

	Packet() {
		mHeader.buf = new char[headerSize];
		mHeader.len = headerSize;
		mData.buf = nullptr;
	}

	Packet(HEADER* header, void* data = nullptr) {
		mHeader.buf = (char*)header;
		mHeader.len = headerSize;
		mData.buf = nullptr;
		if (data != nullptr) {
			DWORD64 dataSize = ntohl(header->mDataLen);
			mData.buf = (char*)data;
			mData.len = dataSize;
		}
	}

	void AllocDataBuffer(int size) {
		if (mData.buf == nullptr) {
			mData.buf = new char[size];
			mData.len = size;
		}
	}

};

class Client {
public:
	Client() = default;
	virtual ~Client();

private:
	WSADATA wsaData;
	SOCKET serverSock;
	sockaddr_in serverAddr;

	QueueEX<Packet*> rQueue;
	QueueEX<Packet*> wQueue;

	std::atomic<bool> IsUsingRQueue = false;
	std::atomic<bool> IsUsingWQueue = false;

	std::atomic<int> CountCMDRequestFrame = 0;

	DWORD64 headerSize = sizeof(HEADER);

public:
	bool Init();
	bool Connection();

	bool RecvMSG();
	bool SendMSG();

	void PushPacketWQueue(Packet* packet);
	void PopPacketRQueue();

	int SizeRQueue() { return rQueue.Size(); }
	int SizeWQueue() { return wQueue.Size(); }

	char* GetData();
	void ReleaseBuffer();

private:
	bool RecvHeader(Packet& packet);
	bool RecvData(Packet& packet);

	bool SendHeader(Packet& packet);
	bool SendData(Packet& packet);
};