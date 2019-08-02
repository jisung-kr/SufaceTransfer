#pragma once
#include <queue>



//��Ʈ�� ť
//��Ʈ��ũ���� ���� ��Ʈ�� �����͸� ť�� �����Ѵ�
class BitmapQueue {
public:
	BitmapQueue();
	virtual ~BitmapQueue();

private:
	std::queue<void*> mQueue;

public:
	//Queue�� ù��° ���� ��ȯ
	void* FrontItem();

	//Queue�� ������ ���ҿ� ������ �ֱ�
	void PushItem(void* item);

	//Queue�� ù��° ���� ����
	void PopItem();

	//����  Queue�� Item ���� ��ȯ
	int Size();

};