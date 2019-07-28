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

	if (!client->ReadData()) {
		cerr << "Reading Data Error" << endl;
		delete client;
		return 1;
	}


	//데이터 읽었으니 출력
	unsigned char* buffer = (unsigned char*)client->GetData();

	for (int i = 0; i < BUFFER_SIZE; ++i) {
		if (i % 16 == 0 && i != 0)
			printf("\n");

		printf("%02X ", buffer[i]);
	}


	return 0;
}