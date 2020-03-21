#include "lan8720.h"
#include "lwip_comm.h"
#include "pcf8574.h"
#include "delay.h"
#include "usart.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//LAN8720驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2018/7/6
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	

ETH_HandleTypeDef LAN8720_ETHHandle;

//以太网描述符和缓冲区
__attribute__((at(0x30040000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT];      //以太网Rx DMA描述符
__attribute__((at(0x30040060))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT];      //以太网Tx DMA描述符
__attribute__((at(0x30040200))) uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE];  //以太网接收缓冲区


//设置网络所使用的0X30040000的ram内存保护
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

//初始化LAN8720
int32_t LAN8720_Init(void)
{         
    u8 macaddress[6];
    u32 timeout=0;
    u32 regval=0;
    u32 phylink=0;
    int32_t status=LAN8720_STATUS_OK;
    
    //硬件复位
    INTX_DISABLE();                         //关闭所有中断，复位过程不能被打断！
    PCF8574_WriteBit(ETH_RESET_IO,1);       //硬件复位
    delay_ms(100);
    PCF8574_WriteBit(ETH_RESET_IO,0);       //复位结束
    delay_ms(100);
    INTX_ENABLE();                          //开启所有中断 
    
    NETMPU_Config();                        //MPU保护设置
    macaddress[0]=lwipdev.mac[0]; 
	macaddress[1]=lwipdev.mac[1]; 
	macaddress[2]=lwipdev.mac[2];
	macaddress[3]=lwipdev.mac[3];   
	macaddress[4]=lwipdev.mac[4];
	macaddress[5]=lwipdev.mac[5];
    
    LAN8720_ETHHandle.Instance=ETH;                             //ETH
    LAN8720_ETHHandle.Init.MACAddr=macaddress;                  //mac地址
    LAN8720_ETHHandle.Init.MediaInterface=HAL_ETH_RMII_MODE;    //RMII接口
    LAN8720_ETHHandle.Init.RxDesc=DMARxDscrTab;                 //发送描述符
    LAN8720_ETHHandle.Init.TxDesc=DMATxDscrTab;                 //接收描述如
    LAN8720_ETHHandle.Init.RxBuffLen=ETH_MAX_PACKET_SIZE;       //接收长度
    HAL_ETH_Init(&LAN8720_ETHHandle);                           //初始化ETH
    HAL_ETH_SetMDIOClockRange(&LAN8720_ETHHandle);
    
    if(LAN8720_WritePHY(LAN8720_BCR,LAN8720_BCR_SOFT_RESET)>=0) //LAN8720软件复位
    {
        //等待软件复位完成
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
                if(timeout>=LAN8720_TIMEOUT) break; //超时跳出,5S
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

    LAN8720_StartAutoNego();                //开启自动协商功能
    
    if(status==LAN8720_STATUS_OK)           //如果前面运行正常就延时1s
        delay_ms(1000);                     //等待1s
       
    //等待网络连接成功
    timeout=0;
    while(LAN8720_GetLinkState()<=LAN8720_STATUS_LINK_DOWN)  
    {
        delay_ms(10);
        timeout++;
        if(timeout>=LAN8720_TIMEOUT) 
        {
            status=LAN8720_STATUS_LINK_DOWN;
            break; //超时跳出,5S
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
    
//中断服务函数
void ETH_IRQHandler(void)
{
    lwip_pkt_handle();
    //清除中断标志位
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_NORMAL_IT);  //清除DMA中断标志位
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_RX_IT);     //清除DMA接收中断标志位
    __HAL_ETH_DMA_CLEAR_IT(&LAN8720_ETHHandle,ETH_DMA_TX_IT);     //清除DMA接收中断标志位
}


//ETH底层驱动，引脚配置，时钟使能
//此函数会被HAL_ETH_Init()调用
//heth:ETH句柄
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE();			//开启GPIOA时钟
	__HAL_RCC_GPIOB_CLK_ENABLE();			//开启GPIOB时钟
    __HAL_RCC_GPIOC_CLK_ENABLE();			//开启GPIOC时钟
    __HAL_RCC_GPIOG_CLK_ENABLE();			//开启GPIOG时钟
    __HAL_RCC_ETH1MAC_CLK_ENABLE();         //使能ETH1 MAC时钟
    __HAL_RCC_ETH1TX_CLK_ENABLE();          //使能ETH1发送时钟
    __HAL_RCC_ETH1RX_CLK_ENABLE();          //使能ETH1接收时钟
    
    GPIO_Initure.Pin=GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7; 
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;          //推挽复用
    GPIO_Initure.Pull=GPIO_NOPULL;              //不带上下拉
    GPIO_Initure.Speed=GPIO_SPEED_FREQ_HIGH;    //高速
    GPIO_Initure.Alternate=GPIO_AF11_ETH;       //复用为ETH功能
    HAL_GPIO_Init(GPIOA,&GPIO_Initure);         //初始化
    
    //PB11
    GPIO_Initure.Pin=GPIO_PIN_11;               //PB11
    HAL_GPIO_Init(GPIOB,&GPIO_Initure);         //始化
    
    //PC1,4,5
    GPIO_Initure.Pin=GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5; //PC1,4,5
    HAL_GPIO_Init(GPIOC,&GPIO_Initure);         //初始化
	
    //PG13,14
    GPIO_Initure.Pin=GPIO_PIN_13|GPIO_PIN_14;   //PG13,14
    HAL_GPIO_Init(GPIOG,&GPIO_Initure);         //初始化
    
    HAL_NVIC_SetPriority(ETH_IRQn,1,0);         //网络中断优先级应该高一点
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}


//读取PHY寄存器值
//reg要读取的寄存器地址
//返回值:0 读取成功，-1 读取失败
int32_t LAN8720_ReadPHY(u16 reg,u32 *regval)
{
    if(HAL_ETH_ReadPHYRegister(&LAN8720_ETHHandle,LAN8720_ADDR,reg,regval)!=HAL_OK)
        return -1;
    return 0;
}

//向LAN8720指定寄存器写入值
//reg:要写入的寄存器
//value:要写入的值
//返回值:0 写入正常，-1 写入失败
int32_t LAN8720_WritePHY(u16 reg,u16 value)
{
    u32 temp=value;
    if(HAL_ETH_WritePHYRegister(&LAN8720_ETHHandle,LAN8720_ADDR,reg,temp)!=HAL_OK)
        return -1;
    return 0;
}

//打开LAN8720 Power Down模式 
void LAN8720_EnablePowerDownMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_POWER_DOWN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//关闭LAN8720 Power Down模式
void LAN8720_DisablePowerDownMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval&=~LAN8720_BCR_POWER_DOWN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//开启LAN8720的自协商功能
void LAN8720_StartAutoNego(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_AUTONEGO_EN;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//使能回测模式
void LAN8720_EnableLoopbackMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval|=LAN8720_BCR_LOOPBACK;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//关闭LAN8720的回测模式
void LAN8720_DisableLoopbackMode(void)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    readval&=~LAN8720_BCR_LOOPBACK;
    LAN8720_WritePHY(LAN8720_BCR,readval);
}

//使能中断，中断源可选:LAN8720_ENERGYON_IT
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

//关闭中断，中断源可选:LAN8720_ENERGYON_IT
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

//清除中断标志位，读寄存器ISFR就可清除中断标志位
void LAN8720_ClearIT(u32 interrupt)
{
    u32 readval=0;
    LAN8720_ReadPHY(LAN8720_ISFR,&readval);
}

//获取中断标志位
//返回值，1 中断标志位置位，
//        0 中断标志位清零
u8 LAN8720_GetITStatus(u32 interrupt)
{
    u32 readval=0;
    u32 status=0;
    LAN8720_ReadPHY(LAN8720_ISFR,&readval);
    if(readval&interrupt) status=1;
    else status=0;
    return status;
}

//获取LAN8720的连接状态
//返回值：LAN8720_STATUS_LINK_DOWN              连接断开
//        LAN8720_STATUS_AUTONEGO_NOTDONE       自动协商完成
//        LAN8720_STATUS_100MBITS_FULLDUPLEX    100M全双工
//        LAN8720_STATUS_100MBITS_HALFDUPLEX    100M半双工
//        LAN8720_STATUS_10MBITS_FULLDUPLEX     10M全双工
//        LAN8720_STATUS_10MBITS_HALFDUPLEX     10M半双工
u32 LAN8720_GetLinkState(void)
{
    u32 readval=0;
    
    //读取两遍，确保读取正确！！！
    LAN8720_ReadPHY(LAN8720_BSR,&readval);
    LAN8720_ReadPHY(LAN8720_BSR,&readval);
    
    //获取连接状态(硬件，网线的连接，不是TCP、UDP等软件连接！)
    if((readval&LAN8720_BSR_LINK_STATUS)==0)
        return LAN8720_STATUS_LINK_DOWN;
    
    //获取自动协商状态
    LAN8720_ReadPHY(LAN8720_BCR,&readval);
    if((readval&LAN8720_BCR_AUTONEGO_EN)!=LAN8720_BCR_AUTONEGO_EN)  //未使能自动协商
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
    else                                                            //使能了自动协商    
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


//设置LAN8720的连接状态
//参数linkstate：LAN8720_STATUS_100MBITS_FULLDUPLEX 100M全双工
//               LAN8720_STATUS_100MBITS_HALFDUPLEX 100M半双工
//               LAN8720_STATUS_10MBITS_FULLDUPLEX  10M全双工
//               LAN8720_STATUS_10MBITS_HALFDUPLEX  10M半双工
void LAN8720_SetLinkState(u32 linkstate)
{
    
    u32 bcrvalue=0;
    LAN8720_ReadPHY(LAN8720_BCR,&bcrvalue);
    //关闭连接配置，比如自动协商，速度和双工
    bcrvalue&=~(LAN8720_BCR_AUTONEGO_EN|LAN8720_BCR_SPEED_SELECT|LAN8720_BCR_DUPLEX_MODE);
    if(linkstate==LAN8720_STATUS_100MBITS_FULLDUPLEX)       //100M全双工
        bcrvalue|=(LAN8720_BCR_SPEED_SELECT|LAN8720_BCR_DUPLEX_MODE);
    else if(linkstate==LAN8720_STATUS_100MBITS_HALFDUPLEX)  //100M半双工
        bcrvalue|=LAN8720_BCR_SPEED_SELECT; 
    else if(linkstate==LAN8720_STATUS_10MBITS_FULLDUPLEX)   //10M全双工
        bcrvalue|=LAN8720_BCR_DUPLEX_MODE;
    
    LAN8720_WritePHY(LAN8720_BCR,bcrvalue);
}
