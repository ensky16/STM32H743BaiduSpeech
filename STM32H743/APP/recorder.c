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
//±¾³ÌĞòÖ»¹©Ñ§Ï°Ê¹ÓÃ£¬Î´¾­×÷ÕßĞí¿É£¬²»µÃÓÃÓÚÆäËüÈÎºÎÓÃÍ¾
//ALIENTEK STM32¿ª·¢°å
//Â¼Òô»ú Ó¦ÓÃ´úÂë	   
//ÕıµãÔ­×Ó@ALIENTEK
//¼¼ÊõÂÛÌ³:www.openedv.com
//´´½¨ÈÕÆÚ:2016/1/11
//°æ±¾£ºV1.0
//°æÈ¨ËùÓĞ£¬µÁ°æ±Ø¾¿¡£
//Copyright(C) ¹ãÖİÊĞĞÇÒíµç×Ó¿Æ¼¼ÓĞÏŞ¹«Ë¾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
  
u8 *sairecbuf1;			//SAI1 DMA½ÓÊÕBUF1
u8 *sairecbuf2; 		//SAI1 DMA½ÓÊÕBUF2
u8 *au08WaveCache;

//RECÂ¼ÒôFIFO¹ÜÀí²ÎÊı.
//ÓÉÓÚFATFSÎÄ¼şĞ´ÈëÊ±¼äµÄ²»È·¶¨ĞÔ,Èç¹ûÖ±½ÓÔÚ½ÓÊÕÖĞ¶ÏÀïÃæĞ´ÎÄ¼ş,¿ÉÄÜµ¼ÖÂÄ³´ÎĞ´ÈëÊ±¼ä¹ı³¤
//´Ó¶øÒıÆğÊı¾İ¶ªÊ§,¹Ê¼ÓÈëFIFO¿ØÖÆ,ÒÔ½â¾ö´ËÎÊÌâ.
vu8 sairecfifordpos=0;	//FIFO¶ÁÎ»ÖÃ
vu8 sairecfifowrpos=0;	//FIFOĞ´Î»ÖÃ
u8 *sairecfifobuf[SAI_RX_FIFO_SIZE];//¶¨Òå10¸öÂ¼Òô½ÓÊÕFIFO

//FIL* f_rec=0;			//Â¼ÒôÎÄ¼ş	
u32 wavsize;			//wavÊı¾İ´óĞ¡(×Ö½ÚÊı,²»°üÀ¨ÎÄ¼şÍ·!!)
u8 rec_sta=0;			//Â¼Òô×´Ì¬
						//[7]:0,Ã»ÓĞ¿ªÆôÂ¼Òô;1,ÒÑ¾­¿ªÆôÂ¼Òô;
						//[6:1]:±£Áô
						//[0]:0,ÕıÔÚÂ¼Òô;1,ÔİÍ£Â¼Òô;
                        
u32 u32WaveCacheLen=0;
u32 u32WaveCacheOfs=0;
u32 u32ReadLen=0;
extern u8 wav_play_for_16bps(u8 * au08WaveData);
extern void sendWavDataToServer(unsigned char * audio_data, unsigned int u32SpeechDataLen);

//¶ÁÈ¡Â¼ÒôFIFO
//buf:Êı¾İ»º´æÇøÊ×µØÖ·
//·µ»ØÖµ:0,Ã»ÓĞÊı¾İ¿É¶Á;
//      1,¶Áµ½ÁË1¸öÊı¾İ¿é
u8 rec_sai_fifo_read(u8 **buf)
{
	if(sairecfifordpos==sairecfifowrpos)
        return 0;
	
    sairecfifordpos++;		//¶ÁÎ»ÖÃ¼Ó1
	
    if(sairecfifordpos>=SAI_RX_FIFO_SIZE)
        sairecfifordpos=0;//¹éÁã 
	
    *buf=sairecfifobuf[sairecfifordpos];
    
	return 1;
}
//Ğ´Ò»¸öÂ¼ÒôFIFO
//buf:Êı¾İ»º´æÇøÊ×µØÖ·
//·µ»ØÖµ:0,Ğ´Èë³É¹¦;
//      1,Ğ´ÈëÊ§°Ü
u8 rec_sai_fifo_write(u8 *buf)
{
	u16 i;
	u8 temp=sairecfifowrpos;//¼ÇÂ¼µ±Ç°Ğ´Î»ÖÃ
	sairecfifowrpos++;		//Ğ´Î»ÖÃ¼Ó1
	if(sairecfifowrpos>=SAI_RX_FIFO_SIZE)sairecfifowrpos=0;//¹éÁã  
	if(sairecfifordpos==sairecfifowrpos)
	{
		sairecfifowrpos=temp;//»¹Ô­Ô­À´µÄĞ´Î»ÖÃ,´Ë´ÎĞ´ÈëÊ§°Ü
		return 1;	
	}
    
	for(i=0;i<SAI_RX_DMA_BUF_SIZE;i++)
        sairecfifobuf[sairecfifowrpos][i]=buf[i];//¿½±´Êı¾İ
    
	return 0;
} 

//Â¼Òô SAI_DMA½ÓÊÕÖĞ¶Ï·şÎñº¯Êı.ÔÚÖĞ¶ÏÀïÃæĞ´ÈëÊı¾İ
void rec_sai_dma_rx_callback(void) 
{      
	if(rec_sta==0X80)//Â¼ÒôÄ£Ê½
	{  
        if(Get_DCahceSta())SCB_CleanInvalidateDCache();
		if(DMA1_Stream6->CR&(1<<19))rec_sai_fifo_write(sairecbuf1);	//sairecbuf1Ğ´ÈëFIFO
		else rec_sai_fifo_write(sairecbuf2);						//sairecbuf2Ğ´ÈëFIFO 
	} 
}  


const u16 saiplaybuf[2]={0X0000,0X0000};//2¸ö16Î»Êı¾İ,ÓÃÓÚÂ¼ÒôÊ±SAI Block AÖ÷»ú·¢ËÍ.Ñ­»··¢ËÍ0.
//½øÈëPCM Â¼ÒôÄ£Ê½ 		  
void recoder_enter_rec_mode(void)
{
	WM8978_ADDA_Cfg(0,1);		//¿ªÆôADC
	WM8978_Input_Cfg(1,1,0);	//¿ªÆôÊäÈëÍ¨µÀ(MIC&LINE IN)
	WM8978_Output_Cfg(0,1);		//¿ªÆôBYPASSÊä³ö 
	WM8978_MIC_Gain(46);		//MICÔöÒæÉèÖÃ 
	WM8978_SPKvol_Set(0);		//¹Ø±ÕÀ®°È.
	WM8978_I2S_Cfg(2,0);		//·ÉÀûÆÖ±ê×¼,16Î»Êı¾İ³¤¶È

    SAIA_Init(SAI_MODEMASTER_TX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
    SAIB_Init(SAI_MODESLAVE_RX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
	SAIA_SampleRate_Set(REC_SAMPLERATE);//ÉèÖÃ²ÉÑùÂÊ 
	SAIA_TX_DMA_Init((u8*)&saiplaybuf[0],(u8*)&saiplaybuf[1],1,1);	//ÅäÖÃTX DMA,16Î»
    __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC); //¹Ø±Õ´«ÊäÍê³ÉÖĞ¶Ï(ÕâÀï²»ÓÃÖĞ¶ÏËÍÊı¾İ) 
	SAIA_RX_DMA_Init(sairecbuf1,sairecbuf2,SAI_RX_DMA_BUF_SIZE/2,1);//ÅäÖÃRX DMA
  	sai_rx_callback=rec_sai_dma_rx_callback;//³õÊ¼»¯»Øµ÷º¯ÊıÖ¸sai_rx_callback
 	SAI_Play_Start();			//¿ªÊ¼SAIÊı¾İ·¢ËÍ(Ö÷»ú)
	SAI_Rec_Start(); 			//¿ªÊ¼SAIÊı¾İ½ÓÊÕ(´Ó»ú)
	recoder_remindmsg_show(0);
} 


//½øÈëPCM ·ÅÒôÄ£Ê½ 		  
void recoder_enter_play_mode(void)
{
	WM8978_ADDA_Cfg(1,0);		//¿ªÆôDAC 
	WM8978_Input_Cfg(0,0,0);	//¹Ø±ÕÊäÈëÍ¨µÀ(MIC&LINE IN)
	WM8978_Output_Cfg(1,0);		//¿ªÆôDACÊä³ö 
	WM8978_MIC_Gain(0);			//MICÔöÒæÉèÖÃÎª0 
	WM8978_SPKvol_Set(50);		//À®°ÈÒôÁ¿ÉèÖÃ
	SAI_Play_Stop();			//Í£Ö¹Ê±ÖÓ·¢ËÍ
	SAI_Rec_Stop(); 			//Í£Ö¹Â¼Òô
	recoder_remindmsg_show(1);
}
//³õÊ¼»¯WAVÍ·.
void recoder_wav_init(__WaveHeader* wavhead) //³õÊ¼»¯WAVÍ·			   
{
	wavhead->riff.ChunkID=0X46464952;	//"RIFF"
	wavhead->riff.ChunkSize=0;			//»¹Î´È·¶¨,×îºóĞèÒª¼ÆËã
	wavhead->riff.Format=0X45564157; 	//"WAVE"
	wavhead->fmt.ChunkID=0X20746D66; 	//"fmt "
	wavhead->fmt.ChunkSize=16; 			//´óĞ¡Îª16¸ö×Ö½Ú
	wavhead->fmt.AudioFormat=0X01; 		//0X01,±íÊ¾PCM;0X01,±íÊ¾IMA ADPCM
 	wavhead->fmt.NumOfChannels=2;		//Ë«ÉùµÀ
 	wavhead->fmt.SampleRate=REC_SAMPLERATE;//ÉèÖÃ²ÉÑùËÙÂÊ
 	wavhead->fmt.ByteRate=wavhead->fmt.SampleRate*4;//×Ö½ÚËÙÂÊ=²ÉÑùÂÊ*Í¨µÀÊı*(ADCÎ»Êı/8)
 	wavhead->fmt.BlockAlign=4;			//¿é´óĞ¡=Í¨µÀÊı*(ADCÎ»Êı/8)
 	wavhead->fmt.BitsPerSample=16;		//16Î»PCM
   	wavhead->data.ChunkID=0X61746164;	//"data"
 	wavhead->data.ChunkSize=0;			//Êı¾İ´óĞ¡,»¹ĞèÒª¼ÆËã  
} 						    
//ÏÔÊ¾Â¼ÒôÊ±¼äºÍÂëÂÊ
//tsec:ÃëÖÓÊı.
void recoder_msg_show(u32 tsec,u32 kbps)
{   
	//ÏÔÊ¾Â¼ÒôÊ±¼ä			 
	LCD_ShowString(30,310,200,16,16,"TIME:");	  	  
	LCD_ShowxNum(30+40,310,tsec/60,2,16,0X80);	//·ÖÖÓ
	LCD_ShowChar(30+56,310,':',16,0);
	LCD_ShowxNum(30+64,310,tsec%60,2,16,0X80);	//ÃëÖÓ	
	//ÏÔÊ¾ÂëÂÊ		 
	LCD_ShowString(140,310,200,16,16,"KPBS:");	  	  
	LCD_ShowxNum(140+40,310,kbps/1000,4,16,0X80);	//ÂëÂÊÏÔÊ¾ 	
}  	
//ÌáÊ¾ĞÅÏ¢
//mode:0,Â¼ÒôÄ£Ê½;1,·ÅÒôÄ£Ê½
void recoder_remindmsg_show(u8 mode)
{
	LCD_Fill(30,220,lcddev.width-1,280,WHITE);//Çå³ıÔ­À´µÄÏÔÊ¾
	POINT_COLOR=RED;
	if(mode==0)	//Â¼ÒôÄ£Ê½
	{
		Show_Str(30,220,200,16,"KEY0:REC/PAUSE",16,0); 
		Show_Str(30,240,200,16,"KEY2:STOP&SAVE",16,0); 
		Show_Str(30,260,200,16,"WK_UP:PLAY",16,0); 
	}else		//·ÅÒôÄ£Ê½
	{
		Show_Str(30,220,200,16,"KEY0:STOP Play",16,0);  
		Show_Str(30,240,200,16,"WK_UP:PLAY/PAUSE",16,0); 
	}
}
 

//WAVÂ¼Òô 
void wav_recorder(void)
{ 
//    u32 k=0;
	u8 res,i;
    u8 u8IsAu08WaveCacheFull=0;
	u8 key;
	u8 rval=0;
	__WaveHeader *wavhead=0; 
 	u8 *pname=0;
	u8 *pdatabuf;
	u8 timecnt=0;					//¼ÆÊ±Æ÷   
	u32 recsec=0;					//Â¼ÒôÊ±¼ä 
	sairecbuf1=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAIÂ¼ÒôÄÚ´æ1ÉêÇë
	sairecbuf2=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAIÂ¼ÒôÄÚ´æ2ÉêÇë

    //5MB
    u32WaveCacheLen=2000*1024;   
    au08WaveCache=mymalloc(SRAMEX, u32WaveCacheLen);//SAIÂ¼ÒôÄÚ´æ2ÉêÇë
    memset(au08WaveCache, 0, u32WaveCacheLen);
    
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
	{
		sairecfifobuf[i]=mymalloc(SRAMEX,SAI_RX_DMA_BUF_SIZE);//SAIÂ¼ÒôFIFOÄÚ´æÉêÇë
		if(sairecfifobuf[i]==NULL)
            break;			//ÉêÇëÊ§°Ü
	}
 	wavhead=(__WaveHeader*)mymalloc(SRAMIN,sizeof(__WaveHeader));//¿ª±Ù__WaveHeader×Ö½ÚµÄÄÚ´æÇøÓò 
	if(!sairecbuf1||!sairecbuf2||!wavhead||i!=SAI_RX_FIFO_SIZE)
        rval=1; 	
    
	if(rval==0)		
	{
		recoder_enter_rec_mode();	//½øÈëÂ¼ÒôÄ£Ê½,´ËÊ±¶ú»ú¿ÉÒÔÌıµ½ßäÍ·²É¼¯µ½µÄÒôÆµ   
		pname[0]=0;					//pnameÃ»ÓĞÈÎºÎÎÄ¼şÃû 
 	   	while(rval==0)
		{
			key=KEY_Scan(0);
			switch(key)
			{		
				case KEY2_PRES:	//STOP&SAVE
					if(rec_sta&0X80)//ÓĞÂ¼Òô
					{
						rec_sta=0;	//¹Ø±ÕÂ¼Òô
						wavhead->riff.ChunkSize=wavsize+36;		//Õû¸öÎÄ¼şµÄ´ó44-8=36;
				   		wavhead->data.ChunkSize=wavsize;		//Êı¾İ´óĞ¡
                        
                        /*********save data to RAM cache*********************/
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        /*********save data to RAM cache*********************/
                        
                        sendWavDataToServer(au08WaveCache, wavsize+40);
                                
						wavsize=0;
						sairecfifordpos=0;	//FIFO¶ÁĞ´Î»ÖÃÖØĞÂ¹éÁã
						sairecfifowrpos=0;
					}
					rec_sta=0;
					recsec=0;
				 	LED1(1);	 						//¹Ø±ÕDS1şÃû		      
					break;	
             
				case KEY0_PRES:	//REC/PAUSE
					
                    if(rec_sta&0X01)//Ô­À´ÊÇÔİÍ£,¼ÌĞøÂ¼Òô
					{
						rec_sta&=0XFE;//È¡ÏûÔİÍ£
					}
                    else if(rec_sta&0X80)//ÒÑ¾­ÔÚÂ¼ÒôÁË,ÔİÍ£
					{
						rec_sta|=0X01;	//ÔİÍ£
					}
                    else				//»¹Ã»¿ªÊ¼Â¼Òô 
					{
						recsec=0;	 
                        //clear "buff if full"
                        LCD_Fill(0, 280, 271 , 296, WHITE);	
						Show_Str(30,280, 271, 16, "recorder start ...", 16,0);		   
				 		recoder_wav_init(wavhead);				//³õÊ¼»¯wavÊı¾İ	
                 
                        /*********save data to RAM cache*********************/
                        memset(au08WaveCache, 0, u32WaveCacheLen);
                        u32WaveCacheOfs=0;
                        u8IsAu08WaveCacheFull=0;
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        u32WaveCacheOfs+=sizeof(__WaveHeader);
                        rec_sta|=0X80;	//¿ªÊ¼Â¼Òô	 
                        /*********save data to RAM cache**********************/                         
 					}
                    
					if(rec_sta&0X01)
                        LED1(0);	//ÌáÊ¾ÕıÔÚÔİÍ£
					else 
                        LED1(1);
					break;  
                    
				case WKUP_PRES:	//²¥·Å×î½üÒ»¶ÎÂ¼Òô
					if(rec_sta!=0X80)//Ã»ÓĞÔÚÂ¼Òô
					{   	 				  
                        LCD_Fill(30,280,260,300,WHITE); 
                        Show_Str(30,280,lcddev.width,16,"play:",16,0);		   
//                        LCD_Fill(30,280,lcddev.width-1,300,WHITE); 
                        recoder_enter_play_mode();	//½øÈë²¥·ÅÄ£Ê½
                        printf("²¥·ÅÒôÆµ:%s\r\n",pname);
                        wav_play_for_16bps(au08WaveCache);
                        __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC);    //¹Ø±ÕSAI·¢ËÍDMAÖĞ¶Ï                         
//                        LCD_Fill(30,280,lcddev.width-1,lcddev.height-1,WHITE);  //Çå³ıÏÔÊ¾,Çå³ıÖ®Ç°ÏÔÊ¾µÄÂ¼ÒôÎÄ¼şÃû	            
                        printf("ÖØĞÂ½øÈëÂ¼ÒôÄ£Ê½\r\n");                        
                        recoder_enter_rec_mode();	//ÖØĞÂ½øÈëÂ¼ÒôÄ£Ê½
					}
					break;
			}//end of switch(key)
            
            if(u8IsAu08WaveCacheFull==0)
            {
                if(rec_sai_fifo_read(&pdatabuf))//¶ÁÈ¡Ò»´ÎÊı¾İ,¶Áµ½Êı¾İÁË,Ğ´ÈëÎÄ¼ş
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
                LED0_Toggle;//DS0ÉÁË¸  
            
 			if(recsec!=(wavsize/wavhead->fmt.ByteRate))	//Â¼ÒôÊ±¼äÏÔÊ¾
			{	   
				LED0_Toggle;//DS0ÉÁË¸ 
				recsec=wavsize/wavhead->fmt.ByteRate;	//Â¼ÒôÊ±¼ä
				printf("Â¼ÒôÊ±¼äÎª£º%d\r\n",recsec);
				recoder_msg_show(recsec,wavhead->fmt.SampleRate*wavhead->fmt.NumOfChannels*wavhead->fmt.BitsPerSample);//ÏÔÊ¾ÂëÂÊ
			}
		}		 
	}    
    
	myfree(SRAMIN,sairecbuf1);	//ÊÍ·ÅÄÚ´æ
	myfree(SRAMIN,sairecbuf2);	//ÊÍ·ÅÄÚ´æ  
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
        myfree(SRAMIN,sairecfifobuf[i]);//SAIÂ¼ÒôFIFOÄÚ´æÊÍ·Å
    
	myfree(SRAMIN,wavhead);		//ÊÍ·ÅÄÚ´æ  
	myfree(SRAMIN,pname);		//ÊÍ·ÅÄÚ´æ  
}




















