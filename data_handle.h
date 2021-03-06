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
//#include"easylogging++.h"
#include"worker_thread.h"
#define DECLARE_HANDLE_PROC(cmd)   static void cmd##Handle(void *,void *);
extern std::string FILE_PATH;
struct LogInfo{
	LogInfo(){
		pItem=NULL;
		last_access=time(0);
	}
	std::string worker_path;
	time_t last_access;//最后一次访问时间
	ConnItem * pItem;
	//off_t log_size_recved;
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
	static void PureDataHandle(void * arg);
	static void OverTimeHandle(evutil_socket_t fd, short what, void * arg);//超时操作
private:
	static void WriteDataSize(evbuffer *);
	static  std::set<CmdHandle> cmd_handle_set_;
	//LOGINFO
//	static bool CreateLogInfo(const std::string& log_name,LogInfo& log_info);
	static bool DeleteLogInfo(const std::string& log_name);
	static LogInfo GetLogInfo(const std::string& log_name);
//	static off_t GetLogSize(const std::string& log_name);
//	static void SetLogSize(const std::string& log_name,off_t log_size);
	static std::string SetWorkerPath(zmq::socket_t& sock,LogInfo& log_info);//
	static std::string GetWorkerPath(const LogInfo& log_info);
	static void SetLogConnItem(LogInfo& log_info, ConnItem* pConnItem);
	static void SetLogAccessTime(LogInfo& log_info);
	static void SetLogInfo(const std::string& log_name,const LogInfo& log_info);
//	static bool DeleteWorkerPath(const std::string& log_name);
//	static bool AddWorkerPath(const std::string& log_name,const std::string& worker_path);
	//TODO 写文件要优化
	static int session_id_;//为每次回话分配session_id号
	static std::map<std::string,LogInfo>	log_info_map_;
	static std::mutex  map_mutex_;
};
#endif

