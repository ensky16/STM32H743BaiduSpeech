#include "lwip_client_app.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include <string.h>
#include "delay.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "lcd.h"

uint8_t NETBUS_txBuffer[1000];
uint32_t NETBUS_txLength=100;
struct netconn *tcp_clientconn=NULL;					//TCP CLIENT�������ӽṹ��
u8 tcp_client_recvbuf[TCP_CLIENT_RX_BUFSIZE];	//TCP�ͻ��˽������ݻ�����
u8 tcp_client_flag;		//TCP�ͻ������ݷ��ͱ�־λ

#define   TCPCLIENT_TASK_PRIO      1
#define   TCPCLIENT_STK_SIZE       256
TaskHandle_t TCPCLIENTTask_Handler;


//tcp�ͻ���������
void tcp_client_thread(void *arg)
{
	u32 data_len = 0;
	struct pbuf *q;
	err_t err,recv_err;
	static ip_addr_t server_ipaddr,loca_ipaddr;
	static u16_t 		 server_port,loca_port;

	LWIP_UNUSED_ARG(arg);
	server_port = REMOTE_PORT;
	IP4_ADDR(&server_ipaddr, lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
	
	while (1) 
	{
		tcp_clientconn=netconn_new(NETCONN_TCP);  //����һ��TCP����
        err = netconn_connect(tcp_clientconn,&server_ipaddr,server_port);//���ӷ�����
		if(err != ERR_OK)
		{
			netconn_delete(tcp_clientconn); //����ֵ������ERR_OK,ɾ��tcp_clientconn����
		}
		else if (err == ERR_OK)    //���������ӵ�����
		{ 
			struct netbuf *recvbuf;
			tcp_clientconn->recv_timeout = 10;
			netconn_getaddr(tcp_clientconn,&loca_ipaddr,&loca_port,1); //��ȡ����IP����IP��ַ�Ͷ˿ں�
			printf("�����Ϸ�����%d.%d.%d.%d,�����˿ں�Ϊ:%d\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3],loca_port);
			while(1)
			{
				if((tcp_client_flag & LWIP_SEND_DATA) == LWIP_SEND_DATA) //������Ҫ����
				{
//                    NETBUS_txBuffer[0]=0xAA;
//                    NETBUS_txBuffer[1]=0x8A;
//                    NETBUS_txBuffer[2]=0x3A;
//                    NETBUS_txBuffer[3]=0x5A;
//                    NETBUS_txBuffer[4]=0xCD;
					err = netconn_write(tcp_clientconn ,NETBUS_txBuffer,NETBUS_txLength,NETCONN_COPY); //��������
					if(err != ERR_OK)
					{
						printf("����ʧ��\r\n");
                        LCD_ShowString(30,210,200,16,16,"client data send fail"); 
					}
                    else
                    {
                        LCD_ShowString(30,210,200,16,16,"client data send ok"); 
                    }
                    
					tcp_client_flag &= ~LWIP_SEND_DATA;
				}
					
				if((recv_err = netconn_recv(tcp_clientconn,&recvbuf)) == ERR_OK)  //���յ�����
				{	
					taskENTER_CRITICAL();
                    LCD_ShowString(30,210,230,16,16,"receive data: "); 
                    LCD_ShowString(30,170,250,16,16,tcp_client_recvbuf); 
                    
                    //copy the receive data to transmit buffer
                    memcpy(NETBUS_txBuffer, tcp_client_recvbuf, 30);                    
                    
					memset(tcp_client_recvbuf,0,TCP_CLIENT_RX_BUFSIZE);  //���ݽ��ջ���������
					for(q=recvbuf->p;q!=NULL;q=q->next)  //����������pbuf����
					{
						//�ж�Ҫ������TCP_CLIENT_RX_BUFSIZE�е������Ƿ����TCP_CLIENT_RX_BUFSIZE��ʣ��ռ䣬�������
						//�Ļ���ֻ����TCP_CLIENT_RX_BUFSIZE��ʣ�೤�ȵ����ݣ�����Ļ��Ϳ������е�����
						if(q->len > (TCP_CLIENT_RX_BUFSIZE-data_len)) memcpy(tcp_client_recvbuf+data_len,q->payload,(TCP_CLIENT_RX_BUFSIZE-data_len));//��������
						else memcpy(tcp_client_recvbuf+data_len,q->payload,q->len);
						data_len += q->len;  	
						if(data_len > TCP_CLIENT_RX_BUFSIZE) break; //����TCP�ͻ��˽�������,����	
					}
					taskEXIT_CRITICAL();
					data_len=0;  //������ɺ�data_lenҪ���㡣					
					printf("%s\r\n",tcp_client_recvbuf);
					//NETBUS_Respond(tcp_client_recvbuf);
					netbuf_delete(recvbuf);
				}else if(recv_err == ERR_CLSD)  //�ر�����
				{
					netconn_close(tcp_clientconn);
					netconn_delete(tcp_clientconn);
					printf("������%d.%d.%d.%d�Ͽ�����\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
					break;
				}
			}
		}
        
        vTaskDelay(1000);
	}
}

//����TCP�ͻ����߳�
//����ֵ:0 TCP�ͻ��˴����ɹ�
//		���� TCP�ͻ��˴���ʧ��
uint8_t tcp_client_init(void)
{
	BaseType_t res;
	
	taskENTER_CRITICAL();              
	res = xTaskCreate((TaskFunction_t)tcp_client_thread,
					(const char*  )"tcp_client_task",
					(uint16_t     )TCPCLIENT_STK_SIZE,
					(void*        )NULL,
					(UBaseType_t  )TCPCLIENT_TASK_PRIO,
					(TaskHandle_t*)&TCPCLIENTTask_Handler);
	taskEXIT_CRITICAL();
	
	if(res == pdPASS)
	{
		return 0;
	}
	return 1;
}

