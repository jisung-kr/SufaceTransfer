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
		unsigned int flag : 8;     // flag는 8비트 크기
		unsigned int srcSize : 12;	//srcSize는 24비트 크기
	};
	struct LEN_POINTER {
		unsigned short len : 4;     // len는 4비트 크기
		unsigned short pointer : 12;     // pointer는 12비트 크기
	};
	const int magicStamp = 0x10;
	const int mWindowSize = 0x1000;
	const int mMax_LookaheadBufferSize = 0x14;

	const unsigned int mMin_size = 0x03;
	const unsigned int mMax_size = 0x10;
	const int mDataPackCount = 0x08;	//Data의 갯수는 8개로(keybyte가 1바이트이므로)


public:
	WSABUF* Encode(char* srcData, int srcDataSize);
	WSABUF* Decode(char* dstData, int dstSize);
};

template <typename T>
WSABUF* LZSS<T>::Encode(char* srcData, int srcDataSize) {
	//if srcData is nullptr, return
	if (srcData == nullptr)
		return nullptr;

	//초기화(출력버퍼 할당, 각 변수의 값들 초기화)
	std::queue<char> tempEncode;	//압축한 결과를 동적으로 저장

	char* window = srcData;	//사전으로 쓸 벡터
	int curWindowSize = 0;

	char* lookaheadBufferAddr = srcData;	//압축할 버퍼의 주소
	int encodingPosition = 0;	//현재 CP

	//데이터 압축
	while (encodingPosition < srcDataSize) {
		char keyByte = 0x00;	//키바이트를 생성
		char tempPackArr[sizeof(T) * 8 * 2];
		int packArrIndex = 1;

		for (int i = 0; i < mDataPackCount; ++i) {
			if (encodingPosition >= srcDataSize)
				break;

			T dicData;	//사전에서 읽은 데이터
			T data;	//데이터를 읽어옴

			LEN_POINTER maxLP, lp;
			maxLP.len = lp.len = 0;
			maxLP.pointer = lp.pointer = 0;

			int windowIndex = 0;
			int lookBufferIndex = 0;
			int curLookBufferSize = srcDataSize - (lookaheadBufferAddr - srcData);
			//사전에서 데이터 검색
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

			//중복된 것이
			if (maxLP.len != 0 && maxLP.len > mMin_size) {
				//있으면

				//keybyte에 체크
				keyByte |= 0x80 >> i;

				//데이터 가공
				overlapLen = maxLP.len;

				maxLP.len -= 3;
				maxLP.pointer = lookaheadBufferAddr - window - 1;

				//maxLP에 있는 값을 압축 데이터 큐에 저장
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
				//없으면
				//데이터를 임시 압축데이터 큐에 넣는다.
				memcpy(&data, lookaheadBufferAddr, sizeof(T));

				for (int j = 0; j < sizeof(T); ++j) {
					char byteInputData = data >> (8 * (sizeof(T) - 1 - j));
					tempPackArr[packArrIndex++] = byteInputData;
					//tempOutput.push(byteInputData);
				}

				lookaheadBufferAddr += sizeof(T);
				encodingPosition += sizeof(T);
			}

			//lookaheadAddr - windowAddr가 mWindowSize보다 커지면 windowAddr이동
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


	//결과 리턴
	WSABUF* encodeBuf = new WSABUF();
	char* encodeData = nullptr;	//압축한 결과를 저장
	int encodeSize = tempEncode.size();
	encodeData = new char[encodeSize + sizeof(HEADER)];

	//헤더 입력
	HEADER header;
	memset(&header, 0x00, sizeof(HEADER));
	header.flag = magicStamp;
	header.srcSize = srcDataSize;
	memcpy(encodeData, &header, sizeof(HEADER));

	//데이터 입력
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
	//헤더 체크
	HEADER header;
	memcpy(&header, dstData, sizeof(HEADER));

	if (header.flag != 0x10)
		return nullptr;

	//초기화
	const int srcDataSize = header.srcSize;
	char* srcData = new char[srcDataSize];
	int srcIndex = 0;

	char* window = srcData;	//사전으로 쓸 벡터

	char* lookaheadBufferAddr = dstData + sizeof(HEADER);	//압축할 버퍼의 주소
	int decodingPosition = 0;	//현재 CP

	//압축풀기
	/*		*/
	while (srcIndex < srcDataSize) {
		char keyByte = 0x00;	//키바이트를 생성
		memcpy(&keyByte, lookaheadBufferAddr + decodingPosition, sizeof(keyByte));
		++decodingPosition;
		/*			*/
		for (int i = 0; i < mDataPackCount; ++i) {
			if (srcIndex >= srcDataSize)
				break;
			int overlapSize = 1;
			if ((keyByte >> (7 - i) & 0x01) == 0x01) {
				//압축이 되어있으면
				LEN_POINTER lp;
				memcpy(&lp, lookaheadBufferAddr + decodingPosition, sizeof(LEN_POINTER));

				lp.len += 3;
				lp.pointer += 1;

				memcpy(srcData + srcIndex, srcData + (srcIndex - lp.pointer), lp.len * sizeof(T));
				overlapSize = lp.len;
				decodingPosition += sizeof(LEN_POINTER);
			}
			else {
				//압축이 안되어 있으면 데이터 그대로 출력
				memcpy(srcData + srcIndex, lookaheadBufferAddr + decodingPosition, sizeof(T));
				decodingPosition += sizeof(T);
			}

			srcIndex += overlapSize * sizeof(T);

			//lookaheadAddr - windowAddr가 mWindowSize보다 커지면 windowAddr이동
			if (lookaheadBufferAddr - window > mWindowSize) {
				++window;
			}
		}
	}

	//압축풀 결과 리턴
	WSABUF* decodeBuf = new WSABUF();
	decodeBuf->buf = srcData;
	decodeBuf->len = srcDataSize;

	return decodeBuf;
}