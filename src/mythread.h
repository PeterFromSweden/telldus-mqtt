#ifndef MY_THREAD_H_
#define MY_THREAD_H_

struct myThread;
typedef struct myThread MyThread;

//MyThread* MyThread_Create(void);
//void MyThread_Destroy(MyThread* self);
void MyThread_Sleep(int ms);

#endif // !MY_THREAD_H_
