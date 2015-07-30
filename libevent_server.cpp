/*
 * libevent_server.cpp
 *
 *  Created on: 2015年7月16日
 *      Author: chenms
 */
#include "libevent_server.h"
#include <string.h>

int LibeventServer::current_max_session_id_=1;

LibeventServer::LibeventServer(int port,int num_of_threads)
{
	last_thread_index_ =-1;
	listen_port_=port;
	num_of_workerthreads_=num_of_threads;
	server_base_=NULL;
	server_conn_listenner_=NULL;
}
LibeventServer::~LibeventServer()
{
	if(server_base_!=NULL)
		event_base_free(server_base_);
	if(server_conn_listenner_!=NULL)
		evconnlistener_free(server_conn_listenner_);
}

bool LibeventServer::Run()
{
	do
	{
		if(!InitWorkerThreads())
			break;
		if(!StartListen())
			break;
		return true;
	}while(0);
	return false;
}

void LibeventServer::WaitForListenThread()
{
	main_listen_thread_->join();
}

void LibeventServer::AcceptError(evconnlistener *listener, void *ptr)
{
	//TODO
	//LibeventServer * pls=static_cast<LibeventServer *>(ptr);
	LOG(ERROR)<<"Main Listen Thread  AcceptError\n";
}

void LibeventServer::AcceptConn(evconnlistener * listener, int sock, sockaddr * addr, int len, void *ptr)
{
		LOG(INFO)<<"accept a connection"<<std::endl;
		LibeventServer * pls=static_cast<LibeventServer *>(ptr);
		int cur_thread_index = (pls->last_thread_index_ + 1) %pls->num_of_workerthreads_; // 轮循选择工作线程
		pls->last_thread_index_ = cur_thread_index;
		ConnItem item;
		memset(&item,0,sizeof(ConnItem));
		item.session_id=current_max_session_id_++;//会话ID
		item.conn_fd=sock;
		item.pthis=static_cast<void *>((pls->vec_worker_thread_[cur_thread_index]).get());//将线程对象的指针加入ConnItem中
		pls->vec_worker_thread_[cur_thread_index]->AddConnItem(item);
		if(write(pls->vec_worker_thread_[cur_thread_index]->notfiy_send_fd_, "c", 1)!=1){//通知失败
			LOG(ERROR)<<"failed to note the workerthread a new connection comes"<<std::endl;
			pls->vec_worker_thread_[cur_thread_index]->DeleteConnItem(item);
		}
}

bool LibeventServer::InitWorkerThreads()
{
	bool ret=true;
	try{
		for(int i=0;i<num_of_workerthreads_;++i)
		{
			std::shared_ptr<WorkerThread>  pwt(new WorkerThread);
			if(!pwt->Run())
			{
				ret=false;
				break;
			}
			vec_worker_thread_.push_back(pwt);
		}
	}catch(...)
	{
		ret=false;
	}
	//LOG(ERROR)<<“Initialize workerthreads failed\n";
    LOG_IF(ret==false,ERROR)<<"Initialize workerthreads failed"<<std::endl;
	return ret;
}

bool LibeventServer::StartListen()
{
	do{
		     struct sockaddr_in sin;
		    server_base_ = event_base_new();
		     if (!server_base_)
		    	 break;
		     memset(&sin, 0, sizeof(sin));
		     sin.sin_family = AF_INET;
		     sin.sin_addr.s_addr = htonl(0);
		     sin.sin_port = htons(listen_port_);

		     server_conn_listenner_ = evconnlistener_new_bind(server_base_, AcceptConn, this,
		    		 LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
		         (struct sockaddr*)&sin, sizeof(sin));
		     if (! server_conn_listenner_ )
		    	 break;
		     evconnlistener_set_error_cb(server_conn_listenner_, AcceptError);

		     try{
		     main_listen_thread_.reset(new std::thread([this]
						{
		    	 	 	 	 event_base_dispatch(server_base_);
						}
			));
		     }catch(...){
		    	 break;
		     }
		     LOG(INFO)<<"Listen on the port "<<listen_port_<<" success"<<std::endl;
		     return true;
	}while(0);
    LOG(ERROR)<<"Listen on the port "<<listen_port_<<" failed"<<std::endl;
	return false;
}
