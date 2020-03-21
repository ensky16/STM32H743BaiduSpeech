#ifndef __SAI_H
#define __SAI_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//SAI驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2017/8/17
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
extern SAI_HandleTypeDef SAI1A_Handler;        //SAI1 Block A句柄
extern SAI_HandleTypeDef SAI1B_Handler;        //SAI1 Block B句柄
extern DMA_HandleTypeDef SAI1_TXDMA_Handler;   //DMA发送句柄
extern DMA_HandleTypeDef SAI1_RXDMA_Handler;   //DMA接收句柄

extern void (*sai_tx_callback)(void);		//sai tx回调函数指针  
extern void (*sai_rx_callback)(void);		//sai rx回调函数指针

void SAIA_Init(u32 mode,u32 cpol,u32 datalen);
void SAIB_Init(u32 mode,u32 cpol,u32 datalen);
u8 SAIA_SampleRate_Set(u32 samplerate);
void SAIA_TX_DMA_Init(u8* buf0,u8 *buf1,u16 num,u8 width);
void SAIA_RX_DMA_Init(u8* buf0,u8 *buf1,u16 num,u8 width);
void SAIA_DMA_Enable(void);
void SAIB_DMA_Enable(void);
void SAI_Play_Start(void); 
void SAI_Play_Stop(void); 
void SAI_Rec_Start(void);
void SAI_Rec_Stop(void);
#endif
