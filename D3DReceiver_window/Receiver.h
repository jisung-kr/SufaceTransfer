#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define PORT 45000

//#define SERVER_IP "127.0.0.1"
//#define SERVER_IP "61.73.65.218"
#define SERVER_IP "119.192.192.116"

enum COMMAND {
	//임시 명령
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
	UINT mDataLen;
	UINT mCommand;
};

struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = htons(COMMAND::COMMAND_REQ_FRAME);
	}

	CHEADER(USHORT command) {
		mDataLen = 0;
		mCommand = htons(command);
	}
	CHEADER(USHORT command, UINT dataLen) {
		mDataLen = htonl(dataLen);
		mCommand = htons(command);
	}
};

struct NETWORK_MSG {
	HEADER header;
	char* data;
};


class Client {
public:
	Client() = default;
	virtual ~Client();

private:
	WSAData wsaData;

	SOCKET serverSock;
	sockaddr_in serverAddr;

	HEADER resHeader;
	void* data = nullptr;

public:
	bool Init();
	bool Connection();
	bool ReadData();
	char* GetData();

	bool Request(HEADER header, void* data = nullptr);
	bool RecvResponse();

	void ReleaseBuffer();
};