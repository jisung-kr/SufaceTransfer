#pragma once
#include <queue>
#include <mutex>


//��Ʈ�� ť
//��Ʈ��ũ���� ���� ��Ʈ�� �����͸� ť�� �����Ѵ�
class BitmapQueue {
public:
	BitmapQueue();
	virtual ~BitmapQueue();

private:
	std::queue<void*> mQueue;
	static std::mutex mMutex;

public:
	//Queue�� ù��° ���� ��ȯ
	void* FrontItem();

	//Queue�� ������ ���ҿ� ������ �ֱ�
	void PushItem(void* item, std::mutex& mutex);

	//Queue�� ù��° ���� ����
	void PopItem(std::mutex& mutex);

	//����  Queue�� Item ���� ��ȯ
	int Size();

};