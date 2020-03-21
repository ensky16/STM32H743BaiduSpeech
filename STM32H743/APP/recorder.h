#ifndef __RECORDER_H
#define __RECORDER_H
#include "sys.h"
//#include "ff.h"
#include "wavplay.h" 
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32������
//¼���� Ӧ�ô���	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2016/1/11
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
 
#define SAI_RX_DMA_BUF_SIZE    	4096		//����RX DMA �����С
#define SAI_RX_FIFO_SIZE		10			//�������FIFO��С

#define REC_SAMPLERATE			44100		//������,44.1Khz

u8 rec_sai_fifo_read(u8 **buf);
u8 rec_sai_fifo_write(u8 *buf);

void rec_sai_dma_rx_callback(void);
void recoder_enter_rec_mode(void);
void recoder_wav_init(__WaveHeader* wavhead);
void recoder_msg_show(u32 tsec,u32 kbps);
void recoder_remindmsg_show(u8 mode);
void recoder_new_pathname(u8 *pname); 
void wav_recorder(void);

#endif












