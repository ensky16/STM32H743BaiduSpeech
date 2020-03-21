#ifndef LWIP_CLIENT_APP_H
#define LWIP_CLIENT_APP_H
#include "sys.h"   
 
 
#define TCP_CLIENT_RX_BUFSIZE	1500	//接收缓冲区长度
#define REMOTE_PORT				8091	//定义远端主机的IP地址
#define LWIP_SEND_DATA			0X80    //定义有数据发送


extern u8 tcp_client_recvbuf[TCP_CLIENT_RX_BUFSIZE];	//TCP客户端接收数据缓冲区
extern u8 tcp_client_flag;		    //TCP客户端数据发送标志位

uint8_t tcp_client_init(void);  //tcp客户端初始化(创建tcp客户端线程)
#endif

