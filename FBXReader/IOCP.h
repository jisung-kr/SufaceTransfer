#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "Camera.h"

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
	INT mDataLen;
	INT mCommand;
};

//��� ���� ���� ����ü
struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = htonl(COMMAND::COMMAND_REQ_FRAME);
	}

	CHEADER(USHORT command) {
		mDataLen = 0;
		mCommand = htonl(command);
	}
	CHEADER(USHORT command, UINT dataLen) {
		mDataLen = htonl(dataLen);
		mCommand = htonl(command);
	}
};

//SocketInfo ����ü
struct SocketInfo {
	OVERLAPPED overlapped;
	SOCKET socket;
	sockaddr_in clientAddr;
	WSABUF wsaReadBuf[2];
	WSABUF wsaWriteBuf[2];
	DWORD flag;
	DWORD readn;
	DWORD writen;

	Camera mCamera;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;	//CmdList Allocator
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;	//Command List

	bool mIsReadBufUsing = false;
	bool mIsWriteBufUsing = false;

	~SocketInfo() {
		if (wsaReadBuf[0].buf != nullptr) {
			delete wsaReadBuf[0].buf;
			wsaReadBuf[0].buf = nullptr;
		}
		if (wsaReadBuf[1].buf != nullptr) {
			delete wsaReadBuf[1].buf;
			wsaReadBuf[1].buf = nullptr;
		}

		if (wsaWriteBuf[0].buf != nullptr) {
			delete wsaWriteBuf[0].buf;
			wsaWriteBuf[0].buf = nullptr;
		}
		if (wsaWriteBuf[1].buf != nullptr) {
			delete wsaWriteBuf[1].buf;
			wsaWriteBuf[1].buf = nullptr;
		}
	}
};

//ServerŬ����
class IOCPServer : public IOCP {
public:
	IOCPServer() = default;
	virtual ~IOCPServer();

private:
	SOCKET listenSock;	//���� ���� ����
	sockaddr_in serverAddr;	//���� �ּ�
	std::vector<std::shared_ptr<SocketInfo>> clients;	//���ӵ� Ŭ���̾�Ʈ��

	int count = 0;
public:
	bool Init() override;	//�ʱ�ȭ
	void AcceptClient();	//Client ����

	void RequestRecv(int sockIdx, bool overlapped = true);	//��ø���Ͽ� ���ſ�û
	void RequestSend(int sockIdx, HEADER header, void* data = nullptr, bool overlapped = true);	//��ø���Ͽ� �۽ſ�û


	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetBuffer;	//RenderTarget Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;	//DepthStencil Buffer

	INT GetClientNum() { return clients.size(); }
	std::shared_ptr<SocketInfo> GetClient(int idx) { return clients[idx]; }
private:
	void RunNetwork(void* cp) override;	//������

	bool RecvHeader(SocketInfo* sInfo, DWORD nowSize);	//Header ����
	bool SendHeader(SocketInfo* sInfo, DWORD nowSize);	//Header �۽�

	bool RecvData(SocketInfo* sInfo);	//Data ����
	bool SendData(SocketInfo* sInfo);	//Data �۽�

}; 