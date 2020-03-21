#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h"
#include "key.h"
#include "lwip_client_app.h"
#include "lcd.h"
#include "sdram.h"
#include "usmart.h"
#include "pcf8574.h"
#include "timer.h"
#include "mpu.h"
#include "malloc.h"
#include "lwip/netif.h"
#include "lwip_comm.h"
#include "lwipopts.h"
#include "FreeRTOS.h"
#include "task.h"
#include "wm8978.h"
#include "recorder.h"
#include "w25qxx.h"
#include "fontupd.h"

extern void TCPClient_Socket(void);
extern int baiduSpeechMain(void);
extern void sendPostRequest(void);

#define   START_TASK_PRIO      1
#define   START_STK_SIZE       128
TaskHandle_t StartTask_Handler;
void Start_task(void *pvParameters);

#define    StatusIndicate_TASK_PRIO        2
#define    StatusIndicate_STK_SIZE        64

TaskHandle_t StatusIndicateTask_Handler;

void StatusIndicate_task(void *p_arg);

#define      DataAcquisition_TASK_PRIO      5
#define      DataAcquisition_STK_SIZE       128

TaskHandle_t DataAcquisitionTask_Handler;

void DataAcquisition_task(void *p_arg);

//��LCD����ʾ��ַ��Ϣ
//mode:2 ��ʾDHCP��ȡ���ĵ�ַ
//	  ���� ��ʾ��̬��ַ
void show_address(u8 mode)
{
	u8 buf[30];
	if(mode==2)
	{
		sprintf((char*)buf,"MAC    :%d.%d.%d.%d.%d.%d",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);//��ӡMAC��ַ
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"DHCP IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//��ӡ��̬IP��ַ
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"DHCP GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//��ӡ���ص�ַ
		LCD_ShowString(30,170,210,16,16,buf); 
		sprintf((char*)buf,"DHCP IP:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//��ӡ���������ַ
		LCD_ShowString(30,190,210,16,16,buf); 
	}
	else 
	{
		sprintf((char*)buf,"MAC      :%d.%d.%d.%d.%d.%d",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);//��ӡMAC��ַ
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//��ӡ��̬IP��ַ
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"Static GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//��ӡ���ص�ַ
		LCD_ShowString(30,170,210,16,16,buf); 
		sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//��ӡ���������ַ
		LCD_ShowString(30,190,210,16,16,buf); 
	}	
}

int main(void)
{
    Write_Through();                        //����ǿ��͸д��
    MPU_Memory_Protection();                //������ش洢����
    Cache_Enable();                         //��L1-Cache
	HAL_Init();				        		//��ʼ��HAL��
	Stm32_Clock_Init(160,5,2,4);  		    //����ʱ��,400Mhz 
	delay_init(400);						//��ʱ��ʼ��
	uart_init(115200);						//���ڳ�ʼ��
	LED_Init();								//��ʼ��LED
	KEY_Init();								//��ʼ������
	SDRAM_Init();                   		//��ʼ��SDRAM
	LCD_Init();								//��ʼ��LCD
    W25QXX_Init();				    		//��ʼ��W25Q256
    WM8978_Init();				    		//��ʼ��WM8978
	WM8978_HPvol_Set(40,40);	    		//������������
	WM8978_SPKvol_Set(40);		    		//������������
	PCF8574_Init();                         //��ʼ��PCF8574   
	my_mem_init(SRAMIN);			        //��ʼ���ڲ��ڴ��(AXI)
	my_mem_init(SRAMEX);			        //��ʼ���ⲿ�ڴ��(SDRAM)
	my_mem_init(SRAM12);			        //��ʼ��SRAM12�ڴ��(SRAM1+SRAM2)
	my_mem_init(SRAM4);				        //��ʼ��SRAM4�ڴ��(SRAM4)
	my_mem_init(SRAMDTCM);			        //��ʼ��DTCM�ڴ��(DTCM)
	my_mem_init(SRAMITCM);			        //��ʼ��ITCM�ڴ��(ITCM)
	while(font_init()) 			    		//����ֿ�
	{	    
		LCD_ShowString(30,50,200,16,16,"Font Error!");
		delay_ms(200);				  
		LCD_Fill(30,50,240,66,WHITE);//�����ʾ	     
		delay_ms(200);				  
	}  
    POINT_COLOR = RED; 		
	LCD_ShowString(30,30,200,16,16,"Apollo STM32F4/F7/H7");
	LCD_ShowString(30,50,200,16,16,"Ethernet lwIP est");
	LCD_ShowString(30,70,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,90,200,16,16,"2018/7/13"); 
  
    TIM3_Init(1000-1,2000-1);               //��ʱ��3��ʼ������ʱ��ʱ��Ϊ200M����Ƶϵ��Ϊ2000-1��
                                            //���Զ�ʱ��3��Ƶ��Ϊ200M/2000=100K���Զ���װ��Ϊ1000-1����ô��ʱ�����ھ���10ms
	while(lwip_comm_init_ethernet())                 //lwip��ʼ��
	{
		LCD_ShowString(30,110,200,20,16,"LWIP Init Falied! ");
		delay_ms(500);
		LCD_ShowString(30,110,200,16,16,"Retrying...       ");
        delay_ms(500);
	}
	LCD_ShowString(30,110,200,20,16,"LWIP Init Success!");
 	LCD_ShowString(30,130,200,16,16,"DHCP IP configing...");  //�ȴ�DHCP��ȡ 
	while((lwipdev.dhcpstatus!=2)&&(lwipdev.dhcpstatus!=0XFF))//�ȴ�DHCP��ȡ�ɹ�/��ʱ���
	{  
		lwip_periodic_handle();	//LWIP�ں���Ҫ��ʱ����ĺ���
	}
    show_address(lwipdev.dhcpstatus);	//��ʾ��ַ��Ϣ
    
	xTaskCreate((TaskFunction_t)Start_task,
                (const char*  )"Start_task",
                (uint16_t     )START_STK_SIZE,
                (void*        )NULL,
                (UBaseType_t  )START_TASK_PRIO,
                (TaskHandle_t*)&StartTask_Handler);

	vTaskStartScheduler();
 
    while(1);
}


void Start_task(void *pvParameters)
{
	taskENTER_CRITICAL();

	xTaskCreate((TaskFunction_t)StatusIndicate_task,
                (const char*  )"StatusIndicate_task",
                (uint16_t     )StatusIndicate_STK_SIZE,
                (void*        )NULL,
                (UBaseType_t  )StatusIndicate_TASK_PRIO,
                (TaskHandle_t*)&StatusIndicateTask_Handler);
                
							
	xTaskCreate((TaskFunction_t)DataAcquisition_task,
                (const char*  )"DataAcquisiton_task",
                (uint16_t     )DataAcquisition_STK_SIZE,
                (void*        )NULL,
                (UBaseType_t  )DataAcquisition_TASK_PRIO,
                (TaskHandle_t*)&DataAcquisitionTask_Handler);
				
	vTaskDelete(StartTask_Handler);
	taskEXIT_CRITICAL();							
}


void StatusIndicate_task(void *p_arg)
{
	while(1)
	{
        delay_ms(200);  
        LED0_Toggle;
        delay_ms(200);  
        LED1_Toggle;
        baiduSpeechMain();
		vTaskDelay(1000);
	}
}


void DataAcquisition_task(void * p_arg)
{
	while(1)
	{
        tcp_client_flag |= LWIP_SEND_DATA;
		vTaskDelay(1000);
	}
}
