/*
 * worker_thread.cpp
 *
 *  Created on: 2015年7月16日
 *      Author: chenms
 */
#include "worker_thread.h"
#include<fcntl.h>
#include<unistd.h>
int WorkerThread::thread_count_=1;



WorkerThread::WorkerThread(DataHandleProc proc)
{
	pthread_event_base_=NULL;
	pnotify_event_=NULL;
	notfiy_recv_fd_=-1;
	notfiy_send_fd_=-1;
	thread_id_=thread_count_++;
	handle_data_proc_=proc;
}

WorkerThread::~WorkerThread()
{
	if(notfiy_recv_fd_!=-1)
	{
		close(notfiy_recv_fd_);
		close(notfiy_send_fd_);
	}
	if(pthread_event_base_!=NULL)
		event_base_free(pthread_event_base_);
	if(pnotify_event_!=NULL)
		event_free(pnotify_event_);
}

bool WorkerThread::Run()
{
	do{
		if(!CreateNotifyFds())
			break;
		if(!InitEventHandler())
			break;
		try{
		shared_ptr_thread_.reset(new std::thread([this]
			{event_base_loop(pthread_event_base_, 0);}
		));
		}catch(...)
		{
			break;
		}
		LOG(TRACE)<<"workerhread "<<thread_id_<<" run success";
		return true;
	}while(0);
	LOG(ERROR)<<"workerhread "<<thread_id_<<"run failed";
	return false;
}
bool WorkerThread::CreateNotifyFds()
{
	 int fds[2];
	 bool ret=false;
	 if (!pipe(fds/*, O_NONBLOCK*/))
	 {
		  notfiy_recv_fd_= fds[0];
		  notfiy_send_fd_ = fds[1];
		  ret=true;
	 }
	 LOG_IF(ret==false,ERROR)<<"workerthread "<<thread_id_<<" create pipes failed\n";
	  return ret;
}

bool WorkerThread::AddConnItem(ConnItem& conn_item)
{
		try
		{
			std::lock_guard<std::mutex>  lock(conn_mutex_);
			list_conn_item_.push_back(conn_item);
		}
		catch(...)
		{
			LOG(ERROR)<<"workerthread "<<thread_id_<<"  add connection item failed\n";
			return false;
		}
		return true;
}

bool WorkerThread::DeleteConnItem(ConnItem& conn_item)
{
		try
		{//list_conn_item_是按session_id从小到大排序的
			std::lock_guard<std::mutex>  lock(conn_mutex_);
			auto pos=std::lower_bound(list_conn_item_.begin(),list_conn_item_.end(),conn_item);
			if((pos!=list_conn_item_.end())&&(conn_item.session_id==pos->session_id)){
				pos->Clear();//善后处理
				list_conn_item_.erase(pos);
				LOG(TRACE)<<list_conn_item_.size()<<std::endl;
			}
		}
		catch(...)
		{
			LOG(ERROR)<<"workerthread "<<thread_id_<<"  delete connection item failed\n";
			return false;
		}
		return true;
}



bool WorkerThread::InitEventHandler()
{
		do
		{
			pthread_event_base_=event_base_new();
			if(pthread_event_base_==NULL)
				break;
			pnotify_event_=event_new(pthread_event_base_,notfiy_recv_fd_,EV_READ | EV_PERSIST,HandleConn,(void *)this);
			if(pnotify_event_==NULL)
				break;
			if(event_add(pnotify_event_, 0))
				break;
			LOG(TRACE)<<"workerthread "<<thread_id_<<"  initialize event handler success\n";
			return true;
		}while(0);
		LOG(ERROR)<<"workerthread "<<thread_id_<<"  initialize event handler failed\n";
		return false;
}

void WorkerThread::HandleConn(evutil_socket_t fd, short what, void * arg)
{
	//当连接请求到来时
	 WorkerThread * pwt=static_cast<WorkerThread *>(arg);
	 char  buf[1];
	 if(read(fd, buf, 1)!=1)//从sockpair的另一端读数据
		 LOG(ERROR)<<"workerthread "<<pwt->thread_id_<<"accept connection failed\n";
	std::lock_guard<std::mutex>  lock(pwt->conn_mutex_);
	 ConnItem *pitem=&pwt->list_conn_item_.back();//取出在容器中的位置
	//pwt->queue_conn_item_.pop();
	struct bufferevent * bev=bufferevent_socket_new(pwt->pthread_event_base_,pitem->conn_fd,BEV_OPT_CLOSE_ON_FREE);
	if(bev==NULL)
		return;
    bufferevent_setcb(bev, ConnReadCb, NULL/*ConnWriteCb*/, ConnEventCB,pitem/*arg*/);
    bufferevent_enable(bev, EV_READ /*| EV_WRITE*/ );
    LOG(INFO)<<"workerthread "<<pwt->thread_id_<<"accept connection success\n";
}

void WorkerThread::ConnReadCb(bufferevent * bev,void *ctx)
{
//TODO
	ConnItem * pitem=static_cast<ConnItem *>(ctx);
	WorkerThread * pwt=static_cast<WorkerThread *>(pitem->pthis);
	LOG(TRACE)<<"workerthread "<<pwt->thread_id_<<"accept datas from session "<<pitem->session_id<<std::endl;
	//DataHandle::AnalyzeData(bev,ctx);
	pwt->handle_data_proc_(bev,ctx);
}
void WorkerThread::ConnWriteCb(bufferevent *bev,void * ctx)
{
	//TODO
	ConnItem * pitem=static_cast<ConnItem *>(ctx);
}
void WorkerThread::ConnEventCB(bufferevent *bev,short int  events,void * ctx)
{
	ConnItem * pitem=static_cast<ConnItem *>(ctx);
	WorkerThread * pwt=static_cast<WorkerThread*>(pitem->pthis);
	LOG(ERROR)<<"session "<<pitem->session_id<<"has encounted an error\n";
	pwt->DeleteConnItem(*pitem);//从线程队列中删除连接对象
	bufferevent_free(bev);
}

