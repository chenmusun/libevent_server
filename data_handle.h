/*
 * data_handle.h
 *
 *  Created on: 2015年7月22日
 *      Author: chenms
 */
#ifndef DATA_HANDLE_H_
#define DATA_HANDLE_H_
#include<set>
#include<event2/bufferevent.h>
#include<event2/buffer.h>
#include<string.h>
//TODO
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
 #include <sys/stat.h>
//增加日志功能
#include"easylogging++.h"
#define DECLARE_HANDLE_PROC(cmd)   static void cmd##Handle(void *,void *);

struct ConnItem{
	int session_id;//会话ID
	evutil_socket_t  conn_fd;
	evutil_socket_t  log_fd;
	void * pthis;//指向线程对象的指针
	int data_remain_length;//剩余未处理完数据
	bool operator<(const ConnItem &ci) const
	{
		return (session_id<ci.session_id);
	}
};


struct CmdHandle{
			const char * command_name_;
			void (*handle_proc_)(void *,void *);
			bool operator<(const CmdHandle &ch) const
			{
				return (strcmp(command_name_,ch.command_name_)<0);
			}
			bool operator==(const CmdHandle &ch) const
			{
				return !strcmp(command_name_,ch.command_name_);
			}
};
class DataHandle{
public:
	static void AnalyzeData(void *arg,void *);
	DECLARE_HANDLE_PROC(Login)
	DECLARE_HANDLE_PROC(Logout)
	DECLARE_HANDLE_PROC(Sync)
	DECLARE_HANDLE_PROC(Config)
	DECLARE_HANDLE_PROC(Status)
	DECLARE_HANDLE_PROC(Upload)
	DECLARE_HANDLE_PROC(Eof)
	DECLARE_HANDLE_PROC(PureData)//纯数据包操作
private:
	static void WriteDataSize(evbuffer *);
	static  std::set<CmdHandle> cmd_handle_set_;
	//TODO 写文件要优化
	static int session_id_;//为每次回话分配session_id号
};
#endif

