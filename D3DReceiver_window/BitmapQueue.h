#pragma once
#include <queue>
#include <mutex>

template <typename T>
class QueueEX {
public:
	QueueEX() = default;
	virtual ~QueueEX() = default;

private:
	std::queue<T> mQueue;

public:
	static std::mutex mMutex;

public:
	//Queue의 첫번째 원소 반환
	T& FrontItem();

	//Queue의 마지막 원소에 데이터 넣기
	void PushItem(T item, std::mutex& mutex = mMutex);

	//Queue의 첫번째 원소 삭제
	void PopItem(std::mutex& mutex = mMutex);

	//현재  Queue의 Item 갯수 반환
	int Size();
};

template <typename T>
std::mutex QueueEX<T>::mMutex;

//Queue의 첫번째 원소 반환
template <typename T>
T& QueueEX<T>::FrontItem() {
	return mQueue.front();
}

//Queue의 마지막 원소에 데이터 넣기
template <typename T>
void QueueEX<T>::PushItem(T item, std::mutex& mutex) {
	mutex.lock();
	mQueue.push(item);
	mutex.unlock();
}

//Queue의 첫번째 원소 삭제
template <typename T>
void QueueEX<T>::PopItem(std::mutex& mutex) {
	mutex.lock();
	mQueue.pop();
	mutex.unlock();
}

//현재  Queue의 Item 갯수 반환
template <typename T>
int QueueEX<T>::Size() {
	return (int)mQueue.size();
}


//비트맵 큐
//네트워크에서 들어온 비트맵 데이터를 큐에 저장한다
class BitmapQueue {
public:
	BitmapQueue();
	virtual ~BitmapQueue();

private:
	std::queue<void*> mQueue;

public:
	static std::mutex mMutex;

public:
	//Queue의 첫번째 원소 반환
	void* FrontItem();

	//Queue의 마지막 원소에 데이터 넣기
	void PushItem(void* item, std::mutex& mutex = mMutex);

	//Queue의 첫번째 원소 삭제
	void PopItem(std::mutex& mutex = mMutex);

	//현재  Queue의 Item 갯수 반환
	int Size();

};