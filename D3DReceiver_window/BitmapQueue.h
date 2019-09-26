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
	//Queue�� ù��° ���� ��ȯ
	T& FrontItem();

	//Queue�� ������ ���ҿ� ������ �ֱ�
	void PushItem(T item, std::mutex& mutex = mMutex);

	//Queue�� ù��° ���� ����
	void PopItem(std::mutex& mutex = mMutex);

	//����  Queue�� Item ���� ��ȯ
	int Size();
};

template <typename T>
std::mutex QueueEX<T>::mMutex;

//Queue�� ù��° ���� ��ȯ
template <typename T>
T& QueueEX<T>::FrontItem() {
	return mQueue.front();
}

//Queue�� ������ ���ҿ� ������ �ֱ�
template <typename T>
void QueueEX<T>::PushItem(T item, std::mutex& mutex) {
	mutex.lock();
	mQueue.push(item);
	mutex.unlock();

}

//Queue�� ù��° ���� ����
template <typename T>
void QueueEX<T>::PopItem(std::mutex& mutex) {
	mutex.lock();
	mQueue.pop();
	mutex.unlock();
}

//����  Queue�� Item ���� ��ȯ
template <typename T>
int QueueEX<T>::Size() {
	return (int)mQueue.size();
}
