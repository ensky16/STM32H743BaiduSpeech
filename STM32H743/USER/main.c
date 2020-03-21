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

//在LCD上显示地址信息
//mode:2 显示DHCP获取到的地址
//	  其他 显示静态地址
void show_address(u8 mode)
{
	u8 buf[30];
	if(mode==2)
	{
		sprintf((char*)buf,"MAC    :%d.%d.%d.%d.%d.%d",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);//打印MAC地址
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"DHCP IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//打印动态IP地址
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"DHCP GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//打印网关地址
		LCD_ShowString(30,170,210,16,16,buf); 
		sprintf((char*)buf,"DHCP IP:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//打印子网掩码地址
		LCD_ShowString(30,190,210,16,16,buf); 
	}
	else 
	{
		sprintf((char*)buf,"MAC      :%d.%d.%d.%d.%d.%d",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);//打印MAC地址
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//打印动态IP地址
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"Static GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//打印网关地址
		LCD_ShowString(30,170,210,16,16,buf); 
		sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//打印子网掩码地址
		LCD_ShowString(30,190,210,16,16,buf); 
	}	
}

int main(void)
{
    Write_Through();                        //开启强制透写！
    MPU_Memory_Protection();                //保护相关存储区域
    Cache_Enable();                         //打开L1-Cache
	HAL_Init();				        		//初始化HAL库
	Stm32_Clock_Init(160,5,2,4);  		    //设置时钟,400Mhz 
	delay_init(400);						//延时初始化
	uart_init(115200);						//串口初始化
	LED_Init();								//初始化LED
	KEY_Init();								//初始化按键
	SDRAM_Init();                   		//初始化SDRAM
	LCD_Init();								//初始化LCD
    W25QXX_Init();				    		//初始化W25Q256
    WM8978_Init();				    		//初始化WM8978
	WM8978_HPvol_Set(40,40);	    		//耳机音量设置
	WM8978_SPKvol_Set(40);		    		//喇叭音量设置
	PCF8574_Init();                         //初始化PCF8574   
	my_mem_init(SRAMIN);			        //初始化内部内存池(AXI)
	my_mem_init(SRAMEX);			        //初始化外部内存池(SDRAM)
	my_mem_init(SRAM12);			        //初始化SRAM12内存池(SRAM1+SRAM2)
	my_mem_init(SRAM4);				        //初始化SRAM4内存池(SRAM4)
	my_mem_init(SRAMDTCM);			        //初始化DTCM内存池(DTCM)
	my_mem_init(SRAMITCM);			        //初始化ITCM内存池(ITCM)
	while(font_init()) 			    		//检查字库
	{	    
		LCD_ShowString(30,50,200,16,16,"Font Error!");
		delay_ms(200);				  
		LCD_Fill(30,50,240,66,WHITE);//清除显示	     
		delay_ms(200);				  
	}  
    POINT_COLOR = RED; 		
	LCD_ShowString(30,30,200,16,16,"Apollo STM32F4/F7/H7");
	LCD_ShowString(30,50,200,16,16,"Ethernet lwIP est");
	LCD_ShowString(30,70,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,90,200,16,16,"2018/7/13"); 
  
    TIM3_Init(1000-1,2000-1);               //定时器3初始化，定时器时钟为200M，分频系数为2000-1，
                                            //所以定时器3的频率为200M/2000=100K，自动重装载为1000-1，那么定时器周期就是10ms
	while(lwip_comm_init_ethernet())                 //lwip初始化
	{
		LCD_ShowString(30,110,200,20,16,"LWIP Init Falied! ");
		delay_ms(500);
		LCD_ShowString(30,110,200,16,16,"Retrying...       ");
        delay_ms(500);
	}
	LCD_ShowString(30,110,200,20,16,"LWIP Init Success!");
 	LCD_ShowString(30,130,200,16,16,"DHCP IP configing...");  //等待DHCP获取 
	while((lwipdev.dhcpstatus!=2)&&(lwipdev.dhcpstatus!=0XFF))//等待DHCP获取成功/超时溢出
	{  
		lwip_periodic_handle();	//LWIP内核需要定时处理的函数
	}
    show_address(lwipdev.dhcpstatus);	//显示地址信息
    
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
