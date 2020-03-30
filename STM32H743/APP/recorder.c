#include "recorder.h" 
#include "audioplay.h"
//#include "ff.h"
#include "malloc.h"
#include "text.h"
#include "usart.h"
#include "wm8978.h"
#include "sai.h"
#include "led.h"
#include "lcd.h"
#include "delay.h"
#include "key.h"
//#include "exfuns.h"  
//#include "text.h"
#include "string.h"  
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//录音机 应用代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2016/1/11
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
  
u8 *sairecbuf1;			//SAI1 DMA接收BUF1
u8 *sairecbuf2; 		//SAI1 DMA接收BUF2
u8 *au08WaveCache;

//REC录音FIFO管理参数.
//由于FATFS文件写入时间的不确定性,如果直接在接收中断里面写文件,可能导致某次写入时间过长
//从而引起数据丢失,故加入FIFO控制,以解决此问题.
vu8 sairecfifordpos=0;	//FIFO读位置
vu8 sairecfifowrpos=0;	//FIFO写位置
u8 *sairecfifobuf[SAI_RX_FIFO_SIZE];//定义10个录音接收FIFO

//FIL* f_rec=0;			//录音文件	
u32 wavsize;			//wav数据大小(字节数,不包括文件头!!)
u8 rec_sta=0;			//录音状态
						//[7]:0,没有开启录音;1,已经开启录音;
						//[6:1]:保留
						//[0]:0,正在录音;1,暂停录音;
                        
u32 u32WaveCacheLen=0;
u32 u32WaveCacheOfs=0;
u32 u32ReadLen=0;
extern u8 wav_play_for_16bps(u8 * au08WaveData);
extern u8 * sendWavDataToServer(unsigned char * audio_data, unsigned int u32SpeechDataLen, unsigned int * u32RobotLen);

//读取录音FIFO
//buf:数据缓存区首地址
//返回值:0,没有数据可读;
//      1,读到了1个数据块
u8 rec_sai_fifo_read(u8 **buf)
{
	if(sairecfifordpos==sairecfifowrpos)
        return 0;
	
    sairecfifordpos++;		//读位置加1
	
    if(sairecfifordpos>=SAI_RX_FIFO_SIZE)
        sairecfifordpos=0;//归零 
	
    *buf=sairecfifobuf[sairecfifordpos];
    
	return 1;
}
//写一个录音FIFO
//buf:数据缓存区首地址
//返回值:0,写入成功;
//      1,写入失败
u8 rec_sai_fifo_write(u8 *buf)
{
	u16 i; 
	u8 temp=sairecfifowrpos;//记录当前写位置
	sairecfifowrpos++;		//写位置加1
	if(sairecfifowrpos>=SAI_RX_FIFO_SIZE)sairecfifowrpos=0;//归零  
	if(sairecfifordpos==sairecfifowrpos)
	{
		sairecfifowrpos=temp;//还原原来的写位置,此次写入失败
		return 1;	
	}
    
	for(i=0;i<SAI_RX_DMA_BUF_SIZE;i++)
        sairecfifobuf[sairecfifowrpos][i]=buf[i];//拷贝数据
    
	return 0;
} 

//录音 SAI_DMA接收中断服务函数.在中断里面写入数据
void rec_sai_dma_rx_callback(void) 
{      
	if(rec_sta==0X80)//录音模式
	{  
        if(Get_DCahceSta())SCB_CleanInvalidateDCache();
		if(DMA1_Stream6->CR&(1<<19))rec_sai_fifo_write(sairecbuf1);	//sairecbuf1写入FIFO
		else rec_sai_fifo_write(sairecbuf2);						//sairecbuf2写入FIFO 
	} 
}  


const u16 saiplaybuf[2]={0X0000,0X0000};//2个16位数据,用于录音时SAI Block A主机发送.循环发送0.
//进入PCM 录音模式 		  
void recoder_enter_rec_mode(void)
{
	WM8978_ADDA_Cfg(0,1);		//开启ADC
	WM8978_Input_Cfg(1,1,0);	//开启输入通道(MIC&LINE IN)
	WM8978_Output_Cfg(0,1);		//开启BYPASS输出 
	WM8978_MIC_Gain(46);		//MIC增益设置 
	WM8978_SPKvol_Set(0);		//关闭喇叭.
	WM8978_I2S_Cfg(2,0);		//飞利浦标准,16位数据长度

    SAIA_Init(SAI_MODEMASTER_TX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
    SAIB_Init(SAI_MODESLAVE_RX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
	SAIA_SampleRate_Set(REC_SAMPLERATE);//设置采样率 
	SAIA_TX_DMA_Init((u8*)&saiplaybuf[0],(u8*)&saiplaybuf[1],1,1);	//配置TX DMA,16位
    __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC); //关闭传输完成中断(这里不用中断送数据) 
	SAIA_RX_DMA_Init(sairecbuf1,sairecbuf2,SAI_RX_DMA_BUF_SIZE/2,1);//配置RX DMA
  	sai_rx_callback=rec_sai_dma_rx_callback;//初始化回调函数指sai_rx_callback
 	SAI_Play_Start();			//开始SAI数据发送(主机)
	SAI_Rec_Start(); 			//开始SAI数据接收(从机)
	recoder_remindmsg_show(0);
} 


//进入PCM 放音模式 		  
void recoder_enter_play_mode(void)
{
	WM8978_ADDA_Cfg(1,0);		//开启DAC 
	WM8978_Input_Cfg(0,0,0);	//关闭输入通道(MIC&LINE IN)
	WM8978_Output_Cfg(1,0);		//开启DAC输出 
	WM8978_MIC_Gain(0);			//MIC增益设置为0 
	WM8978_SPKvol_Set(50);		//喇叭音量设置
	SAI_Play_Stop();			//停止时钟发送
	SAI_Rec_Stop(); 			//停止录音
	recoder_remindmsg_show(1);
}
//初始化WAV头.
void recoder_wav_init(__WaveHeader* wavhead) //初始化WAV头			   
{
	wavhead->riff.ChunkID=0X46464952;	//"RIFF"
	wavhead->riff.ChunkSize=0;			//还未确定,最后需要计算
	wavhead->riff.Format=0X45564157; 	//"WAVE"
	wavhead->fmt.ChunkID=0X20746D66; 	//"fmt "
	wavhead->fmt.ChunkSize=16; 			//大小为16个字节
	wavhead->fmt.AudioFormat=0X01; 		//0X01,表示PCM;0X01,表示IMA ADPCM
 	wavhead->fmt.NumOfChannels=2;		//双声道
 	wavhead->fmt.SampleRate=REC_SAMPLERATE;//设置采样速率
 	wavhead->fmt.ByteRate=wavhead->fmt.SampleRate*4;//字节速率=采样率*通道数*(ADC位数/8)
 	wavhead->fmt.BlockAlign=4;			//块大小=通道数*(ADC位数/8)
 	wavhead->fmt.BitsPerSample=16;		//16位PCM
   	wavhead->data.ChunkID=0X61746164;	//"data"
 	wavhead->data.ChunkSize=0;			//数据大小,还需要计算  
} 						    
//显示录音时间和码率
//tsec:秒钟数.
void recoder_msg_show(u32 tsec,u32 kbps)
{   
	//显示录音时间			 
	LCD_ShowString(30,310,200,16,16,"TIME:");	  	  
	LCD_ShowxNum(30+40,310,tsec/60,2,16,0X80);	//分钟
	LCD_ShowChar(30+56,310,':',16,0);
	LCD_ShowxNum(30+64,310,tsec%60,2,16,0X80);	//秒钟	
	//显示码率		 
	LCD_ShowString(140,310,200,16,16,"KPBS:");	  	  
	LCD_ShowxNum(140+40,310,kbps/1000,4,16,0X80);	//码率显示 	
}  	
//提示信息
//mode:0,录音模式;1,放音模式
void recoder_remindmsg_show(u8 mode)
{
	LCD_Fill(30,220,lcddev.width-1,280,WHITE);//清除原来的显示
	POINT_COLOR=RED;
	if(mode==0)	//录音模式
	{
		Show_Str(30,220,200,16,"KEY0:REC/PAUSE",16,0); 
		Show_Str(30,240,200,16,"KEY2:STOP&SAVE",16,0); 
		Show_Str(30,260,200,16,"WK_UP:PLAY",16,0); 
	}else		//放音模式
	{
		Show_Str(30,220,200,16,"KEY0:STOP Play",16,0);  
		Show_Str(30,240,200,16,"WK_UP:PLAY/PAUSE",16,0); 
	}
}
 
void wav_recorder(void)
{ 
	u8 i;
    u8 u8IsAu08WaveCacheFull=0;
    unsigned int u32RobotLen;
	u8 key;
	u8 rval=0;
	__WaveHeader *wavhead=0; 
 	u8 *pname=0;
	u8 *pdatabuf;
    u8 * robotReplyPtr;
	u8 timecnt=0;					//计时器   
	u32 recsec=0;					//录音时间  
	sairecbuf1=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAI录音内存1申请
	sairecbuf2=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAI录音内存2申请

    //5MB
    u32WaveCacheLen=5000*1024;   
    au08WaveCache=mymalloc(SRAMEX, u32WaveCacheLen);//SAI录音内存2申请
    memset(au08WaveCache, 0, u32WaveCacheLen);
    
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
	{
		sairecfifobuf[i]=mymalloc(SRAMEX,SAI_RX_DMA_BUF_SIZE);//SAI录音FIFO内存申请
		if(sairecfifobuf[i]==NULL)
            break;			//申请失败
	}
 	wavhead=(__WaveHeader*)mymalloc(SRAMIN,sizeof(__WaveHeader));//开辟__WaveHeader字节的内存区域 
	if(!sairecbuf1||!sairecbuf2||!wavhead||i!=SAI_RX_FIFO_SIZE)
        rval=1; 	
    
	if(rval==0)		
	{
		recoder_enter_rec_mode();	//进入录音模式,此时耳机可以听到咪头采集到的音频   
		pname[0]=0;					//pname没有任何文件名 
 	   	while(rval==0)
		{
			key=KEY_Scan(0);
			switch(key)
			{		
				case KEY2_PRES:	//STOP&SAVE
					if(rec_sta&0X80)//有录音
					{
						rec_sta=0;	//关闭录音
						wavhead->riff.ChunkSize=wavsize+36;		//整个文件的大44-8=36;
				   		wavhead->data.ChunkSize=wavsize;		//数据大小
                        
                        /*********save data to RAM cache*********************/
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        /*********save data to RAM cache*********************/
                        
                        robotReplyPtr=sendWavDataToServer(au08WaveCache, wavsize+40, &u32RobotLen);
                        
                        /***********************************/
                        //copy the data to au08WaveCache
                        if(u32RobotLen>u32WaveCacheLen)
                        {
                            printf("error!!! u32RobotLen>u32WaveCacheLen \r\n");
                            printf("u32RobotLen is %d \r\n", u32RobotLen);
                            printf("u32WaveCacheLen is %d \r\n", u32WaveCacheLen);                         
                        }
                        else
                        {                            
                            memset(au08WaveCache, 0, u32WaveCacheLen);
                            memcpy(au08WaveCache, robotReplyPtr, u32RobotLen);
                            myfree(SRAMEX, robotReplyPtr);
                            recoder_enter_play_mode();	//进入播放模式
                 
                            wav_play_for_16bps(au08WaveCache);
                            __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC);    //关闭SAI发送DMA中断                        
                            printf("enter record mode r\n");                        
                            recoder_enter_rec_mode();	//重新进入录音模式   
                        }                            
                      
                        /********************************/
						wavsize=0;                        
						sairecfifordpos=0;	//FIFO读写位置重新归零
						sairecfifowrpos=0;
					}
					rec_sta=0;
					recsec=0;
				 	LED1(1);	 						//关闭DS1	      
					break;	
                
				case KEY0_PRES:	//REC/PAUSE
					
                    if(rec_sta&0X01)//原来是暂停,继续录音
					{
						rec_sta&=0XFE;//取消暂停
					}
                    else if(rec_sta&0X80)//已经在录音了,暂停
					{
						rec_sta|=0X01;	//暂停
					}
                    else				//还没开始录音 
					{
						recsec=0;	 
                         //clear "buff if full"
                        LCD_Fill(0, 280, 271 , 296, WHITE);	
						Show_Str(30,280, 100, 16, "recorder", 16,0);		   
				 		recoder_wav_init(wavhead);				//初始化wav数据	
                 
                        /*********save data to RAM cache*********************/
                        memset(au08WaveCache, 0, u32WaveCacheLen);
                        u32WaveCacheOfs=0;
                        u8IsAu08WaveCacheFull=0;
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        u32WaveCacheOfs+=sizeof(__WaveHeader);
                        rec_sta|=0X80;	//开始录音	 
                        /*********save data to RAM cache**********************/                         
 					}
                    
					if(rec_sta&0X01)
                        LED1(0);	//提示正在暂停
					else 
                        LED1(1);
					break;  
                    
				case WKUP_PRES:	//播放最近一段录音
					if(rec_sta!=0X80)//没有在录音
					{   	  				  
                        Show_Str(30,280,lcddev.width,16,"play:",16,0);		   
                        LCD_Fill(30,280,lcddev.width-1,300,WHITE); 
                        recoder_enter_play_mode();	//进入播放模式
                        printf("播放音频:%s\r\n",pname);
                        wav_play_for_16bps(au08WaveCache);
                        __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC);    //关闭SAI发送DMA中断                         
                        LCD_Fill(30,280,lcddev.width-1,lcddev.height-1,WHITE);  //清除显示,清除之前显示的录音文件名	            
                        printf("重新进入录音模式\r\n");                        
                        recoder_enter_rec_mode();	//重新进入录音模式
					}
					break;
			}//end of switch(key)
            
            if(u8IsAu08WaveCacheFull==0)
            {
                if(rec_sai_fifo_read(&pdatabuf))//读取一次数据,读到数据了,写入文件
                {
                    if((u32WaveCacheOfs+SAI_RX_DMA_BUF_SIZE) >=u32WaveCacheLen)
                    {
                        //buffer full
                         u8IsAu08WaveCacheFull=1;
                         Show_Str(30,280,lcddev.width,16,"recoder buffer is full",16,0);	
                         printf(" au08WaveCache buffer is full  \r\n");
                    }
                    
                    /*********save data to RAM cache*********************/
                    memcpy(&au08WaveCache[u32WaveCacheOfs],pdatabuf , SAI_RX_DMA_BUF_SIZE);
                    u32WaveCacheOfs+=SAI_RX_DMA_BUF_SIZE;
                    /*********save data to RAM cache*********************/
                 
                    wavsize+=SAI_RX_DMA_BUF_SIZE;				
                }
                else 
                {
                    delay_ms(5);
                }
            }//end  if(u8IsAu08WaveCacheFull==0)
            
			timecnt++;
			if((timecnt%20)==0)
                LED0_Toggle;//DS0闪烁  
            
 			if(recsec!=(wavsize/wavhead->fmt.ByteRate))	//录音时间显示
			{	   
				LED0_Toggle;//DS0闪烁 
				recsec=wavsize/wavhead->fmt.ByteRate;	//录音时间
				printf("录音时间为：%d\r\n",recsec);
				recoder_msg_show(recsec,wavhead->fmt.SampleRate*wavhead->fmt.NumOfChannels*wavhead->fmt.BitsPerSample);//显示码率
			}
		}		 
	}    
    
	myfree(SRAMIN,sairecbuf1);	//释放内存
	myfree(SRAMIN,sairecbuf2);	//释放内存  
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
        myfree(SRAMIN,sairecfifobuf[i]);//SAI录音FIFO内存释放
    
	myfree(SRAMIN,wavhead);		//释放内存  
	myfree(SRAMIN,pname);		//释放内存  
    myfree(SRAMEX, au08WaveCache);
}


