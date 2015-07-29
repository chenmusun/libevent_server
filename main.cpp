/*
 * main.cpp
 *
 *  Created on: 2015年7月17日
 *      Author: chenms
 */
#include"libevent_server.h"
#include<stdio.h>
int main(int argc, char* argv[])
{
	int port=5678;
	int num_of_threads=2;
    if (argc > 2) {
             port = atoi(argv[1]);
             num_of_threads=atoi(argv[2]);
     }
    LibeventServer ls(port,num_of_threads);//创建服务
    if(!ls.Run())
    {
    	perror("创建服务失败！\n");
    	return -1;
    }
    ls.WaitForListenThread();
}



