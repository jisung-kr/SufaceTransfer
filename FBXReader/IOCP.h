#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "Camera.h"
#include "BitmapQueue.h"

#define PORT 45000

#define MAXCLIENT 1


//IOCP ����Ŭ����
class IOCP {
protected:
	WSADATA wsaData;
	HANDLE mhIOCP;

public:
	virtual bool Init() = 0;
	virtual void RunNetwork(void* cp) = 0;
protected:

	void SetThread(std::function<void(IOCP*, void*)> thread = &IOCP::RunNetwork) {
		//cpu�ھ�� �˾Ƴ���
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		int numOfCore = sysInfo.dwNumberOfProcessors;

		//�ھ� ����ŭ ������ ����
		for (int i = 0; i < numOfCore * 2; ++i) {
			new std::thread(thread, this, mhIOCP);
		}
	}
};


enum IOCP_FLAG {
	IOCP_FLAG_READ,
	IOCP_FLAG_WRITE,
	IOCP_FLAG_MAX
};
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

//��� ���� ���� ����ü
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
		mHeader.len = 0;

		if (dataSize != 0) {
			mData.buf = new char[dataSize];
			mData.len = dataSize;
		}
	}

	Packet(HEADER* hedaer, void* data = nullptr, int dataSize = 0) {
		mHeader.buf = (char*)hedaer;
		mHeader.len = 0;

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

//Overlapped Ȯ�� ����ü
struct OVERLAPPEDEX{
	OVERLAPPED mOverlapped;
	DWORD mFlag;
	DWORD mNumberOfByte;

	Packet* mPacket = nullptr;
	const DWORD headerSize = sizeof(HEADER);
	/*
	OVERLAPPEDEX() {}

	OVERLAPPEDEX(IOCP_FLAG flag, int numOfByte = 0) : mFlag(flag){
		mNumberOfByte = headerSize;
	}
	OVERLAPPEDEX(Packet* packet, IOCP_FLAG flag, int numOfByte = 0) : mFlag(flag), mPacket(packet){
		mNumberOfByte = headerSize;
	}
	*/
};

//SocketInfo ����ü
struct SocketInfo {
	SOCKET socket;
	sockaddr_in clientAddr;

	Camera mCamera;

	QueueEX<Packet*> rQueue;
	QueueEX<Packet*> wQueue;
	
	std::atomic<bool> IsUsingRQueue = false;
	std::atomic<bool> IsUsingWQueue = false;


	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;	//CmdList Allocator
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;	//Command List

};

//ServerŬ����
class IOCPServer : public IOCP {
public:
	IOCPServer() = default;
	virtual ~IOCPServer();

private:
	SOCKET listenSock;	//���� ���� ����
	sockaddr_in serverAddr;	//���� �ּ�
	std::vector<SocketInfo*> clients;	//���ӵ� Ŭ���̾�Ʈ��

	const DWORD headerSize = sizeof(HEADER);

	int count = 0;
public:
	bool Init() override;	//�ʱ�ȭ
	void AcceptClient();	//Client ����

	void RequestRecv(int sockIdx, bool overlapped = true);	//��ø���Ͽ� ���ſ�û
	void RequestSend(int sockIdx, bool overlapped = true);	//��ø���Ͽ� �۽ſ�û


	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetBuffer;	//RenderTarget Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;	//DepthStencil Buffer

	INT GetClientNum() { return clients.size(); }
	SocketInfo* GetClient(int idx) { return clients[idx]; }
private:
	void RunNetwork(void* cp) override;	//������

	bool RecvHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize);	//Header ����
	bool RecvData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx);	//Data ����

	bool SendHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize);	//Header �۽�
	bool SendData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx);	//Data �۽�
}; 

