#pragma once
#include <WinSock2.h>
#include <iostream>

template <typename T>
class RLE {
private:
	const int maxOverlappedCount = 255;

public:
	WSABUF* Encode(char* srcData, int srcDataSize);
	WSABUF* Decode(char* dstData, int dstSize);
};

template <typename T>
WSABUF* RLE<T>::Encode(char* srcData, int srcDataSize) {
	std::queue<char> tempDstBuf;

	for (int i = 0; i < srcDataSize; ) {
		T data;
		memcpy(&data, srcData + i, sizeof(T));

		unsigned char overlappedCount = 0;
		i += sizeof(T);

		for (int j = i; j < maxOverlappedCount; ) {
			T cmpData;
			memcpy(&cmpData, srcData + j, sizeof(T));

			if (data != cmpData) 
				break;
			
			++overlappedCount;
		}

		for (int j = 0; j < sizeof(T); ++j) {
			char byteInputData = data >> (8 * (sizeof(T) - 1 - j));
			tempDstBuf.push(byteInputData);
		}

		tempDstBuf.push(overlappedCount);
	}

	WSABUF* dstBuf = new WSABUF();
	char* buf = new char[tempDstBuf.size()];

	dstBuf->buf = buf;
	dstBuf->len = tempDstBuf.size();

	return dstBuf;
}
template <typename T>
WSABUF* RLE<T>::Decode(char* dstData, int dstSize) {

}





template <typename T = int32_t>
class LZSS {

private:
	struct HEADER {
		unsigned int flag : 8;     // flag�� 8��Ʈ ũ��
		unsigned int srcSize : 12;	//srcSize�� 24��Ʈ ũ��
	};
	struct LEN_POINTER {
		unsigned short len : 4;     // len�� 4��Ʈ ũ��
		unsigned short pointer : 12;     // pointer�� 12��Ʈ ũ��
	};
	const int magicStamp = 0x10;
	const int mWindowSize = 0x1000;
	const int mMax_LookaheadBufferSize = 0x14;

	const unsigned int mMin_size = 0x03;
	const unsigned int mMax_size = 0x10;
	const int mDataPackCount = 0x08;	//Data�� ������ 8����(keybyte�� 1����Ʈ�̹Ƿ�)


public:
	WSABUF* Encode(char* srcData, int srcDataSize);
	WSABUF* Decode(char* dstData, int dstSize);
};

template <typename T>
WSABUF* LZSS<T>::Encode(char* srcData, int srcDataSize) {
	//if srcData is nullptr, return
	if (srcData == nullptr)
		return nullptr;

	//�ʱ�ȭ(��¹��� �Ҵ�, �� ������ ���� �ʱ�ȭ)
	std::queue<char> tempEncode;	//������ ����� �������� ����

	char* window = srcData;	//�������� �� ����
	int curWindowSize = 0;

	char* lookaheadBufferAddr = srcData;	//������ ������ �ּ�
	int encodingPosition = 0;	//���� CP

	//������ ����
	while (encodingPosition < srcDataSize) {
		char keyByte = 0x00;	//Ű����Ʈ�� ����
		char tempPackArr[sizeof(T) * 8 * 2];
		int packArrIndex = 1;

		for (int i = 0; i < mDataPackCount; ++i) {
			if (encodingPosition >= srcDataSize)
				break;

			T dicData;	//�������� ���� ������
			T data;	//�����͸� �о��

			LEN_POINTER maxLP, lp;
			maxLP.len = lp.len = 0;
			maxLP.pointer = lp.pointer = 0;

			int windowIndex = 0;
			int lookBufferIndex = 0;
			int curLookBufferSize = srcDataSize - (lookaheadBufferAddr - srcData);
			//�������� ������ �˻�
			while (windowIndex < curWindowSize) {
				if (lookBufferIndex >= mMax_LookaheadBufferSize || (lookBufferIndex * sizeof(T)) >= curLookBufferSize)
					break;

				memcpy(&data, lookaheadBufferAddr + lookBufferIndex, sizeof(T));
				memcpy(&dicData, window + windowIndex, sizeof(T));

				if (data == dicData) {
					lookBufferIndex += 1;

					if (lp.len == 0x00)
						lp.pointer = windowIndex;
					lp.len += 1;

				}
				else {
					lookBufferIndex = 0;

					lp.len = 0;
					lp.pointer = 0;
				}

				++windowIndex;

				if (maxLP.len <= lp.len && lp.len != 0x00) {
					maxLP.len = lp.len;
					maxLP.pointer = lp.pointer;
				}
			}

			int overlapLen = 1;

			//�ߺ��� ����
			if (maxLP.len != 0 && maxLP.len > mMin_size) {
				//������

				//keybyte�� üũ
				keyByte |= 0x80 >> i;

				//������ ����
				overlapLen = maxLP.len;

				maxLP.len -= 3;
				maxLP.pointer = lookaheadBufferAddr - window - 1;

				//maxLP�� �ִ� ���� ���� ������ ť�� ����
				short inputData;
				memcpy(&inputData, &maxLP, sizeof(LEN_POINTER));
				for (int j = 0; j < sizeof(short); ++j) {
					char byteInputData = inputData << (8 * j);
					tempPackArr[packArrIndex++] = byteInputData;
					//tempOutput.push(byteInputData);
				}

				lookaheadBufferAddr += overlapLen * sizeof(T);
				encodingPosition += overlapLen * sizeof(T);

			}
			else {
				//������
				//�����͸� �ӽ� ���൥���� ť�� �ִ´�.
				memcpy(&data, lookaheadBufferAddr, sizeof(T));

				for (int j = 0; j < sizeof(T); ++j) {
					char byteInputData = data >> (8 * (sizeof(T) - 1 - j));
					tempPackArr[packArrIndex++] = byteInputData;
					//tempOutput.push(byteInputData);
				}

				lookaheadBufferAddr += sizeof(T);
				encodingPosition += sizeof(T);
			}

			//lookaheadAddr - windowAddr�� mWindowSize���� Ŀ���� windowAddr�̵�
			if (lookaheadBufferAddr - window > mWindowSize) {
				++window;
			}

			curWindowSize += overlapLen;


		}
		tempPackArr[0] = keyByte;

		for (int j = 0; j < packArrIndex; ++j) {
			tempEncode.push(tempPackArr[j]);
		}

	}


	//��� ����
	WSABUF* encodeBuf = new WSABUF();
	char* encodeData = nullptr;	//������ ����� ����
	int encodeSize = tempEncode.size();
	encodeData = new char[encodeSize + sizeof(HEADER)];

	//��� �Է�
	HEADER header;
	memset(&header, 0x00, sizeof(HEADER));
	header.flag = magicStamp;
	header.srcSize = srcDataSize;
	memcpy(encodeData, &header, sizeof(HEADER));

	//������ �Է�
	for (int i = sizeof(HEADER); i < encodeSize + (int)sizeof(HEADER); ++i) {
		encodeData[i] = tempEncode.front();
		tempEncode.pop();
	}
	encodeBuf->buf = encodeData;
	encodeBuf->len = encodeSize + sizeof(HEADER);

	return encodeBuf;
}

template <typename T>
WSABUF* LZSS<T>::Decode(char* dstData, int dstSize) {
	//��� üũ
	HEADER header;
	memcpy(&header, dstData, sizeof(HEADER));

	if (header.flag != 0x10)
		return nullptr;

	//�ʱ�ȭ
	const int srcDataSize = header.srcSize;
	char* srcData = new char[srcDataSize];
	int srcIndex = 0;

	char* window = srcData;	//�������� �� ����

	char* lookaheadBufferAddr = dstData + sizeof(HEADER);	//������ ������ �ּ�
	int decodingPosition = 0;	//���� CP

	//����Ǯ��
	/*		*/
	while (srcIndex < srcDataSize) {
		char keyByte = 0x00;	//Ű����Ʈ�� ����
		memcpy(&keyByte, lookaheadBufferAddr + decodingPosition, sizeof(keyByte));
		++decodingPosition;
		/*			*/
		for (int i = 0; i < mDataPackCount; ++i) {
			if (srcIndex >= srcDataSize)
				break;
			int overlapSize = 1;
			if ((keyByte >> (7 - i) & 0x01) == 0x01) {
				//������ �Ǿ�������
				LEN_POINTER lp;
				memcpy(&lp, lookaheadBufferAddr + decodingPosition, sizeof(LEN_POINTER));

				lp.len += 3;
				lp.pointer += 1;

				memcpy(srcData + srcIndex, srcData + (srcIndex - lp.pointer), lp.len * sizeof(T));
				overlapSize = lp.len;
				decodingPosition += sizeof(LEN_POINTER);
			}
			else {
				//������ �ȵǾ� ������ ������ �״�� ���
				memcpy(srcData + srcIndex, lookaheadBufferAddr + decodingPosition, sizeof(T));
				decodingPosition += sizeof(T);
			}

			srcIndex += overlapSize * sizeof(T);

			//lookaheadAddr - windowAddr�� mWindowSize���� Ŀ���� windowAddr�̵�
			if (lookaheadBufferAddr - window > mWindowSize) {
				++window;
			}
		}
	}

	//����Ǯ ��� ����
	WSABUF* decodeBuf = new WSABUF();
	decodeBuf->buf = srcData;
	decodeBuf->len = srcDataSize;

	return decodeBuf;
}