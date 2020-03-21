#include "netif/ethernetif.h" 
#include "lan8720.h"  
#include "lwip_comm.h" 
#include "netif/etharp.h"  
#include "string.h"  
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//LWIP ethernetif驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2018/7/6
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
ETH_TxPacketConfig TxConfig; 
uint32_t current_pbuf_idx =0;
struct pbuf_custom rx_pbuf[ETH_RX_DESC_CNT];
void pbuf_free_custom(struct pbuf *p);

//由ethernetif_init()调用用于初始化硬件
//netif:网卡结构体指针 
//返回值:ERR_OK,正常
//       其他,失败
static void low_level_init(struct netif *netif)
{ 
    uint32_t idx = 0;
    ETH_MACConfigTypeDef MACConf;
    u32 PHYLinkState;
    u32 speed=0,duplex=0;
    
    netif->hwaddr_len=ETHARP_HWADDR_LEN;  //设置MAC地址长度,为6个字节
	//初始化MAC地址,设置什么地址由用户自己设置,但是不能与网络中其他设备MAC地址重复
	netif->hwaddr[0]=lwipdev.mac[0]; 
	netif->hwaddr[1]=lwipdev.mac[1]; 
	netif->hwaddr[2]=lwipdev.mac[2];
	netif->hwaddr[3]=lwipdev.mac[3];   
	netif->hwaddr[4]=lwipdev.mac[4];
	netif->hwaddr[5]=lwipdev.mac[5];
    netif->mtu=ETH_MAX_PAYLOAD;//最大允许传输单元,允许该网卡广播和ARP功能
  
    netif->flags|=NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP|NETIF_FLAG_LINK_UP;
    for(idx=0;idx<ETH_RX_DESC_CNT;idx ++)
    {
        HAL_ETH_DescAssignMemory(&LAN8720_ETHHandle,idx,Rx_Buff[idx],NULL);
        rx_pbuf[idx].custom_free_function=pbuf_free_custom;
    }
  
    memset(&TxConfig,0,sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes=ETH_TX_PACKETS_FEATURES_CSUM|ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl=ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl=ETH_CRC_PAD_INSERT;
    PHYLinkState=LAN8720_GetLinkState();    //获取连接状态
    switch (PHYLinkState)
    {
        case LAN8720_STATUS_100MBITS_FULLDUPLEX:    //100M全双工
            duplex=ETH_FULLDUPLEX_MODE;
            speed=ETH_SPEED_100M;
            break;
        case LAN8720_STATUS_100MBITS_HALFDUPLEX:    //100M半双工
            duplex=ETH_HALFDUPLEX_MODE;
            speed=ETH_SPEED_100M;
            break;
        case LAN8720_STATUS_10MBITS_FULLDUPLEX:     //10M全双工
            duplex=ETH_FULLDUPLEX_MODE;
            speed=ETH_SPEED_10M;
            break;
        case LAN8720_STATUS_10MBITS_HALFDUPLEX:     //10M半双工
            duplex=ETH_HALFDUPLEX_MODE;
            speed=ETH_SPEED_10M;
            break;
        default:
            break;      
    }
    
    HAL_ETH_GetMACConfig(&LAN8720_ETHHandle,&MACConf); 
    MACConf.DuplexMode=duplex;
    MACConf.Speed=speed;
    HAL_ETH_SetMACConfig(&LAN8720_ETHHandle,&MACConf);  //设置MAC
    HAL_ETH_Start_IT(&LAN8720_ETHHandle);
    netif_set_up(netif);                        //打开网卡
    netif_set_link_up(netif);                   //开启网卡连接
        
    //ethernet_link_check_state(netif);
}

//用于发送数据包的最底层函数(lwip通过netif->linkoutput指向该函数)
//netif:网卡结构体指针
//p:pbuf数据结构体指针
//返回值:ERR_OK,发送正常
//       ERR_MEM,发送失败
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    uint32_t i=0, framelen = 0;
    struct pbuf *q;
    err_t errval=ERR_OK;
    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
  
    for(q=p;q!=NULL;q=q->next)
    {
        if(i>=ETH_TX_DESC_CNT)	
            return ERR_IF;
    
        Txbuffer[i].buffer=q->payload;
        Txbuffer[i].len=q->len;
        framelen+=q->len;
    
        if(i>0)
        {
            Txbuffer[i-1].next=&Txbuffer[i];
        }
    
        if(q->next == NULL)
        {
            Txbuffer[i].next=NULL;
        }
        i++;
    }

    TxConfig.Length=framelen;
    TxConfig.TxBuffer=Txbuffer;

    SCB_CleanInvalidateDCache();    //无效化并清除Dcache
    HAL_ETH_Transmit(&LAN8720_ETHHandle,&TxConfig,0);
  
    return errval;
}

//用于接收数据包的最底层函数
//neitif:网卡结构体指针
//返回值:pbuf数据结构体指针
static struct pbuf * low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    ETH_BufferTypeDef RxBuff;
    uint32_t framelength = 0;
  
    if(HAL_ETH_IsRxDataAvailable(&LAN8720_ETHHandle))
    {
        SCB_CleanInvalidateDCache();    //无效化并且清除Dcache
        HAL_ETH_GetRxDataBuffer(&LAN8720_ETHHandle,&RxBuff);
        HAL_ETH_GetRxDataLength(&LAN8720_ETHHandle,&framelength);
        p=pbuf_alloced_custom(PBUF_RAW,framelength,PBUF_POOL,&rx_pbuf[current_pbuf_idx],RxBuff.buffer, framelength);

        if(current_pbuf_idx<(ETH_RX_DESC_CNT-1))
        {
            current_pbuf_idx++;
        }
        else
        {
            current_pbuf_idx=0;
        }
    
        return p;
    }
    else
    {
        return NULL;
    }
}

//网卡接收数据(lwip直接调用) 
//netif:网卡结构体指针
//返回值:ERR_OK,发送正常
//       ERR_MEM,发送失败
err_t ethernetif_input(struct netif *netif)
{
    err_t err;
    struct pbuf *p;
  
    p=low_level_input(netif);
    if(p==NULL) return ERR_MEM;
    err=netif->input(p, netif);
    if (err!=ERR_OK)
    {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
        p=NULL;
    }
    HAL_ETH_BuildRxDescriptors(&LAN8720_ETHHandle);
    return err;
}

//此函数使用low_level_init()函数来初始化网络
//netif:网卡结构体指针
//返回值:ERR_OK,正常
//       其他,失败
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
  
#if LWIP_NETIF_HOSTNAME
    netif->hostname="lwip"; //主机名字
#endif

    netif->name[0]=IFNAME0;
    netif->name[1]=IFNAME1;
    netif->output=etharp_output;
    netif->linkoutput=low_level_output;
    low_level_init(netif);
    return ERR_OK;
}


//用户自定义的接收pbuf释放函数
//p：要释放的pbuf
void pbuf_free_custom(struct pbuf *p)
{
    if(p != NULL)
    {
        p->flags=0;
        p->next=NULL;
        p->len=p->tot_len=0;
        p->ref=0;
        p->payload=NULL;
    }
}

//网络连接状态检查
void ethernet_link_check_state(struct netif *netif)
{
    ETH_MACConfigTypeDef MACConf;
    u32 PHYLinkState;
    u32 linkchanged=0,speed=0,duplex=0;
    
    PHYLinkState=LAN8720_GetLinkState();    //获取连接状态
    //如果检测到连接断开或者不正常就关闭网口
    if(netif_is_link_up(netif)&&(PHYLinkState<=LAN8720_STATUS_LINK_DOWN))
    {
        HAL_ETH_Stop_IT(&LAN8720_ETHHandle);
        netif_set_down(netif);              //关闭网口
        netif_set_link_down(netif);         //连接关闭
    }
    //LWIP网卡还未打开，但是LAN8720已经协商成功
    else if(!netif_is_link_up(netif)&&(PHYLinkState>LAN8720_STATUS_LINK_DOWN))
    {
        switch (PHYLinkState)
        {
            case LAN8720_STATUS_100MBITS_FULLDUPLEX:    //100M全双工
                duplex=ETH_FULLDUPLEX_MODE;
                speed=ETH_SPEED_100M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_100MBITS_HALFDUPLEX:    //100M半双工
                duplex=ETH_HALFDUPLEX_MODE;
                speed=ETH_SPEED_100M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_10MBITS_FULLDUPLEX:     //10M全双工
                duplex=ETH_FULLDUPLEX_MODE;
                speed=ETH_SPEED_10M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_10MBITS_HALFDUPLEX:     //10M半双工
                duplex=ETH_HALFDUPLEX_MODE;
                speed=ETH_SPEED_10M;
                linkchanged=1;
                break;
            default:
                break;      
        }
    
        if(linkchanged)                                 //连接正常
        {
            HAL_ETH_GetMACConfig(&LAN8720_ETHHandle,&MACConf); 
            MACConf.DuplexMode=duplex;
            MACConf.Speed=speed;
            HAL_ETH_SetMACConfig(&LAN8720_ETHHandle,&MACConf);  //设置MAC
            HAL_ETH_Start_IT(&LAN8720_ETHHandle);
            netif_set_up(netif);                        //打开网卡
            netif_set_link_up(netif);                   //开启网卡连接
        }
    }
}














