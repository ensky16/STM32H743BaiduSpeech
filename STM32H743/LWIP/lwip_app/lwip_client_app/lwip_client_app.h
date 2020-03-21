#ifndef LWIP_CLIENT_APP_H
#define LWIP_CLIENT_APP_H
#include "sys.h"   
 
 
#define TCP_CLIENT_RX_BUFSIZE	1500	//���ջ���������
#define REMOTE_PORT				8091	//����Զ��������IP��ַ
#define LWIP_SEND_DATA			0X80    //���������ݷ���


extern u8 tcp_client_recvbuf[TCP_CLIENT_RX_BUFSIZE];	//TCP�ͻ��˽������ݻ�����
extern u8 tcp_client_flag;		    //TCP�ͻ������ݷ��ͱ�־λ

uint8_t tcp_client_init(void);  //tcp�ͻ��˳�ʼ��(����tcp�ͻ����߳�)
#endif

