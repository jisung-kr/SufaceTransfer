#include "BitmapQueue.h"


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
void BitmapQueue::PushItem(void* item) {
	mQueue.push(item);
}

//Queue�� ù��° ���� ����
void BitmapQueue::PopItem() {
	mQueue.pop();
}

//����  Queue�� Item ���� ��ȯ
int BitmapQueue::Size() {
	return (int)mQueue.size();
}