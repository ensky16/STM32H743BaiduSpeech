#include "wavplay.h" 
#include "audioplay.h"
#include "usart.h" 
#include "delay.h" 
#include "malloc.h"
//#include "ff.h"
#include "sai.h"
#include "wm8978.h"
#include "key.h"
#include "led.h"

//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32������
//WAV �������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2016/1/18
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved				
//********************************************************************************
//V1.0 ˵��
//1,֧��16λ/24λWAV�ļ�����
//2,��߿���֧�ֵ�192K/24bit��WAV��ʽ. 
////////////////////////////////////////////////////////////////////////////////// 	
 
#define FR_OK  0
 
__wavctrl wavctrl;		//WAV���ƽṹ��
vu8 wavtransferend=0;	//sai������ɱ�־
vu8 wavwitchbuf=0;		//saibufxָʾ��־
 
//WAV������ʼ��
//fname:�ļ�·��+�ļ���
//wavx:wav ��Ϣ��Žṹ��ָ��
//����ֵ:0,�ɹ�;1,���ļ�ʧ��;2,��WAV�ļ�;3,DATA����δ�ҵ�.
//u8 wav_decode_init(u8* fname,__wavctrl* wavx)
//{
//	FIL*ftemp;
//	u8 *buf; 
//	u32 br=0;
//	u8 res=0;
//	
//	ChunkRIFF *riff;
//	ChunkFMT *fmt;
//	ChunkFACT *fact;
//	ChunkDATA *data;
//	ftemp=(FIL*)mymalloc(SRAMIN,sizeof(FIL));
//	buf=mymalloc(SRAMIN,512);
//	if(ftemp&&buf)	//�ڴ�����ɹ�
//	{
//		res=f_open(ftemp,(TCHAR*)fname,FA_READ);//���ļ�
//		if(res==FR_OK)
//		{
//			f_read(ftemp,buf,512,&br);	//��ȡ512�ֽ�������
//			riff=(ChunkRIFF *)buf;		//��ȡRIFF��
//			if(riff->Format==0X45564157)//��WAV�ļ�
//			{
//				fmt=(ChunkFMT *)(buf+12);	//��ȡFMT�� 
//				fact=(ChunkFACT *)(buf+12+8+fmt->ChunkSize);//��ȡFACT��
//				if(fact->ChunkID==0X74636166||fact->ChunkID==0X5453494C)wavx->datastart=12+8+fmt->ChunkSize+8+fact->ChunkSize;//����fact/LIST���ʱ��(δ����)
//				else wavx->datastart=12+8+fmt->ChunkSize;  
//				data=(ChunkDATA *)(buf+wavx->datastart);	//��ȡDATA��
//				if(data->ChunkID==0X61746164)//�����ɹ�!
//				{
//					wavx->audioformat=fmt->AudioFormat;		//��Ƶ��ʽ
//					wavx->nchannels=fmt->NumOfChannels;		//ͨ����
//					wavx->samplerate=fmt->SampleRate;		//������
//					wavx->bitrate=fmt->ByteRate*8;			//�õ�λ��
//					wavx->blockalign=fmt->BlockAlign;		//�����
//					wavx->bps=fmt->BitsPerSample;			//λ��,16/24/32λ
//					
//					wavx->datasize=data->ChunkSize;			//���ݿ��С
//					wavx->datastart=wavx->datastart+8;		//��������ʼ�ĵط�. 
//					 
//					printf("\r\n");
//					printf("wavx->audioformat:%d\r\n",wavx->audioformat);
//					printf("wavx->nchannels:%d\r\n",wavx->nchannels);
//					printf("wavx->samplerate:%d\r\n",wavx->samplerate);
//					printf("wavx->bitrate:%d\r\n",wavx->bitrate);
//					printf("wavx->blockalign:%d\r\n",wavx->blockalign);
//					printf("wavx->bps:%d\r\n",wavx->bps);
//					printf("wavx->datasize:%d\r\n",wavx->datasize);
//					printf("wavx->datastart:%d\r\n",wavx->datastart);  
//				}else res=3;//data����δ�ҵ�.
//			}else res=2;//��wav�ļ�
//			
//		}else res=1;//���ļ�����
//	}
//	f_close(ftemp);
//	myfree(SRAMIN,ftemp);//�ͷ��ڴ�
//	myfree(SRAMIN,buf); 
//	return 0;
//}

////���buf
////buf:������
////size:���������
////bits:λ��(16/24)
////����ֵ:���������ݸ���
//u32 wav_buffill(u8 *buf,u16 size,u8 bits)
//{
//	u16 readlen=0;
//	u32 bread;
//	u16 i;
//	u32 *p,*pbuf;
//	if(bits==24)//24bit��Ƶ,��Ҫ����һ��
//	{
//		readlen=(size/4)*3;		//�˴�Ҫ��ȡ���ֽ���
//		f_read(audiodev.file, audiodev.tbuf, readlen, (UINT*)&bread);//��ȡ���� 
//		pbuf=(u32*)buf;
//		for(i=0;i<size/4;i++)
//		{  
//			p=(u32*)(audiodev.tbuf+i*3);
//			pbuf[i]=p[0];  
//		} 
//		bread=(bread*4)/3;		//����Ĵ�С.
//	}else 
//	{
//		f_read(audiodev.file, buf,size, (UINT*)&bread);//16bit��Ƶ,ֱ�Ӷ�ȡ����  
//		if(bread<size)//����������,����0
//		{
//			for(i=bread;i<size-bread;i++)buf[i]=0; 
//		}
//	}
//	return bread;
//}  
//WAV����ʱ,SAI DMA����ص�����
void wav_sai_dma_tx_callback(void) 
{   
	u16 i;
	if(DMA1_Stream5->CR&(1<<19))
	{
		wavwitchbuf=0;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_SAI_TX_DMA_BUFSIZE;i++)//��ͣ
			{
				audiodev.saibuf1[i]=0;//���0
			}
		}
	}else 
	{
		wavwitchbuf=1;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_SAI_TX_DMA_BUFSIZE;i++)//��ͣ
			{
				audiodev.saibuf2[i]=0;//���0
			}
		}
	}
	wavtransferend=1;
} 
////�õ���ǰ����ʱ��
////fx:�ļ�ָ��
////wavx:wav���ſ�����
//void wav_get_curtime(FIL*fx,__wavctrl *wavx)
//{
//	long long fpos;  	
// 	wavx->totsec=wavx->datasize/(wavx->bitrate/8);	//�����ܳ���(��λ:��) 
//	fpos=fx->fptr-wavx->datastart; 					//�õ���ǰ�ļ����ŵ��ĵط� 
//	wavx->cursec=fpos*wavx->totsec/wavx->datasize;	//��ǰ���ŵ��ڶ�������?	
//}
 



/* total wave data size is: wavsize
*@param au08WaveData: this buffer contains the wave data, including header
*@param au08ResultBuffer: this is the output buffer 
*@param u16ReadLen read length
*@return real read length
*/
extern u32 wavsize;
u32 u32WaveReadOffset=0;
u32 readWaveData(u8 * au08WaveData, u8 *au08ResultBuffer, u16 u16ReadLen )
{
	u32 u32RetValue=0, i=0;
    
    u32 u32WaveSize=0;
    u32 u32Temp=0;
    u32 u32RemainingDataSize=0;
 
    u32Temp=au08WaveData[7];
    u32WaveSize|=(u32Temp<<24);
    u32Temp=au08WaveData[6];
    u32WaveSize|=(u32Temp<<16);
    u32Temp=au08WaveData[5];
    u32WaveSize|=(u32Temp<<8);
    u32Temp=au08WaveData[4];
    u32WaveSize|=(u32Temp<<0);
    
    u32RemainingDataSize=u32WaveSize-u32WaveReadOffset;
	
	if(u32RemainingDataSize>=u16ReadLen)
	{
		memcpy(au08ResultBuffer, &au08WaveData[u32WaveReadOffset], u16ReadLen);
		u32WaveReadOffset+=u16ReadLen;
		u32RetValue=u16ReadLen;
	}
	else
	{
		memcpy(au08ResultBuffer, &au08WaveData[u32WaveReadOffset], u32RemainingDataSize);
		u32WaveReadOffset+=u32RemainingDataSize;
		for(i=u32RemainingDataSize; i<u16ReadLen; i++)
		{
			au08ResultBuffer[i]=0;
		}
		u32RetValue=u32RemainingDataSize;
	}

	return u32RetValue;
}


//total wave data size is: wavsize

u8 wav_play_for_16bps(u8 * au08WaveData)
{
//	u8 key;
//	u8 t=0; 
	u8 res;  
	u32 fillnum; 
	u32 u32WaveDataStart=sizeof(__WaveHeader);
	audiodev.saibuf1=mymalloc(SRAMIN,WAV_SAI_TX_DMA_BUFSIZE);
	audiodev.saibuf2=mymalloc(SRAMIN,WAV_SAI_TX_DMA_BUFSIZE);

	WM8978_I2S_Cfg(2,0);	//16 bit data length
	SAIA_Init(SAI_MODEMASTER_TX, SAI_CLOCKSTROBING_RISINGEDGE, SAI_DATASIZE_16);
	SAIA_SampleRate_Set(44100);//set sample rate
	SAIA_TX_DMA_Init(audiodev.saibuf1,audiodev.saibuf2,WAV_SAI_TX_DMA_BUFSIZE/2,1); //set TX DMA, 16 bits
	sai_tx_callback=wav_sai_dma_tx_callback;	
	audio_stop();
	
	//it's a flag
	res=0;
	u32WaveReadOffset=0;
	//copy wave data to cache		
	u32WaveReadOffset=u32WaveDataStart;
	fillnum=readWaveData(au08WaveData, audiodev.saibuf1, WAV_SAI_TX_DMA_BUFSIZE );
	fillnum=readWaveData(au08WaveData, audiodev.saibuf2, WAV_SAI_TX_DMA_BUFSIZE );
	audio_start();  	
	
	while(res==0)
	{ 
		while(wavtransferend==0);//wait DMA transmit finish
		wavtransferend=0;
		if(fillnum!=WAV_SAI_TX_DMA_BUFSIZE)//check if data read been finished
		{
			res=KEY0_PRES;
			break;
		} 
		if(wavwitchbuf)
			fillnum=readWaveData(au08WaveData, audiodev.saibuf2,WAV_SAI_TX_DMA_BUFSIZE);//���buf2
		else 
			fillnum=readWaveData(au08WaveData, audiodev.saibuf1,WAV_SAI_TX_DMA_BUFSIZE );//���buf1
        
	}
	
	audio_stop();

	myfree(SRAMIN,audiodev.saibuf1);//
	myfree(SRAMIN,audiodev.saibuf2);// 
    return res;
}











