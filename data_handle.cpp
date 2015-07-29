/*
 * data_handle.cpp
 *
 *  Created on: 2015年7月22日
 *      Author: chenms
 */
#include"data_handle.h"

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

int DataHandle::session_id_=1;

std::set<CmdHandle> DataHandle::cmd_handle_set_(cmd_name,cmd_name+sizeof(cmd_name)/sizeof(CmdHandle));

void DataHandle::AnalyzeData(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	struct evbuffer * input=bufferevent_get_input(buffer);//得到ioevbuffer
	unsigned char lengthch[2]={0};
	int length=0;
	char * cmd_type=NULL;
	char * cmd_name=NULL;

	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	do{
		//上次的数据包未处理完，直接进入函数处理
		if(pitem->data_remain_length){
			PureDataHandle(arg,arg2);
			break;
		}

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
		if(!n_read_out||strncmp("[Request]",cmd_type,9))//非指令请求包
		{
			if(evbuffer_drain(input,2)!=-1)//当成是数据包，移除\r\n
			{
				char data_head[35]={0};
				evbuffer_remove(input,data_head,35);
				pitem->data_remain_length=length-35-4;
				PureDataHandle(arg,arg2);
			}
			break;
		}

		//指令包的处理
		cmd_name=evbuffer_readln(input,&n_read_out,EVBUFFER_EOL_CRLF_STRICT);
		if(!n_read_out)//非正确的指令包格式
			break;
		CmdHandle cmd_handle_temp;
		cmd_handle_temp.command_name_=cmd_name;
		auto pos=cmd_handle_set_.find(cmd_handle_temp);
		if(pos!=cmd_handle_set_.end()){
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
}

void DataHandle::LoginHandle(void *arg,void *arg2)
{
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	//WorkerThread * pwt=static_cast<WorkerThread*>(pitem->pthis);

	bufferevent * buffer=static_cast<bufferevent *>(arg);
	evbuffer * out=bufferevent_get_output(buffer);
	evbuffer_add_printf(out,"[Response]\r\n");
	evbuffer_add_printf(out,"Command=Login\r\n");
	evbuffer_add_printf(out,"Session=%d\r\n",pitem->session_id);
	evbuffer_add_printf(out,"Result=AC\r\n");//TODO 需要做逻辑判断
	//evbuffer_add_printf(out,"Code=A Code\r\n");
	evbuffer_add_printf(out,"SverU=Y\r\n");
	evbuffer_add_printf(out,"CverU=Y\r\n");
	WriteDataSize(out);
}

void DataHandle::LogoutHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	bufferevent_free(buffer);
}


void DataHandle::SyncHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	evbuffer * in=bufferevent_get_input(buffer);
	char * session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
	if(session_id){
		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(out,"Command=Sync Time\r\n ");
		evbuffer_add_printf(out,"Session=%s\r\n ",session_id);
		evbuffer_add_printf(out,"URL=127.0.0.1\r\n ");
		free(session_id);
	}
}

void DataHandle::ConfigHandle(void *arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	//TODO
	evbuffer * in=bufferevent_get_input(buffer);
	char * session_id=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
	if(session_id){
		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(out,"Command=Config\r\n ");
		evbuffer_add_printf(out,"Session=%s\r\n ",session_id);
		evbuffer_add_printf(out,"Cver=config version\r\n ");
		evbuffer_add_printf(out,"Result=AC\r\n ");
		evbuffer_add_printf(out,"Code=A Code\r\n ");
		evbuffer_add_printf(out,"PacketCount=N\r\n ");
		evbuffer_add_printf(out,"PacketNo=n\r\n ");
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
		new_file=evbuffer_readln(in,NULL,EVBUFFER_EOL_CRLF_STRICT);
		if(!new_file)
			break;

		evbuffer * out=bufferevent_get_output(buffer);
		evbuffer_add_printf(out,"[Response]\r\n");
		evbuffer_add_printf(out,"Command=Upload\r\n");
		evbuffer_add_printf(out,"%s\r\n",session_id);
		evbuffer_add_printf(out,"%s\r\n",file_name);
		evbuffer_add_printf(out,"Result=AC\r\n");
		//evbuffer_add_printf(out,"Code=A Code\r\n ");
		//获取文件大小
//		 struct stat buf;
//		 char path[256]={0};
//		 sprintf(path,"./%s",file_name);
//		 if(stat(path, &buf)==-1)
//			 buf.st_size=0;
		//创建文件
		char path[256]={0};
		sprintf(path,"/home/chenms/cms/datang2/%s",file_name+9);
		int fd=open(path,O_WRONLY|O_APPEND|O_CREAT,S_IWUSR|S_IRUSR);
		pitem->log_fd=fd;
		//	evbuffer_write(input,fd);
		//	close(fd);
		evbuffer_add_printf(out,"Size=0\r\n");//TODO
		evbuffer_add_printf(out,"TriDES= triple-DES\r\n");//TODO
		WriteDataSize(out);
	}while(0);

	free(session_id);
	free(file_name);
	free(new_file);
}

void DataHandle::EofHandle(void *arg,void *arg2)
{
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	if(pitem->log_fd!=-1){
		close(pitem->log_fd);
		pitem->log_fd=-1;
	}
//	TODO返回Eof的响应
}

void DataHandle::PureDataHandle(void * arg,void *arg2)
{
	bufferevent * buffer=static_cast<bufferevent *>(arg);
	ConnItem * pitem=static_cast<ConnItem *>(arg2);
	evbuffer * in=bufferevent_get_input(buffer);
	int length=0;
	if(pitem->log_fd!=-1)
		length=evbuffer_write_atmost(in,pitem->log_fd,pitem->data_remain_length);//写数据
	pitem->data_remain_length-=length;
	if(pitem->data_remain_length<4096)
		int i=0;
//	if(pitem->data_remain_length)
}
//	char  flag[30]={0};
//	char  session_id_ch[5]={0};
//	char length_ch[3]={0};
//	int session_id=0;
//	int length=0;
//
//	do{
//		if(evbuffer_remove(in,flag,29)!=29)
//			break;
//		if((evbuffer_remove(in,session_id_ch,4)!=4)||!(session_id=atoi(session_id_ch)))
//			break;
//		if((evbuffer_remove(in,length_ch,2)!=2)||!(length=atoi(length_ch)))
//			break;

