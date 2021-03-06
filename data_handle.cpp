/*
 * data_handle.cpp
 *
 *  Created on: 2015年7月22日
 *      Author: chenms
 */
#include"data_handle.h"
#include"zlib.h"
#include<nettle/des.h>
#include<uuid/uuid.h>
//add zmq

const std::string HAS_NO_WORKER="HASNOWORKER";
const std::string CLIENT_REQUEST="CLIENTREQUEST";
const std::string WORKER_REQUEST="WORKERREQUEST";
const std::string BROKER_REPLY="BROKERREPLY";
const std::string WORKER_CHANGED="WORKERCHANGED";

CmdHandle  cmd_name[]={
		{"Command=Login",DataHandle::LoginHandle},
		{"Command=Logout",DataHandle::LogoutHandle},
		{"Command=Sync Time",DataHandle::SyncHandle},
		{"Command=Config",DataHandle::ConfigHandle},
		{"Command=Status",DataHandle::StatusHandle},
		{"Command=Upload",DataHandle::UploadHandle},
	//	{"\r\n\r\n",DataHandle::UploadHandle},//纯数据包的处理
		{"Command=Eof",DataHandle::EofHandle},
};

static void generate_triple_des(char (&arr)[49],char (&arr_send)[33])
{
	  char str[36];
	  uuid_t uuid;
	  uuid_generate(uuid);
	  uuid_unparse(uuid,str);
	  for(int i=0,j=0;i<36;++i)
	    if(str[i]!='-'){
		if(str[i]>='a'){
	   	   arr[j]=str[i]-32;
	   	   arr_send[j]=str[i]-32;
		}
		else{
		   arr[j]=str[i];
	         arr_send[j]=str[i];
		}

	      j++;
	    }
	  memcpy(arr+32,arr,16);
	  arr[48]=0;
	  arr_send[32]=0;
}

static void ConvertToByte(char (&arr)[49],unsigned char (&arr_key)[24])
{
	for(int i=0;i<24;++i){
		//int j=2*i;
		for(int j=0;j<2;++j){
			char c=arr[2*i+j];
			if(c>=97){
				arr_key[i]+=c-97+10;
			}
			else if(c>=65){
				arr_key[i]+=c-65+10;
			}
			else{
				arr_key[i]+=c-48;
			}

			if(j==0)
				arr_key[i]*=16;
		}
	}
}

static void Decrypt3DES(const uint8_t * src,size_t length,uint8_t * dest,char (&password)[49])
{
	//准备好24字节的秘钥
	unsigned char key[24]={0};
	ConvertToByte(password,key);
	//设置key
	struct des3_ctx des3={0};
	des3_set_key(&des3,key);
	//进行解密
	des3_decrypt(&des3,length,dest,src);
}

bool DecryptDecompressData(const uint8_t * src,int& length,uint8_t * dest,uLongf& destLength,char (&password)[49])
{		//const int len=length;
		bool ret=false;
		do{
			if(length%8){
				LOG(ERROR)<<"length%8 error";		
				break;
			}
			uint8_t decrypted_dest[65535]={0};
			Decrypt3DES(src,length,decrypted_dest,password);

			int status=Z_OK;
			if((status=uncompress(dest,&destLength,decrypted_dest,length))==Z_OK){
				length=destLength;
				ret=true;
			}
			else{
				LOG(ERROR)<<"uncompress error "<<status<<" and destLength is "<<destLength<<" and length is "<<length;			
			}
		}while(0);
		return ret;
}



int DataHandle::session_id_=1;
std::mutex DataHandle::map_mutex_;
std::map<std::string,LogInfo>	DataHandle::log_info_map_;
std::set<CmdHandle> DataHandle::cmd_handle_set_(cmd_name,cmd_name+sizeof(cmd_name)/sizeof(CmdHandle));

void DataHandle::AnalyzeData(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	struct evbuffer * input=bufferevent_get_input(buffer);//得到ioevbuffer
//	struct evbuffer * output=bufferevent_get_output(buffer);//得到ioevbuffer
	//TODO
//	unsigned char tmp[65535]={0};
//	int num=evbuffer_copyout(input,tmp,65535);
	//TODO
	unsigned char lengthch[2]={0};
	int length=0;
	char * cmd_type=NULL;
	char * cmd_name=NULL;

	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	do{
		//上次的数据包未处理完，直接进入函数处理
		if(pitem->data_remain_length){
			LOG(TRACE)<<"continue accepting data packet from session "<<pitem->session_id<<" length remains "<<pitem->data_remain_length<<std::endl;
			//TODO
			//pitem->data_packet_buffer=new char[pitem->total_packet_length];
			int data_occupied=pitem->total_packet_length-pitem->data_remain_length;
			int nread=evbuffer_remove(input,pitem->data_packet_buffer+data_occupied,pitem->data_remain_length);
			pitem->data_remain_length-=nread;
			if(!pitem->data_remain_length)
				PureDataHandle(pitem);
			break;
		}
//		//TODO
//
//		int testfd=open("./loginresponse",O_WRONLY|O_APPEND|O_CREAT,S_IWUSR|S_IRUSR);
//		int nwrite=evbuffer_write(input,testfd);
//		close(testfd);
//		return;
//		//TODO
		if(evbuffer_remove(input,lengthch,2)==2)
		{
			length=lengthch[0]*256+lengthch[1];
		}

		if(!length){
			//读包总长度失败，清空缓存中所有数据
			length=65535;
			break;
		}

		size_t n_read_out=0;
		cmd_type=evbuffer_readln(input,&n_read_out,EVBUFFER_EOL_CRLF_STRICT);
		if(!n_read_out)//非指令包
		{
			if(evbuffer_drain(input,2)!=-1)//当成是数据包，移除\r\n
			{
				char data_head[35]={0};
				evbuffer_remove(input,data_head,35);
				pitem->total_packet_length=length-35-4;
				pitem->data_remain_length=length-35-4;
				length=0;
			//	pitem->data_packet_buffer=new unsigned char[pitem->total_packet_length];//new unsigned char[pitem->total_packet_length];
				pitem->data_packet_buffer=(unsigned char *)nedalloc::nedmalloc(pitem->total_packet_length);
				int nread=evbuffer_remove(input,pitem->data_packet_buffer,pitem->total_packet_length);
				pitem->data_remain_length-=nread;
			//	LOG(TRACE)<<"accept pure data packet from session "<<pitem->session_id<<" length "<<pitem->data_remain_length<<std::endl;
			//	PureDataHandle(arg,arg2);
				if(!pitem->data_remain_length)//完整接收数据包，进一步处理
					PureDataHandle(pitem);
			}
			break;
		}

		CmdHandle cmd_handle_temp;
		if(strncmp("[Request]",cmd_type,9))//不带[Request]的指令包
			cmd_handle_temp.command_name_=cmd_type;
		else{
			//带[Request]指令包的处理
			cmd_name=evbuffer_readln(input,&n_read_out,EVBUFFER_EOL_CRLF_STRICT);
			if(!n_read_out)//非正确的指令包格式
				break;
			cmd_handle_temp.command_name_=cmd_name;
		}

		auto pos=cmd_handle_set_.find(cmd_handle_temp);
		if(pos!=cmd_handle_set_.end()){
			LOG(TRACE)<<"accept command packet from session "<<pitem->session_id<<" command "<<cmd_handle_temp.command_name_<<std::endl;
			pos->handle_proc_(buffer,arg2);
		}
	}while(0);
	evbuffer_drain(input,length);//释放缓冲区中未处理的属于同一数据包的数据仅对指令而言，数据部分要全部写入文件
	free(cmd_name);//在此释放cmd_name
	free(cmd_type);
}

void DataHandle::WriteDataSize(evbuffer *buffer){
	size_t leng=evbuffer_get_length(buffer);//获取输出缓冲区的字节数
	char lengthch[2]={0};
	lengthch[0]=leng/256;
	lengthch[1]=leng%256;
	evbuffer_prepend(buffer,lengthch,2);
	//TODO
//	char buff[100]={0};
//	evbuffer_copyout(buffer,buff,100);
//	int i=0;
}

void DataHandle::LoginHandle(void *arg,void *arg2)
{
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	//WorkerThread * pwt=static_cast<WorkerThread*>(pitem->pthis);

	bufferevent * buffer=static_cast<bufferevent *>(arg);
//TODO
//		int testfd=open("./loginresponse",O_WRONLY|O_APPEND|O_CREAT,S_IWUSR|S_IRUSR);
//		int nwrite=evbuffer_write(input,testfd);
//		close(testfd);
//		return;
//TODO
	evbuffer * out=bufferevent_get_output(buffer);

	//evbuffer * outtmp=evbuffer_new();
	//int length=evbuffer_get_length(out);
	evbuffer_add_printf(pitem->format_buffer,"[Response]\r\n");
	evbuffer_add_printf(pitem->format_buffer,"Command=Login\r\n");
	evbuffer_add_printf(pitem->format_buffer,"Session=%d\r\n",pitem->session_id);
	evbuffer_add_printf(pitem->format_buffer,"Result=AC\r\n");//TODO 需要做逻辑判断
	evbuffer_add_printf(pitem->format_buffer,"Code=0x00\r\n");
	evbuffer_add_printf(pitem->format_buffer,"SverU=N\r\n");
	evbuffer_add_printf(pitem->format_buffer,"CverU=N\r\n");
	evbuffer_add_printf(pitem->format_buffer,"Adc=Y\r\n\r\n");
	WriteDataSize(pitem->format_buffer);

	if(evbuffer_add_buffer(out,pitem->format_buffer))
		LOG(ERROR)<<"evbuffer_add_buffer failed\n";
//	evbuffer_free(outtmp);//释放临时evbuffer
}

void DataHandle::LogoutHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	//pitem->Clear();
	WorkerThread * pwt=static_cast<WorkerThread*>(pitem->pthis);
	pwt->DeleteConnItem(*pitem);//从线程队列中删除连接对象
	bufferevent_free(buffer);
}


void DataHandle::SyncHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	evbuffer * in=bufferevent_get_input(buffer);
	char * session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	if(session_id){
		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(pitem->format_buffer,"Command=Sync Time\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"Session=%s\r\n ",session_id);
		evbuffer_add_printf(pitem->format_buffer,"URL=172.30.4.125\r\n ");//TODO
		free(session_id);
	}
}

void DataHandle::ConfigHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	//TODO
	evbuffer * in=bufferevent_get_input(buffer);
	char * session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
	if(session_id){
		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(pitem->format_buffer,"Command=Config\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"Session=%s\r\n ",session_id);
		evbuffer_add_printf(pitem->format_buffer,"Cver=config version\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"Result=AC\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"Code=A Code\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"PacketCount=N\r\n ");
		evbuffer_add_printf(pitem->format_buffer,"PacketNo=n\r\n ");
		free(session_id);
	}
}

void DataHandle::StatusHandle(void *arg,void *arg2)
{
	printf("StatusHandle\n");
}

void DataHandle::UploadHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	evbuffer * in=bufferevent_get_input(buffer);
//TODO..............................
//		int testfd=open("./Uploadresponse",O_WRONLY|O_APPEND|O_CREAT,S_IWUSR|S_IRUSR);
//		int nwrite=evbuffer_write(in,testfd);
//		close(testfd);
//		return;
//TODO..............................

	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	char * session_id=NULL;
	char * file_name=NULL;
	char * new_file=NULL;
	do{
		session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
		if(!session_id)
			break;
		file_name=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
		if(!file_name)
			break;
	//	new_file=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
	//	new_file=evbuffer_readln(in,NULL,EVBUFFER_EOL_ANY);
	//	if(!new_file)
	//		break;

		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(pitem->format_buffer,"[Response]\r\n");
		evbuffer_add_printf(pitem->format_buffer,"Command=Upload\r\n");
		evbuffer_add_printf(pitem->format_buffer,"%s\r\n",session_id);
		evbuffer_add_printf(pitem->format_buffer,"%s\r\n",file_name);

		evbuffer_add_printf(pitem->format_buffer,"Result=AC\r\n");
		pitem->recving_log=true;//接收日志中
		pitem->log_name=file_name+9;

		//evbuffer_add_printf(out,"Code=A Code\r\n ");
		//获取文件大小
//		 struct stat buf;
//		 char path[256]={0};
//		 sprintf(path,"./%s",file_name);
//		 if(stat(path, &buf)==-1)
//			 buf.st_size=0;
		//创建或打开文件
		char path[256]={0};
		char path_file[256]={0};
		sprintf(path,"%s",FILE_PATH.c_str());
		sprintf(path_file,"%s/%s",path,file_name+9);
		if(access(path,F_OK)==-1){
			if(mkdir(path,S_IRWXU|S_IRWXG|S_IRWXO)==-1)
				LOG(ERROR)<<"mkdir "<<path<<" failed";
		}
		int fd=open(path_file,O_WRONLY|O_APPEND|O_CREAT,S_IWUSR|S_IRUSR);
		if(fd==-1)
		{
			LOG(ERROR)<<"open file "<<file_name+9<<" failed";
			break;
		}
		pitem->log_fd=fd;
		//获取文件大小
		struct stat buf;
		if(stat(path, &buf)==-1)//TODO 
			 buf.st_size=0;
//		off_t log_size=GetLogSize(file_name+9);
//		if(log_size==-1){//第一次获取
//			CreateLogInfo(file_name+9);
//			log_size=0;
//		}
		evbuffer_add_printf(pitem->format_buffer,"Size=%d\r\n",(int)buf.st_size);

		//add zmq
		LogInfo loginfo=GetLogInfo(pitem->log_name);
		SetLogConnItem(loginfo,pitem);//将连接信息加入LogInfo中
		if((pitem->worker_name=loginfo.worker_path).empty()){//设置路径信息
			WorkerThread * pwt=static_cast<WorkerThread *>(pitem->pthis);
			pitem->worker_name=SetWorkerPath(pwt->requester_,loginfo);
		}
		SetLogInfo(pitem->log_name,loginfo);//回写到map中
		char arr_send[33];
		generate_triple_des(pitem->triple_des,arr_send);
		evbuffer_add_printf(pitem->format_buffer,"TriDES=%s\r\n",arr_send);
		WriteDataSize(pitem->format_buffer);

		if(evbuffer_add_buffer(out,pitem->format_buffer))
			LOG(ERROR)<<"evbuffer_add_buffer failed\n";
	}while(0);

	free(session_id);
	free(file_name);
	//free(new_file);
}

void DataHandle::EofHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	evbuffer * in=bufferevent_get_input(buffer);
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	pitem->recving_log=false;//接收日志结束
	if(pitem->log_fd>0){
		close(pitem->log_fd);
		pitem->log_fd=-1;
	}
//	TODO返回Eof的响应
	char * session_id=NULL;
	char * file_name=NULL;
	do{
		session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
		if(!session_id)
			break;
		file_name=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
		if(!file_name)
			break;
		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(pitem->format_buffer,"[Response]\r\n");
		evbuffer_add_printf(pitem->format_buffer,"Command=Eof\r\n");
		evbuffer_add_printf(pitem->format_buffer,"%s\r\n",session_id);
		evbuffer_add_printf(pitem->format_buffer,"%s\r\n",file_name+6);
		evbuffer_add_printf(pitem->format_buffer,"Result=AC\r\n");
		WriteDataSize(pitem->format_buffer);
		if(evbuffer_add_buffer(out,pitem->format_buffer))
			LOG(ERROR)<<"evbuffer_add_buffer failed\n";
	}while(0);


	if(DeleteLogInfo(file_name+6))//日志接收完成,删除对应信息
	{
			//发送EOF标识

	//	LOG(ERROR)<<"Delete Log Info successs";
		LOG(ERROR)<<file_name+6<<" removed caused by eof";
		if(!pitem->worker_name.empty()){
			WorkerThread * pwt=static_cast<WorkerThread *>(pitem->pthis);
			// pitem->worker_name=SetWorkerPath(pwt->requester_,file_name);
			if(s_sendmore(pwt->requester_,pitem->worker_name))
			{
				s_sendmore(pwt->requester_,pitem->log_name);
				s_send(pwt->requester_,"EOF");
			}
		}
	}
	free(session_id);
	free(file_name);
}

void DataHandle::PureDataHandle(void * arg)
{
//	bufferevent * buffer=static_cast<bufferevent *>(arg);
	ConnItem * pitem=static_cast<ConnItem *>(arg);
//	evbuffer * in=bufferevent_get_input(buffer);
//	int length=0;
	if(pitem->log_fd>0){
		//unsigned char encrypted_data[65535]={0};
		unsigned char decrypted_data[65535*10]={0};
		uLongf decryptedLenth=65535*10;
//		length=evbuffer_remove(in,encrypted_data,65535);
//		pitem->data_remain_length-=length;
		int length=pitem->total_packet_length;
		if(DecryptDecompressData(pitem->data_packet_buffer,length,decrypted_data,decryptedLenth,pitem->triple_des)){
			//SetLogSize(pitem->log_name,length);
			write(pitem->log_fd,decrypted_data,length);
			LogInfo loginfo=GetLogInfo(pitem->log_name);//设置访问时间信息
			SetLogAccessTime(loginfo);
			//pitem->log_size_recved+=length;
			//SetLogSize(pitem->log_name,length);//记录已经获取的文件大小
			WorkerThread * pwt=static_cast<WorkerThread *>(pitem->pthis);
			//if(pitem->worker_name.empty()){//获取路径
				// pitem->worker_name=SetWorkerPath(pwt->requester_,loginfo);
			 //   std::string worker=s_recv(pwt->requester_);
			//	if(worker==HAS_NO_WORKER){
			//		LOG(ERROR)<<"the broker HAS_NO_WORKER";
			//	}
			//	else if(worker.empty())
			//		LOG(ERROR)<<"the broker is not avaliable";
			//	else {
			//		pitem->worker_name=worker;
			//	}
			//}
			SetLogInfo(pitem->log_name,loginfo);//回写到map中
			if(!pitem->worker_name.empty()){//此处已经假定broker已经存在
				// pitem->worker_name=SetWorkerPath(pwt->requester_,file_name);
				if(s_sendmore(pwt->requester_,pitem->worker_name))
				{
					s_sendmore(pwt->requester_,pitem->log_name);
					s_send(pwt->requester_,(char *)decrypted_data);
					std::string rep=s_recv(pwt->requester_);
					if(rep==HAS_NO_WORKER)
						LOG(ERROR)<<"after send data got HAS_NO_WORKER";
					else if(rep==WORKER_CHANGED){
						LOG(ERROR)<<"after send data got WORKER_CHANGED";
						//GET THE NEW WORKER PATH
						std::string rep=s_recv(pwt->requester_);
						pitem->worker_name=rep;
					}
					else if(rep.empty())
						LOG(ERROR)<<"after send data got null string";
					else if(rep!=BROKER_REPLY){
						LOG(ERROR)<<"after send data got unexpected return ,rep is "<<rep;
					}

				}
				else {
					LOG(ERROR)<<"send data failed";
				}
			}
			else {
					LOG(ERROR)<<"has no worker path,can't send data to remote";
				// SetWorkerPath(pwt->requester_,loginfo);
			}
		}
		else{
			LOG(ERROR)<<"DecryptDecompressData error";
		}

	//	delete[] pitem->data_packet_buffer;//回收内存，此处可以使用内存池进行优化
		if(pitem->data_packet_buffer){
			nedalloc::nedfree(pitem->data_packet_buffer);
			pitem->data_packet_buffer=NULL;
		}

//		length=evbuffer_write_atmost(in,pitem->log_fd,pitem->data_remain_length);//写数据
	}

//	if(pitem->data_remain_length<4096)
//		int i=0;
//	if(pitem->data_remain_length)
}

// std::string DataHandle::GetWorkerPath(const LogInfo& log_info){
// 	return log_info.worker_path;
// }


std::string DataHandle::SetWorkerPath(zmq::socket_t& sock,LogInfo& log_info){
	std::string worker;
	try{
		if(s_send(sock,CLIENT_REQUEST)){
			worker=s_recv(sock);
			if(worker==HAS_NO_WORKER){
				LOG(ERROR)<<"the broker HAS_NO_WORKER";
				worker="";
			}
			else if(worker.empty())
				LOG(ERROR)<<"the broker is not avaliable";
			else {
				log_info.worker_path=worker;
			}
		}else{
			LOG(INFO)<<"send CLIENT_REQUEST failed";
		}
	}
	catch(...){
		worker="";
	}

	return worker;
}

LogInfo DataHandle::GetLogInfo(const std::string& log_name// ,LogInfo& log_info
	)
{
	LogInfo log_info;
	try{
		std::lock_guard<std::mutex>  lock(map_mutex_);
		auto pos=log_info_map_.find(log_name);
		if(pos!=log_info_map_.end()){
			log_info=pos->second;
		}
		else{
			log_info_map_.insert(std::make_pair(log_name,log_info));
		}

	}catch(...){
	}
	return log_info;

}
bool DataHandle::DeleteLogInfo(const std::string& log_name)
{
	bool ret=false;
	try{
		std::lock_guard<std::mutex>  lock(map_mutex_);
		auto pos=log_info_map_.find(log_name);
		if(pos!=log_info_map_.end()){
			log_info_map_.erase(pos);
			ret=true;
			//LOG(ERROR)<<log_name<<" removed caused by eof";
		}

	}catch(...){
	}
	return ret;
}

void DataHandle::OverTimeHandle(evutil_socket_t fd, short what, void * arg)
{
	try{
//		LOG(ERROR)<<"overtime handle";
		int *pover_time=(int *)arg;
		time_t now=time(0);
		std::lock_guard<std::mutex> lock(map_mutex_);
		for(auto pos=log_info_map_.begin();pos!=log_info_map_.end();)
			if((now-pos->second.last_access)>=*pover_time){
				LOG(ERROR)<<pos->first<<" removed caused by overtime";
				//发送OVERTIME标识
				if(!pos->second.worker_path.empty()){
					ConnItem * pitem=pos->second.pItem;
					WorkerThread * pwt=static_cast<WorkerThread *>(pitem->pthis);
					// pitem->worker_name=SetWorkerPath(pwt->requester_,file_name);
					if(s_sendmore(pwt->requester_,pitem->worker_name))
					{
						s_sendmore(pwt->requester_,pitem->log_name);
						s_send(pwt->requester_,"OVERTIME");
					}
				}
				log_info_map_.erase(pos++);
			}
			else
				++pos;
	}catch(...){
	}
	// LOG(ERROR)<<"overtime handle";
}

void DataHandle::SetLogAccessTime(LogInfo& log_info){
		time_t now=time(0);
		log_info.last_access=now;
}

void DataHandle::SetLogConnItem(LogInfo& log_info,ConnItem* pConnItem)
{
		log_info.pItem=pConnItem;
}

void DataHandle::SetLogInfo(const std::string& log_name,const LogInfo& log_info)
{
	// LogInfo log_info;
	try{
		std::lock_guard<std::mutex>  lock(map_mutex_);
		auto pos=log_info_map_.find(log_name);
		if(pos!=log_info_map_.end()){
			pos->second=log_info;
		}
		else{
			log_info_map_.insert(std::make_pair(log_name,log_info));
		}

	}catch(...){
	}
	// return log_info;
}
//暂时不用
/*off_t DataHandle::GetLogSize(const std::string& log_name)
{
	off_t ret=-1;
	try{	
		std::lock_guard<std::mutex>  lock(map_mutex_);
		auto pos=log_info_map_.find(log_name);
		if(pos!=log_info_map_.end())
			ret=pos->second.log_size_recved;
	}catch(...){
	}
	return ret;
}
//暂时不用
void DataHandle::SetLogSize(const std::string& log_name,off_t log_size)
{
	try{
		std::lock_guard<std::mutex>  lock(map_mutex_);
		auto pos=log_info_map_.find(log_name);
		if(pos!=log_info_map_.end())
			pos->second.log_size_recved+=log_size;//累加
		}
		else{
		}
	catch(...){

	}
}*/
