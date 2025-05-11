#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// 状态量实现的生产者消费者模型

void err_thread(int ret, char* str) {
    if (ret != 0) {
        fprintf(stderr, "%s: %s\n", str, strerror(ret));
        pthread_exit(NULL);
    }
}

struct msg {
    int num;
    struct msg* next;
};

struct msg* head = NULL;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t has_data = PTHREAD_COND_INITIALIZER;

void* producer(void* arg) {
    struct msg* new_msg = (struct msg*)malloc(sizeof(struct msg));
    if (new_msg == NULL) {
        perror("malloc");
        return NULL;
    }
    new_msg->num = rand() % 100; // 生产随机数
    pthread_mutex_lock(&mutex);
    head = new_msg; // 将新消息添加到链表头部
    new_msg->next = head; // 即写入公共区域
    pthread_mutex_unlock(&mutex); // 随即解锁

    pthread_cond_signal(&has_data); // 通知消费者有数据可用

    sleep(rand() % 3);

    return NULL;
}

void* consumer(void* arg) {
    pthread_mutex_lock(&mutex);
    while (head == NULL) { // 如果是单个消费者可以用if，但多个消费者用if就会逻辑错误
        pthread_cond_wait(&has_data, &mutex); // 等待生产者生产数据、解锁
    }// 生产者生产数据后会自动唤醒消费者、重新加锁
    
    struct msg* food = head;
    head = food->next; // 消费数据
    free(food); // 释放内存

    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3);

    return NULL;
}

int main() {
    int ret;
    srand(time(NULL)); // 初始化随机数种子
    pthread_t pid, cid;
    ret = pthread_create(&pid, NULL, producer, NULL); // 生产者
    if (ret != 0) {
        err_thread(ret, "pthread_create producer eror");
    }
    ret = pthread_create(&cid, NULL, consumer, NULL); // 消费者
    if (ret != 0) {
        err_thread(ret, "pthread_create comsumer eror");
    }



    pthread_join(pid, NULL); // 等待生产者线程结束
    pthread_join(cid, NULL); // 等待消费者线程结束
    return 0;
}