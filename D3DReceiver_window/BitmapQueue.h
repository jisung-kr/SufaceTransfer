#pragma once
#include <queue>
#include <mutex>
#include <atomic>

template <typename T>
class QueueEX {
public:
	QueueEX() = default;
	virtual ~QueueEX() = default;

private:
	std::queue<T> mQueue;
	std::mutex mMutex;
	std::condition_variable mCond;

public:
	//Queue의 첫번째 원소 반환
	T& FrontItem();

	//Queue의 마지막 원소에 데이터 넣기
	void PushItem(T item);

	//Queue의 첫번째 원소 삭제
	void PopItem();

	//현재  Queue의 Item 갯수 반환
	int Size();
};

//Queue의 첫번째 원소 반환
template <typename T>
T& QueueEX<T>::FrontItem() {
	std::unique_lock<std::mutex> lock(mMutex);

	while (mQueue.empty()) {
		mCond.wait(lock);
	}

	return mQueue.front();
}

//Queue의 마지막 원소에 데이터 넣기
template <typename T>
void QueueEX<T>::PushItem(T item) {
	mMutex.lock();
	mQueue.push(std::move(item));
	mMutex.unlock();

}

//Queue의 첫번째 원소 삭제
template <typename T>
void QueueEX<T>::PopItem() {
	std::unique_lock<std::mutex> lock(mMutex);

	while (mQueue.empty()) {
		mCond.wait(lock);
	}

	mQueue.pop();
}

//현재  Queue의 Item 갯수 반환
template <typename T>
int QueueEX<T>::Size() {
	mMutex.lock();
	int size = (int)mQueue.size();
	mMutex.unlock();
	return size;
}
