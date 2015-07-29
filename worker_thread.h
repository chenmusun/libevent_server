/*
 * worker_thread.h
 *
 *  Created on: 2015年7月16日
 *      Author: chenms
 */
#ifndef WORKER_THREAD_H_
#define WORKER_THREAD_H_
#include<event2/util.h>
#include<event2/event.h>
#include<event2/bufferevent.h>
#include<thread>
#include<memory>
//#include<queue>
#include<list>
#include<mutex>
#include<unistd.h>
#include<event2/buffer.h>
#include"data_handle.h"

class WorkerThread
{
public:
	WorkerThread();
	~WorkerThread();
	bool Run();//运行工作线程
	bool AddConnItem(ConnItem& conn_item);//增加连接对象到线程队列中
	bool DeleteConnItem(ConnItem& conn_item);//从线程队列中删除连接对象
	static void HandleConn(evutil_socket_t fd, short what, void * arg);//处理链接回调
	static void ConnReadCb(struct bufferevent *,void *);//buffer读回调
	static void ConnWriteCb(struct bufferevent *,void *);//buffer写回调
	static void ConnEventCB(struct bufferevent *,short int,void *);//出错回调
public:
	evutil_socket_t  notfiy_recv_fd_;//工作线程接收端
	evutil_socket_t  notfiy_send_fd_;//监听线程发送端
	std::shared_ptr<std::thread>   shared_ptr_thread_;
private:
	bool CreateNotifyFds();//创建主线程和工作线程通信管道
	bool InitEventHandler();//初始化事件处理器
private:
	struct event  * pnotify_event_; //主线程通知工作线程连接到来事件
public:
	struct event_base * pthread_event_base_;
public:
	std::mutex  conn_mutex_;
	std::list<ConnItem>  list_conn_item_;
	int thread_id_;//用于测试
};
#endif



