/*
 * main.cpp
 *
 *  Created on: 2015年7月17日
 *      Author: chenms
 */
#include"libevent_server.h"
#include<stdio.h>
INITIALIZE_EASYLOGGINGPP//初始化日志记录库
int main(int argc, char* argv[])
{
	el::Configurations conf("./log.conf");
	el::Loggers::reconfigureLogger("default", conf);
	int port=5678;
	int num_of_threads=2;
    if (argc > 2) {
             port = atoi(argv[1]);
             num_of_threads=atoi(argv[2]);
     }
    LOG(INFO)<<"LibeventServer will start with "<<num_of_threads<<" threads and listen on port "<<port<<std::endl;
    LibeventServer ls(port,num_of_threads);//创建服务
    if(!ls.Run())
    {
        LOG(ERROR)<<"LibeventServer fails to start service"<<std::endl;
        return -1;
    }
    LOG(INFO)<<"LibeventServer main thread waits for listenning thread"<<std::endl;
    ls.WaitForListenThread();
}



