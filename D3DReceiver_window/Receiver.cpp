#include "Receiver.h"


Client::~Client() {
	
	closesocket(serverSock);

	WSACleanup();
}

bool Client::Init() {
	//윈속 초기화
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		return false;
	}

	//소켓 생성
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return false;
	}

	//소켓 설정
	memset(&serverAddr, 0x00, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	ULONG ulongAddr;
	InetPtonA(AF_INET, SERVER_IP, &ulongAddr);
	serverAddr.sin_addr.S_un.S_addr = ulongAddr;

	return true;
}

bool Client::Connection() {
	//서버와 커넥트
	if (connect(serverSock, (sockaddr*)& serverAddr, sizeof(serverAddr)) == INVALID_SOCKET) {
		return false;
	}

	return true;
}


bool Client::ReadData() {
	//문자열 수신
	unsigned int size = 0;
	unsigned int totSize = 0;
	unsigned int nowSize = 0;
	char str[256];

	//버퍼 크기 받아오기
	while (true) {
		nowSize = recv(serverSock, ((char*)&size) + totSize, sizeof(unsigned int), 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= sizeof(unsigned int))
				break;
		}
		else {
			OutputDebugStringA("데이터 실패\n");
			return false;
		}
	}

	//버퍼에 데이터 받아오기
	size = (UINT)ntohl(size);

	if (size <= 0)
		return false;

	totSize = 0;
	nowSize = 0;

	data = new char[size];
	ZeroMemory(data, size);

	while (true) {
		nowSize = recv(serverSock, ((char*)data) + totSize, size - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			wsprintfA(str, "현재 수신된 데이터 %d / %d\n", totSize, size);
			OutputDebugStringA(str);

			if (totSize >= size)
				break;
		}
		else {
			OutputDebugStringA("데이터 실패\n");
			return false;
		}
	}

	OutputDebugStringA("수신 완료!\n");
	return true;
}

bool Client::RecvResponse() {
	//RES수신
	memset(&resHeader, -1, sizeof(HEADER));
	const UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	//헤더 받아오기
	while (true) {
		nowSize = recv(serverSock, ((char*)& resHeader) + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("RES헤더 수신 실패\n");
			return false;
		}
	}

	//버퍼에 데이터 받아오기
	const UINT size = (UINT)ntohl(resHeader.mDataLen);

	if (size < 0)
		return false;

	if (size != 0) {
		totSize = 0;
		nowSize = 0;

		data = new char[size];
		ZeroMemory(data, size);

		while (true) {
			nowSize = recv(serverSock, ((char*)data) + totSize, size - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				char str[256];
				wsprintfA(str, "현재 수신된 데이터 %d / %d\n", totSize, size);
				OutputDebugStringA(str);

				if (totSize >= size)
					break;
			}
			else {
				OutputDebugStringA("데이터 수신 실패\n");
				return false;
			}
		}

	}

	OutputDebugStringA("수신 완료!\n");
	return true;
}

bool Client::Request(HEADER reqHeader, void* data) {
	UINT headerSize = sizeof(HEADER);
	UINT totSize = 0;
	UINT nowSize = 0;

	//REQ전송
	while (true) {
		nowSize = send(serverSock, ((char*)& reqHeader) + totSize, headerSize - totSize, 0);
		if (nowSize > 0) {
			totSize += nowSize;

			if (totSize >= headerSize)
				break;
		}
		else {
			OutputDebugStringA("REQ헤더 송신 실패\n");
			return false;
		}
	}

	//Data도 있을 시 같이 전송
	if (data != nullptr) {
		const UINT dataSize = reqHeader.mDataLen;
		totSize = 0;
		nowSize = 0;

		while (true) {
			nowSize = send(serverSock, ((char*)data) + totSize, dataSize - totSize, 0);
			if (nowSize > 0) {
				totSize += nowSize;

				if (totSize >= dataSize)
					break;
			}
			else {
				OutputDebugStringA("Data 송신 실패\n");
				return false;
			}
		}
	}

	return true;
}

char* Client::GetData() {
	return (char*)data;
}

void Client::ReleaseBuffer() {
	 if (data != nullptr) {
		delete data;
		data = nullptr;
	} 
}


