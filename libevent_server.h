/*
 * libevent_server.h
 *
 *  Created on: 2015年7月16日
 *      Author: chenms
 */
#ifndef LIBEVENT_SERVER_H_
#define LIBEVENT_SERVER_H_
//#include "worker_thread.h"
#include "data_handle.h"
#include<event2/listener.h>
#include<vector>
class LibeventServer{
public:
	LibeventServer(int port,int num_of_threads,int overtime);
	~LibeventServer();
	bool Run(zmq::context_t& context,const std::string& addr,int timespan);//开启服务
	void WaitForListenThread();
	static  void AcceptConn(evconnlistener *, int, sockaddr *, int, void *);
	static void AcceptError(evconnlistener *, void *);
private:
	bool StartListen();
	bool StartOvertimeCheck(int timespan);//启动超时检查线程
	bool InitWorkerThreads(zmq::context_t& context,const std::string& addr);
	std::vector<std::shared_ptr<WorkerThread> >  vec_worker_thread_;
	std::shared_ptr<std::thread> main_listen_thread_;
	event_base * server_base_;
	evconnlistener *server_conn_listenner_;
	struct event  * overtime_event_;
	event_base * overtime_check_base_;
	std::shared_ptr<std::thread> overtime_check_thread_;
	int listen_port_;//监听端口号
	int last_thread_index_;//上一次请求分配的线程索引
	int num_of_workerthreads_;//工作线程总数
	static int current_max_session_id_;//当前最大session_id
	int overtime_max_;
};
#endif


