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
  
u8 *sairecbuf1;			//SAI1 DMA����BUF1
u8 *sairecbuf2; 		//SAI1 DMA����BUF2
u8 *au08WaveCache;

//REC¼��FIFO�������.
//����FATFS�ļ�д��ʱ��Ĳ�ȷ����,���ֱ���ڽ����ж�����д�ļ�,���ܵ���ĳ��д��ʱ�����
//�Ӷ��������ݶ�ʧ,�ʼ���FIFO����,�Խ��������.
vu8 sairecfifordpos=0;	//FIFO��λ��
vu8 sairecfifowrpos=0;	//FIFOдλ��
u8 *sairecfifobuf[SAI_RX_FIFO_SIZE];//����10��¼������FIFO

//FIL* f_rec=0;			//¼���ļ�	
u32 wavsize;			//wav���ݴ�С(�ֽ���,�������ļ�ͷ!!)
u8 rec_sta=0;			//¼��״̬
						//[7]:0,û�п���¼��;1,�Ѿ�����¼��;
						//[6:1]:����
						//[0]:0,����¼��;1,��ͣ¼��;
                        
u32 u32WaveCacheLen=0;
u32 u32WaveCacheOfs=0;
u32 u32ReadLen=0;
extern u8 wav_play_for_16bps(u8 * au08WaveData);
extern void sendWavDataToServer(unsigned char * audio_data, unsigned int u32SpeechDataLen);

//��ȡ¼��FIFO
//buf:���ݻ������׵�ַ
//����ֵ:0,û�����ݿɶ�;
//      1,������1�����ݿ�
u8 rec_sai_fifo_read(u8 **buf)
{
	if(sairecfifordpos==sairecfifowrpos)
        return 0;
	
    sairecfifordpos++;		//��λ�ü�1
	
    if(sairecfifordpos>=SAI_RX_FIFO_SIZE)
        sairecfifordpos=0;//���� 
	
    *buf=sairecfifobuf[sairecfifordpos];
    
	return 1;
}
//дһ��¼��FIFO
//buf:���ݻ������׵�ַ
//����ֵ:0,д��ɹ�;
//      1,д��ʧ��
u8 rec_sai_fifo_write(u8 *buf)
{
	u16 i;
	u8 temp=sairecfifowrpos;//��¼��ǰдλ��
	sairecfifowrpos++;		//дλ�ü�1
	if(sairecfifowrpos>=SAI_RX_FIFO_SIZE)sairecfifowrpos=0;//����  
	if(sairecfifordpos==sairecfifowrpos)
	{
		sairecfifowrpos=temp;//��ԭԭ����дλ��,�˴�д��ʧ��
		return 1;	
	}
    
	for(i=0;i<SAI_RX_DMA_BUF_SIZE;i++)
        sairecfifobuf[sairecfifowrpos][i]=buf[i];//��������
    
	return 0;
} 

//¼�� SAI_DMA�����жϷ�����.���ж�����д������
void rec_sai_dma_rx_callback(void) 
{      
	if(rec_sta==0X80)//¼��ģʽ
	{  
        if(Get_DCahceSta())SCB_CleanInvalidateDCache();
		if(DMA1_Stream6->CR&(1<<19))rec_sai_fifo_write(sairecbuf1);	//sairecbuf1д��FIFO
		else rec_sai_fifo_write(sairecbuf2);						//sairecbuf2д��FIFO 
	} 
}  


const u16 saiplaybuf[2]={0X0000,0X0000};//2��16λ����,����¼��ʱSAI Block A��������.ѭ������0.
//����PCM ¼��ģʽ 		  
void recoder_enter_rec_mode(void)
{
	WM8978_ADDA_Cfg(0,1);		//����ADC
	WM8978_Input_Cfg(1,1,0);	//��������ͨ��(MIC&LINE IN)
	WM8978_Output_Cfg(0,1);		//����BYPASS��� 
	WM8978_MIC_Gain(46);		//MIC�������� 
	WM8978_SPKvol_Set(0);		//�ر�����.
	WM8978_I2S_Cfg(2,0);		//�����ֱ�׼,16λ���ݳ���

    SAIA_Init(SAI_MODEMASTER_TX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
    SAIB_Init(SAI_MODESLAVE_RX,SAI_CLOCKSTROBING_RISINGEDGE,SAI_DATASIZE_16);
	SAIA_SampleRate_Set(REC_SAMPLERATE);//���ò����� 
	SAIA_TX_DMA_Init((u8*)&saiplaybuf[0],(u8*)&saiplaybuf[1],1,1);	//����TX DMA,16λ
    __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC); //�رմ�������ж�(���ﲻ���ж�������) 
	SAIA_RX_DMA_Init(sairecbuf1,sairecbuf2,SAI_RX_DMA_BUF_SIZE/2,1);//����RX DMA
  	sai_rx_callback=rec_sai_dma_rx_callback;//��ʼ���ص�����ָsai_rx_callback
 	SAI_Play_Start();			//��ʼSAI���ݷ���(����)
	SAI_Rec_Start(); 			//��ʼSAI���ݽ���(�ӻ�)
	recoder_remindmsg_show(0);
} 


//����PCM ����ģʽ 		  
void recoder_enter_play_mode(void)
{
	WM8978_ADDA_Cfg(1,0);		//����DAC 
	WM8978_Input_Cfg(0,0,0);	//�ر�����ͨ��(MIC&LINE IN)
	WM8978_Output_Cfg(1,0);		//����DAC��� 
	WM8978_MIC_Gain(0);			//MIC��������Ϊ0 
	WM8978_SPKvol_Set(50);		//������������
	SAI_Play_Stop();			//ֹͣʱ�ӷ���
	SAI_Rec_Stop(); 			//ֹͣ¼��
	recoder_remindmsg_show(1);
}
//��ʼ��WAVͷ.
void recoder_wav_init(__WaveHeader* wavhead) //��ʼ��WAVͷ			   
{
	wavhead->riff.ChunkID=0X46464952;	//"RIFF"
	wavhead->riff.ChunkSize=0;			//��δȷ��,�����Ҫ����
	wavhead->riff.Format=0X45564157; 	//"WAVE"
	wavhead->fmt.ChunkID=0X20746D66; 	//"fmt "
	wavhead->fmt.ChunkSize=16; 			//��СΪ16���ֽ�
	wavhead->fmt.AudioFormat=0X01; 		//0X01,��ʾPCM;0X01,��ʾIMA ADPCM
 	wavhead->fmt.NumOfChannels=2;		//˫����
 	wavhead->fmt.SampleRate=REC_SAMPLERATE;//���ò�������
 	wavhead->fmt.ByteRate=wavhead->fmt.SampleRate*4;//�ֽ�����=������*ͨ����*(ADCλ��/8)
 	wavhead->fmt.BlockAlign=4;			//���С=ͨ����*(ADCλ��/8)
 	wavhead->fmt.BitsPerSample=16;		//16λPCM
   	wavhead->data.ChunkID=0X61746164;	//"data"
 	wavhead->data.ChunkSize=0;			//���ݴ�С,����Ҫ����  
} 						    
//��ʾ¼��ʱ�������
//tsec:������.
void recoder_msg_show(u32 tsec,u32 kbps)
{   
	//��ʾ¼��ʱ��			 
	LCD_ShowString(30,310,200,16,16,"TIME:");	  	  
	LCD_ShowxNum(30+40,310,tsec/60,2,16,0X80);	//����
	LCD_ShowChar(30+56,310,':',16,0);
	LCD_ShowxNum(30+64,310,tsec%60,2,16,0X80);	//����	
	//��ʾ����		 
	LCD_ShowString(140,310,200,16,16,"KPBS:");	  	  
	LCD_ShowxNum(140+40,310,kbps/1000,4,16,0X80);	//������ʾ 	
}  	
//��ʾ��Ϣ
//mode:0,¼��ģʽ;1,����ģʽ
void recoder_remindmsg_show(u8 mode)
{
	LCD_Fill(30,220,lcddev.width-1,280,WHITE);//���ԭ������ʾ
	POINT_COLOR=RED;
	if(mode==0)	//¼��ģʽ
	{
		Show_Str(30,220,200,16,"KEY0:REC/PAUSE",16,0); 
		Show_Str(30,240,200,16,"KEY2:STOP&SAVE",16,0); 
		Show_Str(30,260,200,16,"WK_UP:PLAY",16,0); 
	}else		//����ģʽ
	{
		Show_Str(30,220,200,16,"KEY0:STOP Play",16,0);  
		Show_Str(30,240,200,16,"WK_UP:PLAY/PAUSE",16,0); 
	}
}
 

//WAV¼�� 
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
	u8 timecnt=0;					//��ʱ��   
	u32 recsec=0;					//¼��ʱ�� 
	sairecbuf1=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAI¼���ڴ�1����
	sairecbuf2=mymalloc(SRAMIN,SAI_RX_DMA_BUF_SIZE);//SAI¼���ڴ�2����

    //5MB
    u32WaveCacheLen=2000*1024;   
    au08WaveCache=mymalloc(SRAMEX, u32WaveCacheLen);//SAI¼���ڴ�2����
    memset(au08WaveCache, 0, u32WaveCacheLen);
    
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
	{
		sairecfifobuf[i]=mymalloc(SRAMEX,SAI_RX_DMA_BUF_SIZE);//SAI¼��FIFO�ڴ�����
		if(sairecfifobuf[i]==NULL)
            break;			//����ʧ��
	}
 	wavhead=(__WaveHeader*)mymalloc(SRAMIN,sizeof(__WaveHeader));//����__WaveHeader�ֽڵ��ڴ����� 
	if(!sairecbuf1||!sairecbuf2||!wavhead||i!=SAI_RX_FIFO_SIZE)
        rval=1; 	
    
	if(rval==0)		
	{
		recoder_enter_rec_mode();	//����¼��ģʽ,��ʱ��������������ͷ�ɼ�������Ƶ   
		pname[0]=0;					//pnameû���κ��ļ��� 
 	   	while(rval==0)
		{
			key=KEY_Scan(0);
			switch(key)
			{		
				case KEY2_PRES:	//STOP&SAVE
					if(rec_sta&0X80)//��¼��
					{
						rec_sta=0;	//�ر�¼��
						wavhead->riff.ChunkSize=wavsize+36;		//�����ļ��Ĵ�44-8=36;
				   		wavhead->data.ChunkSize=wavsize;		//���ݴ�С
                        
                        /*********save data to RAM cache*********************/
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        /*********save data to RAM cache*********************/
                        
                        sendWavDataToServer(au08WaveCache, wavsize+40);
                                
						wavsize=0;
						sairecfifordpos=0;	//FIFO��дλ�����¹���
						sairecfifowrpos=0;
					}
					rec_sta=0;
					recsec=0;
				 	LED1(1);	 						//�ر�DS1���		      
					break;	
             
				case KEY0_PRES:	//REC/PAUSE
					
                    if(rec_sta&0X01)//ԭ������ͣ,����¼��
					{
						rec_sta&=0XFE;//ȡ����ͣ
					}
                    else if(rec_sta&0X80)//�Ѿ���¼����,��ͣ
					{
						rec_sta|=0X01;	//��ͣ
					}
                    else				//��û��ʼ¼�� 
					{
						recsec=0;	 
                        //clear "buff if full"
                        LCD_Fill(0, 280, 271 , 296, WHITE);	
						Show_Str(30,280, 271, 16, "recorder start ...", 16,0);		   
				 		recoder_wav_init(wavhead);				//��ʼ��wav����	
                 
                        /*********save data to RAM cache*********************/
                        memset(au08WaveCache, 0, u32WaveCacheLen);
                        u32WaveCacheOfs=0;
                        u8IsAu08WaveCacheFull=0;
                        memcpy(au08WaveCache,wavhead,sizeof(__WaveHeader));
                        u32WaveCacheOfs+=sizeof(__WaveHeader);
                        rec_sta|=0X80;	//��ʼ¼��	 
                        /*********save data to RAM cache**********************/                         
 					}
                    
					if(rec_sta&0X01)
                        LED1(0);	//��ʾ������ͣ
					else 
                        LED1(1);
					break;  
                    
				case WKUP_PRES:	//�������һ��¼��
					if(rec_sta!=0X80)//û����¼��
					{   	 				  
                        LCD_Fill(30,280,260,300,WHITE); 
                        Show_Str(30,280,lcddev.width,16,"play:",16,0);		   
//                        LCD_Fill(30,280,lcddev.width-1,300,WHITE); 
                        recoder_enter_play_mode();	//���벥��ģʽ
                        printf("������Ƶ:%s\r\n",pname);
                        wav_play_for_16bps(au08WaveCache);
                        __HAL_DMA_DISABLE_IT(&SAI1_TXDMA_Handler,DMA_IT_TC);    //�ر�SAI����DMA�ж�                         
//                        LCD_Fill(30,280,lcddev.width-1,lcddev.height-1,WHITE);  //�����ʾ,���֮ǰ��ʾ��¼���ļ���	            
                        printf("���½���¼��ģʽ\r\n");                        
                        recoder_enter_rec_mode();	//���½���¼��ģʽ
					}
					break;
			}//end of switch(key)
            
            if(u8IsAu08WaveCacheFull==0)
            {
                if(rec_sai_fifo_read(&pdatabuf))//��ȡһ������,����������,д���ļ�
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
                LED0_Toggle;//DS0��˸  
            
 			if(recsec!=(wavsize/wavhead->fmt.ByteRate))	//¼��ʱ����ʾ
			{	   
				LED0_Toggle;//DS0��˸ 
				recsec=wavsize/wavhead->fmt.ByteRate;	//¼��ʱ��
				printf("¼��ʱ��Ϊ��%d\r\n",recsec);
				recoder_msg_show(recsec,wavhead->fmt.SampleRate*wavhead->fmt.NumOfChannels*wavhead->fmt.BitsPerSample);//��ʾ����
			}
		}		 
	}    
    
	myfree(SRAMIN,sairecbuf1);	//�ͷ��ڴ�
	myfree(SRAMIN,sairecbuf2);	//�ͷ��ڴ�  
	for(i=0;i<SAI_RX_FIFO_SIZE;i++)
        myfree(SRAMIN,sairecfifobuf[i]);//SAI¼��FIFO�ڴ��ͷ�
    
	myfree(SRAMIN,wavhead);		//�ͷ��ڴ�  
	myfree(SRAMIN,pname);		//�ͷ��ڴ�  
}




















