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
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//WAV 解码代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2016/1/18
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved				
//********************************************************************************
//V1.0 说明
//1,支持16位/24位WAV文件播放
//2,最高可以支持到192K/24bit的WAV格式. 
////////////////////////////////////////////////////////////////////////////////// 	
 
#define FR_OK  0
 
__wavctrl wavctrl;		//WAV控制结构体
vu8 wavtransferend=0;	//sai传输完成标志
vu8 wavwitchbuf=0;		//saibufx指示标志
 
//WAV解析初始化
//fname:文件路径+文件名
//wavx:wav 信息存放结构体指针
//返回值:0,成功;1,打开文件失败;2,非WAV文件;3,DATA区域未找到.
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
//	if(ftemp&&buf)	//内存申请成功
//	{
//		res=f_open(ftemp,(TCHAR*)fname,FA_READ);//打开文件
//		if(res==FR_OK)
//		{
//			f_read(ftemp,buf,512,&br);	//读取512字节在数据
//			riff=(ChunkRIFF *)buf;		//获取RIFF块
//			if(riff->Format==0X45564157)//是WAV文件
//			{
//				fmt=(ChunkFMT *)(buf+12);	//获取FMT块 
//				fact=(ChunkFACT *)(buf+12+8+fmt->ChunkSize);//读取FACT块
//				if(fact->ChunkID==0X74636166||fact->ChunkID==0X5453494C)wavx->datastart=12+8+fmt->ChunkSize+8+fact->ChunkSize;//具有fact/LIST块的时候(未测试)
//				else wavx->datastart=12+8+fmt->ChunkSize;  
//				data=(ChunkDATA *)(buf+wavx->datastart);	//读取DATA块
//				if(data->ChunkID==0X61746164)//解析成功!
//				{
//					wavx->audioformat=fmt->AudioFormat;		//音频格式
//					wavx->nchannels=fmt->NumOfChannels;		//通道数
//					wavx->samplerate=fmt->SampleRate;		//采样率
//					wavx->bitrate=fmt->ByteRate*8;			//得到位速
//					wavx->blockalign=fmt->BlockAlign;		//块对齐
//					wavx->bps=fmt->BitsPerSample;			//位数,16/24/32位
//					
//					wavx->datasize=data->ChunkSize;			//数据块大小
//					wavx->datastart=wavx->datastart+8;		//数据流开始的地方. 
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
//				}else res=3;//data区域未找到.
//			}else res=2;//非wav文件
//			
//		}else res=1;//打开文件错误
//	}
//	f_close(ftemp);
//	myfree(SRAMIN,ftemp);//释放内存
//	myfree(SRAMIN,buf); 
//	return 0;
//}

////填充buf
////buf:数据区
////size:填充数据量
////bits:位数(16/24)
////返回值:读到的数据个数
//u32 wav_buffill(u8 *buf,u16 size,u8 bits)
//{
//	u16 readlen=0;
//	u32 bread;
//	u16 i;
//	u32 *p,*pbuf;
//	if(bits==24)//24bit音频,需要处理一下
//	{
//		readlen=(size/4)*3;		//此次要读取的字节数
//		f_read(audiodev.file, audiodev.tbuf, readlen, (UINT*)&bread);//读取数据 
//		pbuf=(u32*)buf;
//		for(i=0;i<size/4;i++)
//		{  
//			p=(u32*)(audiodev.tbuf+i*3);
//			pbuf[i]=p[0];  
//		} 
//		bread=(bread*4)/3;		//填充后的大小.
//	}else 
//	{
//		f_read(audiodev.file, buf,size, (UINT*)&bread);//16bit音频,直接读取数据  
//		if(bread<size)//不够数据了,补充0
//		{
//			for(i=bread;i<size-bread;i++)buf[i]=0; 
//		}
//	}
//	return bread;
//}  
//WAV播放时,SAI DMA传输回调函数
void wav_sai_dma_tx_callback(void) 
{   
	u16 i;
	if(DMA1_Stream5->CR&(1<<19))
	{
		wavwitchbuf=0;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_SAI_TX_DMA_BUFSIZE;i++)//暂停
			{
				audiodev.saibuf1[i]=0;//填充0
			}
		}
	}else 
	{
		wavwitchbuf=1;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_SAI_TX_DMA_BUFSIZE;i++)//暂停
			{
				audiodev.saibuf2[i]=0;//填充0
			}
		}
	}
	wavtransferend=1;
} 
////得到当前播放时间
////fx:文件指针
////wavx:wav播放控制器
//void wav_get_curtime(FIL*fx,__wavctrl *wavx)
//{
//	long long fpos;  	
// 	wavx->totsec=wavx->datasize/(wavx->bitrate/8);	//歌曲总长度(单位:秒) 
//	fpos=fx->fptr-wavx->datastart; 					//得到当前文件播放到的地方 
//	wavx->cursec=fpos*wavx->totsec/wavx->datasize;	//当前播放到第多少秒了?	
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
			fillnum=readWaveData(au08WaveData, audiodev.saibuf2,WAV_SAI_TX_DMA_BUFSIZE);//填充buf2
		else 
			fillnum=readWaveData(au08WaveData, audiodev.saibuf1,WAV_SAI_TX_DMA_BUFSIZE );//填充buf1
        
	}
	
	audio_stop();

	myfree(SRAMIN,audiodev.saibuf1);//
	myfree(SRAMIN,audiodev.saibuf2);// 
    return res;
}











