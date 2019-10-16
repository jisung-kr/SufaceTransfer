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
	//Queue�� ù��° ���� ��ȯ
	T& FrontItem();

	//Queue�� ������ ���ҿ� ������ �ֱ�
	void PushItem(T item);

	//Queue�� ù��° ���� ����
	void PopItem();

	//����  Queue�� Item ���� ��ȯ
	int Size();
};

//Queue�� ù��° ���� ��ȯ
template <typename T>
T& QueueEX<T>::FrontItem() {
	std::unique_lock<std::mutex> lock(mMutex);

	while (mQueue.empty()) {
		mCond.wait(lock);
	}

	return mQueue.front();
}

//Queue�� ������ ���ҿ� ������ �ֱ�
template <typename T>
void QueueEX<T>::PushItem(T item) {
	mMutex.lock();
	mQueue.push(std::move(item));
	mMutex.unlock();

}

//Queue�� ù��° ���� ����
template <typename T>
void QueueEX<T>::PopItem() {
	std::unique_lock<std::mutex> lock(mMutex);

	while (mQueue.empty()) {
		mCond.wait(lock);
	}

	mQueue.pop();
}

//����  Queue�� Item ���� ��ȯ
template <typename T>
int QueueEX<T>::Size() {
	mMutex.lock();
	int size = (int)mQueue.size();
	mMutex.unlock();
	return size;
}
