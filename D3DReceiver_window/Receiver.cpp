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

void Client::SendMSG(HEADER& header, char** data) {

	//명령 보내기
	if (send(serverSock, (char*)&header, sizeof(HEADER), 0) < 0) {
		//send 함수 실패
		OutputDebugStringA("명령 송신 실패\n");
		closesocket(serverSock);
		serverSock = INVALID_SOCKET;
	}
	else {
		OutputDebugStringA("명령 송신 성공\n");

		/*		*/
		//헤더 보낸 후 데이터 받기
		if (recv(serverSock, (char*)&header, sizeof(HEADER), 0) > 0) {
			OutputDebugStringA("헤더 수신 성공\n");

			//recv 함수 성공
			*data = (char*)malloc(sizeof(char) * header.dataLen);
			OutputDebugStringA("헤더 정보\n");
			char str[256];
			wsprintfA(str, "명령: %d\n", header.command);
			OutputDebugStringA(str);

			wsprintfA(str, "데이터 길이: %d\n", header.dataLen);
			OutputDebugStringA(str);

			wsprintfA(str, "msgNum: %d\n", header.msgNum);
			OutputDebugStringA(str);

			wsprintfA(str, "msgTotalNum: %d\n", header.msgTotalNum);
			OutputDebugStringA(str);

			switch (header.command) {
			case COMMAND::COMMAND_REQUEST_FRAME_ACK:
				int size = 0;
				if ((size = recv(serverSock, (char*)*data, header.dataLen, 0)) > 0) {
					OutputDebugStringA("데이터 수신 성공, size\n" + size);

				}
				else {
					//recv 함수 실패
					OutputDebugStringA("데이터 수신 실패\n");
					closesocket(serverSock);
					serverSock = INVALID_SOCKET;
				}
				break;
			}
		}
		else {
			//recv함수 실패
			OutputDebugStringA("헤더 수신 실패\n");
			closesocket(serverSock);
			serverSock = INVALID_SOCKET;
			
		}

	}

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
	size = (unsigned int)ntohl(size);

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


char* Client::GetData() {
	return (char*)data;
}


