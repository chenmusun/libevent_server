/*
 * libevent_server.cpp
 *
 *  Created on: 2015年7月16日
 *      Author: chenms
 */
#include "libevent_server.h"
#include <string.h>

int LibeventServer::current_max_session_id_=1;

LibeventServer::LibeventServer(int port,int num_of_threads,int overtime)
{
	last_thread_index_ =-1;
	listen_port_=port;
	num_of_workerthreads_=num_of_threads;
	server_base_=NULL;
    server_conn_listenner_=NULL;
    overtime_event_=NULL;
    overtime_check_base_=NULL;
    overtime_max_=overtime;
}
LibeventServer::~LibeventServer()
{
	if(server_base_!=NULL)
        event_base_free(server_base_);
    if(overtime_check_base_!=NULL)
        event_base_free(overtime_check_base_);
	if(server_conn_listenner_!=NULL)
        evconnlistener_free(server_conn_listenner_);
    if(overtime_event_)
        event_free(overtime_event_);
}

bool LibeventServer::Run(zmq::context_t& context,const std::string& addr,int timespan)
{
	do
	{
        if(!InitWorkerThreads(context,addr))
            break;
        if(!StartOvertimeCheck(timespan))
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
        overtime_check_thread_->join();
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
		item.data_remain_length=0;
//		memset(&item,0,sizeof(ConnItem));
        item.session_id=current_max_session_id_++;//会话ID
		item.conn_fd=sock;
		if(!(item.format_buffer=evbuffer_new())){
			LOG(ERROR)<<"failed to new format_evbuffer for item"<<std::endl;
			return;
		}
		item.pthis=static_cast<void *>((pls->vec_worker_thread_[cur_thread_index]).get());//将线程对象的指针加入ConnItem中
		pls->vec_worker_thread_[cur_thread_index]->AddConnItem(item);
		if(write(pls->vec_worker_thread_[cur_thread_index]->notfiy_send_fd_, "c", 1)!=1){//通知失败
			LOG(ERROR)<<"failed to note the workerthread a new connection comes"<<std::endl;
			pls->vec_worker_thread_[cur_thread_index]->DeleteConnItem(item);
        }
}

bool LibeventServer::InitWorkerThreads(zmq::context_t& context,const std::string& addr)
{
	bool ret=true;
	try{
		//设置回调函数
	//	WorkerThread::SetDataHandleProc(DataHandle::AnalyzeData);
		for(int i=0;i<num_of_workerthreads_;++i)
        {
            std::shared_ptr<WorkerThread>  pwt(new WorkerThread(DataHandle::AnalyzeData,context,addr));
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

bool LibeventServer::StartOvertimeCheck(int timespan)
{
        do{
                try{
                        overtime_check_base_=event_base_new();
                        if(!overtime_check_base_)
                                break;
                        //ADD OVERTIME
                        overtime_event_=event_new(overtime_check_base_,-1,EV_TIMEOUT|EV_PERSIST,DataHandle::OverTimeHandle,&overtime_max_);
                        if(!overtime_event_)
                                break;
                        timeval tv={timespan,0};
                        if(event_add(overtime_event_,&tv)==-1)
                                break;
                        overtime_check_thread_.reset(new std::thread([this]
                           {
                                   event_base_dispatch(overtime_check_base_);
                           }
                        ));
                   }
                catch(...){
                        break;
                }
                LOG(INFO)<<"success start the overtime check thread";
                return true;
        }while (0);
        LOG(ERROR)<<"failed start the overtime check thread";
        return false;
 }
