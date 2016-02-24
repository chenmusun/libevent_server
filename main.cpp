/*
 * main.cpp
 *
 *  Created on: 2015年7月17日
 *      Author: chenms
 */
#include"libevent_server.h"
#include"./property_tree/ptree.hpp"
#include "./property_tree/xml_parser.hpp"
#include<stdio.h>
namespace pt = boost::property_tree;//
INITIALIZE_EASYLOGGINGPP//初始化日志记录库

std::string FILE_PATH;

int main(int argc, char* argv[])
{
	el::Configurations conf("./log.conf");
    el::Loggers::reconfigureLogger("default", conf);
    pt::ptree tree;
    pt::read_xml("./libevent_server.xml", tree);

    int port=tree.get("server.port",5678);
    int num_of_threads=tree.get("server.threadnum",2);
    int timespan=tree.get("server.timespan",2);
    int overtime=tree.get("server.overtime",20);
    std::string addr=tree.get<std::string>("server.addr");
    FILE_PATH=tree.get<std::string>("server.path");
    // if (argc > 2) {
    //          port = atoi(argv[1]);
    //          num_of_threads=atoi(argv[2]);
    //  }
    LOG(INFO)<<"LibeventServer will start with "<<num_of_threads<<" threads and listen on port "<<port<<std::endl;
    LibeventServer ls(port,num_of_threads,overtime);//创建服务
    zmq::context_t context;
    if(!ls.Run(context,addr,timespan))
    {
        LOG(ERROR)<<"LibeventServer fails to start service"<<std::endl;
        return -1;
    }
    LOG(INFO)<<"LibeventServer main thread waits for listenning thread"<<std::endl;
    ls.WaitForListenThread();
    LOG(ERROR)<<"LibeventServer exit"<<std::endl;
}



