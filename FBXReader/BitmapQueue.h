#pragma once
#include <queue>
#include <mutex>


//비트맵 큐
//네트워크에서 들어온 비트맵 데이터를 큐에 저장한다
class BitmapQueue {
public:
	BitmapQueue();
	virtual ~BitmapQueue();

private:
	std::queue<void*> mQueue;
	static std::mutex mMutex;

public:
	//Queue의 첫번째 원소 반환
	void* FrontItem();

	//Queue의 마지막 원소에 데이터 넣기
	void PushItem(void* item, std::mutex& mutex);

	//Queue의 첫번째 원소 삭제
	void PopItem(std::mutex& mutex);

	//현재  Queue의 Item 갯수 반환
	int Size();

};