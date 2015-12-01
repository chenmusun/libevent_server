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
//#define HAVE_CPP0XVARIADICTEMPLATES
#include"nedmalloc.h"
//#include"malloc.c.h"
//#include"data_handle.h"
//增加日志功能
#define ELPP_THREAD_SAFE
#include"easylogging++.h"
#include "zhelpers.hpp"
//#include<boost/pool/singleton_pool.hpp>//内存池
struct ConnItem{
	int session_id;//会话ID
	evutil_socket_t  conn_fd;
	evutil_socket_t  log_fd;
	evbuffer * format_buffer;
	void * pthis;//指向线程对象的指针
	int data_remain_length;//剩余未处理完数据
	int total_packet_length;//总数据包长度
	char triple_des[49];
	unsigned char * data_packet_buffer;//数据包缓冲
	bool recving_log;//是否正在接收日志
	std::string worker_name;//worker路径名
	std::string log_name;//日志名
	void Clear()
	{
		if(log_fd>0){//关闭日志描述符
			close(log_fd);
			log_fd=-1;
		}
	//	delete[] data_packet_buffer;
		nedalloc::nedfree(data_packet_buffer);
		data_packet_buffer=NULL;
		evbuffer_free(format_buffer);//释放format_buffer占用内存
	}
	bool operator<(const ConnItem &ci) const
	{
		return (session_id<ci.session_id);
	}
};


class WorkerThread
{
public:
	typedef  void (*DataHandleProc) (void *,void *)  ;
	WorkerThread(DataHandleProc proc,zmq::context_t& context,const std::string& addr);
	~WorkerThread();
	bool Run();//运行工作线程
	bool AddConnItem(ConnItem& conn_item);//增加连接对象到线程队列中
	bool DeleteConnItem(ConnItem& conn_item);//从线程队列中删除连接对象
	static void HandleConn(evutil_socket_t fd, short what, void * arg);//处理链接回调
	static void ConnReadCb(struct bufferevent *,void *);//buffer读回调
	static void ConnWriteCb(struct bufferevent *,void *);//buffer写回调
	static void ConnEventCB(struct bufferevent *,short int,void *);//出错回调
	//static void ConnTimeoutReadCb(struct bufferevent *,void *);//buffer读超时回调
	 //void SetDataHandleProc(DataHandleProc proc);//设置回调函数
public:
	evutil_socket_t  notfiy_recv_fd_;//工作线程接收端
	evutil_socket_t  notfiy_send_fd_;//监听线程发送端
	std::shared_ptr<std::thread>   shared_ptr_thread_;
private:
	bool CreateNotifyFds();//创建主线程和工作线程通信管道
	bool InitEventHandler();//初始化事件处理器
private:
	struct event  * pnotify_event_; //主线程通知工作线程连接到来事件
	DataHandleProc handle_data_proc_;//数据处理回调函数
public:
	struct event_base * pthread_event_base_;
public:
	std::mutex  conn_mutex_;
	std::list<ConnItem>  list_conn_item_;
	int thread_id_;//用于测试
	static int thread_count_;
	zmq::socket_t requester_;//zmq socket
};
#endif



