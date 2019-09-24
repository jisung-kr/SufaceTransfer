#include "BitmapQueue.h"


std::mutex BitmapQueue::mMutex;

//������
BitmapQueue::BitmapQueue() {


}

//�ı���
BitmapQueue::~BitmapQueue() {

}



//Queue�� ù��° ���� ��ȯ
void* BitmapQueue::FrontItem() {
	return mQueue.front();
}

//Queue�� ������ ���ҿ� ������ �ֱ�
void BitmapQueue::PushItem(void* item, std::mutex& mutex) {
	mutex.lock();
	mQueue.push(item);
	mutex.unlock();
}

//Queue�� ù��° ���� ����
void BitmapQueue::PopItem(std::mutex& mutex) {
	mutex.lock();
	mQueue.pop();
	mutex.unlock();
}

//����  Queue�� Item ���� ��ȯ
int BitmapQueue::Size() {
	return (int)mQueue.size();
}



