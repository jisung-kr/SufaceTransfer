#include "BitmapQueue.h"


//생성자
BitmapQueue::BitmapQueue() {


}

//파괴자
BitmapQueue::~BitmapQueue() {

}



//Queue의 첫번째 원소 반환
void* BitmapQueue::FrontItem() {
	return mQueue.front();
}

//Queue의 마지막 원소에 데이터 넣기
void BitmapQueue::PushItem(void* item) {
	mQueue.push(item);
}

//Queue의 첫번째 원소 삭제
void BitmapQueue::PopItem() {
	mQueue.pop();
}

//현재  Queue의 Item 갯수 반환
int BitmapQueue::Size() {
	return (int)mQueue.size();
}