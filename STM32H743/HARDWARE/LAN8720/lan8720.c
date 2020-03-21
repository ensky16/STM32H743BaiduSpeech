#include "lan8720.h"
#include "lwip_comm.h"
#include "pcf8574.h"
#include "delay.h"
#include "usart.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32H7������
//LAN8720��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2018/7/6
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	

ETH_HandleTypeDef LAN8720_ETHHandle;

//��̫���������ͻ�����
__attribute__((at(0x30040000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT];      //��̫��Rx DMA������
__attribute__((at(0x30040060))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT];      //��̫��Tx DMA������
__attribute__((at(0x30040200))) uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE];  //��̫�����ջ�����


//����������ʹ�õ�0X30040000��ram�ڴ汣��
void NETMPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct;

    HAL_MPU_Disable();
    MPU_InitStruct.Enable=MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress=0x30040000;
    MPU_InitStruct.Size=MPU_REGION_SIZE_256B;
    MPU_InitStruct.AccessPermission=MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable=MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable=MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.Number=MPU_REGION_NUMBER5;
    MPU_InitStruct.TypeExtField=MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable=0x00;
    MPU_InitStruct.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct); 
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

//��ʼ��LAN8720
int32_t LAN8720_Init(void)
{         
    u8 macaddress[6];
    u32 timeout=0;
    u32 regval=0;
    u32 phylink=0;
    int32_t status=LAN8720_STATUS_OK;
    
    //Ӳ����λ
    INTX_DISABLE();                         //�ر������жϣ���λ���̲��ܱ���ϣ�
    PCF8574_WriteBit(ETH_RESET_IO,1);       //Ӳ����λ
    delay_ms(100);
    PCF8574_WriteBit(ETH_RESET_IO,0);       //��λ����
    delay_ms(100);
    INTX_ENABLE();                          //���������ж� 
    
    NETMPU_Config();                        //MPU��������
    macaddress[0]=lwipdev.mac[0]; 
	macaddress[1]=lwipdev.mac[1]; 
	macaddress[2]=lwipdev.mac[2];
	macaddress[3]=lwipdev.mac[3];   
	macaddress[4]=lwipdev.mac[4];
	macaddress[5]=lwipdev.mac[5];
    
    LAN8720_ETHHandle.Instance=ETH;                             //ETH
    LAN8720_ETHHandle.Init.MACAddr=macaddress;                  //mac��ַ
    LAN8720_ETHHandle.Init.MediaInterface=HAL_ETH_RMII_MODE;    //RMII�ӿ�
    LAN8720_ETHHandle.Init.RxDesc=DMARxDscrTab;                 //����������
    LAN8720_ETHHandle.Init.TxDesc=DMATxDscrTab;                 //����������
    LAN8720_ETHHandle.Init.RxBuffLen=ETH_MAX_PACKET_SIZE;       //���ճ���
    HAL_ETH_Init(&LAN8720_ETHHandle);                           //��ʼ��ETH
    HAL_ETH_SetMDIOClockRange(&LAN8720_ETHHandle);
    
    if(LAN8720_WritePHY(LAN8720_BCR,LAN8720_BCR_SOFT_RESET)>=0) //LAN8720�����λ
    {
        //�ȴ������λ���
        if(LAN8720_ReadPHY(LAN8720_BCR,&regval)>=0)
        {
            while(regval&LAN8720_BCR_SOFT_RESET)
            {
                if(LAN8720_ReadPHY(LAN8720_BCR,&regval)<0)
                {
                    status=LAN8720_STATUS_READ_ERROR;
                    break;
                }
                delay_ms(10);
                timeout++;
                if(timeout>=LAN8720_TIMEOUT) break; //��ʱ����,5S
            }
    
        }
        else
        {
            status=LAN8720_STATUS_READ_ERROR;
        }
    }
    else
    {
        status=LAN8720_STATUS_WRITE_ERROR;
    }

    LAN8720_StartAutoNego();                //�����Զ�Э�̹���
    
    if(status==LAN8720_STATUS_OK)           //���ǰ��������������ʱ1s
        delay_ms(1000);                     //�ȴ�1s
       
    //�ȴ��������ӳɹ�
    timeout=0;
    while(LAN8720_GetLinkState()<=LAN8720_STATUS_LINK_DOWN)  
    {
        delay_ms(10);
        timeout++;
        if(timeout>=LAN8720_TIMEOUT) 
        {
            status=LAN8720_STATUS_LINK_DOWN;
            break; //��ʱ����,5S
        }
    }
    phylink=LAN8720_GetLinkState();
    if(phylink==LAN8720_STATUS_100MBITS_FULLDUPLEX)
        printf("LAN8720:100Mb/s FullDuplex\r\n");
    else if(phylink==LAN8720_STATUS_100MBITS_HALFDUPLEX)
        printf("LAN8720:100Mb/s HalfDuplex\r\n");
    else if(phylink==LAN8720_STATUS_10MBITS_FULLDUPLEX)
        printf("LAN8720:10Mb/s FullDuplex\r\n");
    else if(phylink==LAN8720_STATUS_10MBITS_HALFDUPLEX)
        printf("LAN8720:10Mb/s HalfDuplex\r\n");
    return status; 
}

extern void lwip_pkt_handle(void);
    
//�жϷ�����
void ETH_IRQHandler(void)
{
    lwip_pkt_handle();
    //����жϱ�־λ
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_NORMAL_IT);  //���DMA�жϱ�־λ
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_RX_IT);     //���DMA�����жϱ�־λ
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_TX_IT);     //���DMA�����жϱ�־λ
}


//ETH�ײ��������������ã�ʱ��ʹ��
//�˺����ᱻHAL_ETH_Init()����
//heth:ETH���
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE();			//����GPIOAʱ��
	__HAL_RCC_GPIOB_CLK_ENABLE();			//����GPIOBʱ��
    __HAL_RCC_GPIOC_CLK_ENABLE();			//����GPIOCʱ��
    __HAL_RCC_GPIOG_CLK_ENABLE();			//����GPIOGʱ��
    __HAL_RCC_ETH1MAC_CLK_ENABLE();         //ʹ��ETH1 MACʱ��
    __HAL_RCC_ETH1TX_CLK_ENABLE();          //ʹ��ETH1����ʱ��
    __HAL_RCC_ETH1RX_CLK_ENABLE();          //ʹ��ETH1����ʱ��
    
    GPIO_Initure.Pin=GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7; 
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;          //���츴��
    GPIO_Initure.Pull=GPIO_NOPULL;              //����������
    GPIO_Initure.Speed=GPIO_SPEED_FREQ_HIGH;    //����
    GPIO_Initure.Alternate=GPIO_AF11_ETH;       //����ΪETH����
    HAL_GPIO_Init(GPIOA,&GPIO_Initure);         //��ʼ��
    
    //PB11
    GPIO_Initure.Pin=GPIO_PIN_11;               //PB11
    HAL_GPIO_Init(GPIOB,&GPIO_Initure);         //ʼ��
    
    //PC1,4,5
    GPIO_Initure.Pin=GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5; //PC1,4,5
    HAL_GPIO_Init(GPIOC,&GPIO_Initure);         //��ʼ��
	
    //PG13,14
    GPIO_Initure.Pin=GPIO_PIN_13|GPIO_PIN_14;   //PG13,14
    HAL_GPIO_Init(GPIOG,&GPIO_Initure);         //��ʼ��
    
    HAL_NVIC_SetPriority(ETH_IRQn,1,0);         //�����ж����ȼ�Ӧ�ø�һ��
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}


//��ȡPHY�Ĵ���ֵ
//regҪ��ȡ�ļĴ�����ַ
//����ֵ:0 ��ȡ�ɹ���-1 ��ȡʧ��
int32_t LAN8720_ReadPHY(u16 reg,u32 *regval)
{
    if(HAL_ETH_ReadPHYRegister(&LAN8720_ETHHandle,LAN8720_ADDR,reg,regval)!=HAL_OK)
        return -1;
    return 0;
}

//��LAN8720ָ���Ĵ���д��ֵ
//reg:Ҫд��ļĴ���
//value:Ҫд���ֵ
//����ֵ:0 д��������-1 д��ʧ��
int32_t LAN8720_WritePHY(u16 reg,u16 value)
{
    u32 temp=value;
    if(HAL_ETH_WritePHYRegister(&LAN8720_ETHHandle,LAN8720_ADDR,reg,temp)!=HAL_OK)
        return -1;
    return 0;
}

//��LAN8720 Power Downģʽ 
void LAN8720_EnablePowerDownMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_POWER_DOWN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//�ر�LAN8720 Power Downģʽ
void LAN8720_DisablePowerDownMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval&=~LAN8720_BCR_POWER_DOWN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//����LAN8720����Э�̹���
void LAN8720_StartAutoNego(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_AUTONEGO_EN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//ʹ�ܻز�ģʽ
void LAN8720_EnableLoopbackMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_LOOPBACK;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//�ر�LAN8720�Ļز�ģʽ
void LAN8720_DisableLoopbackMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval&=~LAN8720_BCR_LOOPBACK;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//ʹ���жϣ��ж�Դ��ѡ:LAN8720_ENERGYON_IT
//                     LAN8720_AUTONEGO_COMPLETE_IT
//                     LAN8720_REMOTE_FAULT_IT
//                     LAN8720_LINK_DOWN_IT
//                     LAN8720_AUTONEGO_LP_ACK_IT        
//                     LAN8720_PARALLEL_DETECTION_FAULT_IT
//                     LAN8720_AUTONEGO_PAGE_RECEIVED_IT
void LAN8720_EnableIT(u32 interrupt)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_IMR,&readval);
    readval|=interrupt;
    LAN8720_WritePHY(LAN8720_IMR,readval);
}

//�ر��жϣ��ж�Դ��ѡ:LAN8720_ENERGYON_IT
//                     LAN8720_AUTONEGO_COMPLETE_IT
//                     LAN8720_REMOTE_FAULT_IT
//                     LAN8720_LINK_DOWN_IT
//                     LAN8720_AUTONEGO_LP_ACK_IT        
//                     LAN8720_PARALLEL_DETECTION_FAULT_IT
//                     LAN8720_AUTONEGO_PAGE_RECEIVED_IT
void LAN8720_DisableIT(u32 interrupt)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_IMR,&readval);
    readval&=~interrupt;
    LAN8720_WritePHY(LAN8720_IMR,readval);
}

//����жϱ�־λ�����Ĵ���ISFR�Ϳ�����жϱ�־λ
void LAN8720_ClearIT(u32 interrupt)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_ISFR,&readval);
}

//��ȡ�жϱ�־λ
//����ֵ��1 �жϱ�־λ��λ��
//        0 �жϱ�־λ����
u8 LAN8720_GetITStatus(u32 interrupt)
{
    u32 readval=0;
    u32 status=0;
    LAN8720_ReadPHY(LAN8720_ISFR,&readval);
    if(readval&interrupt) status=1;
    else status=0;
    return status;
}

//��ȡLAN8720������״̬
//����ֵ��LAN8720_STATUS_LINK_DOWN              ���ӶϿ�
//        LAN8720_STATUS_AUTONEGO_NOTDONE       �Զ�Э�����
//        LAN8720_STATUS_100MBITS_FULLDUPLEX    100Mȫ˫��
//        LAN8720_STATUS_100MBITS_HALFDUPLEX    100M��˫��
//        LAN8720_STATUS_10MBITS_FULLDUPLEX     10Mȫ˫��
//        LAN8720_STATUS_10MBITS_HALFDUPLEX     10M��˫��
u32 LAN8720_GetLinkState(void)
{
    u32 readval=0;
    
    //��ȡ���飬ȷ����ȡ��ȷ������
    LAN8720_ReadPHY(LAN8720_BSR,&readval);
    LAN8720_ReadPHY(LAN8720_BSR,&readval);
    
    //��ȡ����״̬(Ӳ�������ߵ����ӣ�����TCP��UDP��������ӣ�)
    if((readval&LAN8720_BSR_LINK_STATUS)==0)
        return LAN8720_STATUS_LINK_DOWN;
    
    //��ȡ�Զ�Э��״̬
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    if((readval&LAN8720_BCR_AUTONEGO_EN)!=LAN8720_BCR_AUTONEGO_EN)  //δʹ���Զ�Э��
    {
        if(((readval&LAN8720_BCR_SPEED_SELECT)==LAN8720_BCR_SPEED_SELECT)&&
                ((readval&LAN8720_BCR_DUPLEX_MODE)==LAN8720_BCR_DUPLEX_MODE)) 
            return LAN8720_STATUS_100MBITS_FULLDUPLEX;
        else if((readval&LAN8720_BCR_SPEED_SELECT)==LAN8720_BCR_SPEED_SELECT)
            return LAN8720_STATUS_100MBITS_HALFDUPLEX;
        else if((readval&LAN8720_BCR_DUPLEX_MODE)==LAN8720_BCR_DUPLEX_MODE)
            return LAN8720_STATUS_10MBITS_FULLDUPLEX;
        else
            return LAN8720_STATUS_10MBITS_HALFDUPLEX;
    }
    else                                                            //ʹ�����Զ�Э��    
    {
        LAN8720_ReadPHY(LAN8720_PHYSCSR,&readval);
        if((readval&LAN8720_PHYSCSR_AUTONEGO_DONE)==0)
            return LAN8720_STATUS_AUTONEGO_NOTDONE;
        if((readval&LAN8720_PHYSCSR_HCDSPEEDMASK)==LAN8720_PHYSCSR_100BTX_FD)
            return LAN8720_STATUS_100MBITS_FULLDUPLEX;
        else if ((readval&LAN8720_PHYSCSR_HCDSPEEDMASK)==LAN8720_PHYSCSR_100BTX_HD)
            return LAN8720_STATUS_100MBITS_HALFDUPLEX;
        else if ((readval&LAN8720_PHYSCSR_HCDSPEEDMASK)==LAN8720_PHYSCSR_10BT_FD)
            return LAN8720_STATUS_10MBITS_FULLDUPLEX;
        else
            return LAN8720_STATUS_10MBITS_HALFDUPLEX;         
    }
}


//����LAN8720������״̬
//����linkstate��LAN8720_STATUS_100MBITS_FULLDUPLEX 100Mȫ˫��
//               LAN8720_STATUS_100MBITS_HALFDUPLEX 100M��˫��
//               LAN8720_STATUS_10MBITS_FULLDUPLEX  10Mȫ˫��
//               LAN8720_STATUS_10MBITS_HALFDUPLEX  10M��˫��
void LAN8720_SetLinkState(u32 linkstate)
{
    
    u32 bcrvalue=0;
    LAN8720_ReadPHY(LAN8720_BCR,&bcrvalue);
    //�ر��������ã������Զ�Э�̣��ٶȺ�˫��
    bcrvalue&=~(LAN8720_BCR_AUTONEGO_EN|LAN8720_BCR_SPEED_SELECT|LAN8720_BCR_DUPLEX_MODE);
    if(linkstate==LAN8720_STATUS_100MBITS_FULLDUPLEX)       //100Mȫ˫��
        bcrvalue|=(LAN8720_BCR_SPEED_SELECT|LAN8720_BCR_DUPLEX_MODE);
    else if(linkstate==LAN8720_STATUS_100MBITS_HALFDUPLEX)  //100M��˫��
        bcrvalue|=LAN8720_BCR_SPEED_SELECT; 
    else if(linkstate==LAN8720_STATUS_10MBITS_FULLDUPLEX)   //10Mȫ˫��
        bcrvalue|=LAN8720_BCR_DUPLEX_MODE;
    
    LAN8720_WritePHY(LAN8720_BCR,bcrvalue);
}
