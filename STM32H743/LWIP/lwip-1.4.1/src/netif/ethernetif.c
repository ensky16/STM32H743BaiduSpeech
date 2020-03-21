#include "netif/ethernetif.h" 
#include "lan8720.h"  
#include "lwip_comm.h" 
#include "netif/etharp.h"  
#include "string.h"  
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32H7������
//LWIP ethernetif��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2018/7/6
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	
ETH_TxPacketConfig TxConfig; 
uint32_t current_pbuf_idx =0;
struct pbuf_custom rx_pbuf[ETH_RX_DESC_CNT];
void pbuf_free_custom(struct pbuf *p);

//��ethernetif_init()�������ڳ�ʼ��Ӳ��
//netif:�����ṹ��ָ�� 
//����ֵ:ERR_OK,����
//       ����,ʧ��
static void low_level_init(struct netif *netif)
{ 
    uint32_t idx = 0;
    ETH_MACConfigTypeDef MACConf;
    u32 PHYLinkState;
    u32 speed=0,duplex=0;
    
    netif->hwaddr_len=ETHARP_HWADDR_LEN;  //����MAC��ַ����,Ϊ6���ֽ�
	//��ʼ��MAC��ַ,����ʲô��ַ���û��Լ�����,���ǲ����������������豸MAC��ַ�ظ�
	netif->hwaddr[0]=lwipdev.mac[0]; 
	netif->hwaddr[1]=lwipdev.mac[1]; 
	netif->hwaddr[2]=lwipdev.mac[2];
	netif->hwaddr[3]=lwipdev.mac[3];   
	netif->hwaddr[4]=lwipdev.mac[4];
	netif->hwaddr[5]=lwipdev.mac[5];
    netif->mtu=ETH_MAX_PAYLOAD;//��������䵥Ԫ,����������㲥��ARP����
  
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
    PHYLinkState=LAN8720_GetLinkState();    //��ȡ����״̬
    switch (PHYLinkState)
    {
        case LAN8720_STATUS_100MBITS_FULLDUPLEX:    //100Mȫ˫��
            duplex=ETH_FULLDUPLEX_MODE;
            speed=ETH_SPEED_100M;
            break;
        case LAN8720_STATUS_100MBITS_HALFDUPLEX:    //100M��˫��
            duplex=ETH_HALFDUPLEX_MODE;
            speed=ETH_SPEED_100M;
            break;
        case LAN8720_STATUS_10MBITS_FULLDUPLEX:     //10Mȫ˫��
            duplex=ETH_FULLDUPLEX_MODE;
            speed=ETH_SPEED_10M;
            break;
        case LAN8720_STATUS_10MBITS_HALFDUPLEX:     //10M��˫��
            duplex=ETH_HALFDUPLEX_MODE;
            speed=ETH_SPEED_10M;
            break;
        default:
            break;      
    }
    
    HAL_ETH_GetMACConfig(&LAN8720_ETHHandle,&MACConf); 
    MACConf.DuplexMode=duplex;
    MACConf.Speed=speed;
    HAL_ETH_SetMACConfig(&LAN8720_ETHHandle,&MACConf);  //����MAC
    HAL_ETH_Start_IT(&LAN8720_ETHHandle);
    netif_set_up(netif);                        //������
    netif_set_link_up(netif);                   //������������
        
    //ethernet_link_check_state(netif);
}

//���ڷ������ݰ�����ײ㺯��(lwipͨ��netif->linkoutputָ��ú���)
//netif:�����ṹ��ָ��
//p:pbuf���ݽṹ��ָ��
//����ֵ:ERR_OK,��������
//       ERR_MEM,����ʧ��
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

    SCB_CleanInvalidateDCache();    //��Ч�������Dcache
    HAL_ETH_Transmit(&LAN8720_ETHHandle,&TxConfig,0);
  
    return errval;
}

//���ڽ������ݰ�����ײ㺯��
//neitif:�����ṹ��ָ��
//����ֵ:pbuf���ݽṹ��ָ��
static struct pbuf * low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    ETH_BufferTypeDef RxBuff;
    uint32_t framelength = 0;
  
    if(HAL_ETH_IsRxDataAvailable(&LAN8720_ETHHandle))
    {
        SCB_CleanInvalidateDCache();    //��Ч���������Dcache
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

//������������(lwipֱ�ӵ���) 
//netif:�����ṹ��ָ��
//����ֵ:ERR_OK,��������
//       ERR_MEM,����ʧ��
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

//�˺���ʹ��low_level_init()��������ʼ������
//netif:�����ṹ��ָ��
//����ֵ:ERR_OK,����
//       ����,ʧ��
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
  
#if LWIP_NETIF_HOSTNAME
    netif->hostname="lwip"; //��������
#endif

    netif->name[0]=IFNAME0;
    netif->name[1]=IFNAME1;
    netif->output=etharp_output;
    netif->linkoutput=low_level_output;
    low_level_init(netif);
    return ERR_OK;
}


//�û��Զ���Ľ���pbuf�ͷź���
//p��Ҫ�ͷŵ�pbuf
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

//��������״̬���
void ethernet_link_check_state(struct netif *netif)
{
    ETH_MACConfigTypeDef MACConf;
    u32 PHYLinkState;
    u32 linkchanged=0,speed=0,duplex=0;
    
    PHYLinkState=LAN8720_GetLinkState();    //��ȡ����״̬
    //�����⵽���ӶϿ����߲������͹ر�����
    if(netif_is_link_up(netif)&&(PHYLinkState<=LAN8720_STATUS_LINK_DOWN))
    {
        HAL_ETH_Stop_IT(&LAN8720_ETHHandle);
        netif_set_down(netif);              //�ر�����
        netif_set_link_down(netif);         //���ӹر�
    }
    //LWIP������δ�򿪣�����LAN8720�Ѿ�Э�̳ɹ�
    else if(!netif_is_link_up(netif)&&(PHYLinkState>LAN8720_STATUS_LINK_DOWN))
    {
        switch (PHYLinkState)
        {
            case LAN8720_STATUS_100MBITS_FULLDUPLEX:    //100Mȫ˫��
                duplex=ETH_FULLDUPLEX_MODE;
                speed=ETH_SPEED_100M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_100MBITS_HALFDUPLEX:    //100M��˫��
                duplex=ETH_HALFDUPLEX_MODE;
                speed=ETH_SPEED_100M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_10MBITS_FULLDUPLEX:     //10Mȫ˫��
                duplex=ETH_FULLDUPLEX_MODE;
                speed=ETH_SPEED_10M;
                linkchanged=1;
                break;
            case LAN8720_STATUS_10MBITS_HALFDUPLEX:     //10M��˫��
                duplex=ETH_HALFDUPLEX_MODE;
                speed=ETH_SPEED_10M;
                linkchanged=1;
                break;
            default:
                break;      
        }
    
        if(linkchanged)                                 //��������
        {
            HAL_ETH_GetMACConfig(&LAN8720_ETHHandle,&MACConf); 
            MACConf.DuplexMode=duplex;
            MACConf.Speed=speed;
            HAL_ETH_SetMACConfig(&LAN8720_ETHHandle,&MACConf);  //����MAC
            HAL_ETH_Start_IT(&LAN8720_ETHHandle);
            netif_set_up(netif);                        //������
            netif_set_link_up(netif);                   //������������
        }
    }
}














