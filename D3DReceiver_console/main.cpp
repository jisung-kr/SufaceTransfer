#include "Receiver.h"

#include <iostream>

using namespace std;

int main() {
	Client* client = new Client();

	if (!client->Init()) {
		cerr << "Init Error" << endl;
		delete client;
		return 1;
	}

	if (!client->Connection()) {
		cerr << "Connection Error" << endl;
		delete client;
		return 1;
	}


	//명령 보내기
	HEADER header;
	header.command = COMMAND::COMMAND_REQUEST_FRAME;
	header.dataLen = 0;
	header.msgNum = 1;
	header.msgTotalNum = 1;

	unsigned char* data = nullptr;
	client->SendMSG(header, (char**)&data);

	for (int i = 0; i < header.dataLen; ++i) {
		if (i % 16 == 0 && i != 0)
			printf("\n");

		printf("%02X ", data[i]);
	}

	return 0;
}