/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
 
#define SYS_ARCH_GLOBALS
 
/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/lwip_sys.h"
#include "lwip/mem.h"
#include "delay.h"
#include "arch/sys_arch.h"
#include "malloc.h"
#include "task.h"

const uint32_t NullMessage;
 

//����һ����Ϣ����
//*mbox:��Ϣ����
//size:�����С
//����ֵ:ERR_OK,�����ɹ�
//         ����,����ʧ��
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	if(size>MAX_QUEUE_ENTRIES)size=MAX_QUEUE_ENTRIES;		//��Ϣ�����������MAX_QUEUE_ENTRIES��Ϣ��Ŀ
 	mbox->xQueue = xQueueCreate(size, sizeof(void *));  		//������Ϣ���У�����Ϣ���д��ָ��
	LWIP_ASSERT("OSQCreate",mbox->xQueue!=NULL); 
	if(mbox->xQueue!=NULL)return ERR_OK;  //����ERR_OK,��ʾ��Ϣ���д����ɹ� ERR_OK=0
	else return ERR_MEM;  				//��Ϣ���д�������
} 

//�ͷŲ�ɾ��һ����Ϣ����
//*mbox:Ҫɾ������Ϣ����
void sys_mbox_free(sys_mbox_t *mbox)
{
	vQueueDelete(mbox->xQueue);
//	LWIP_ASSERT( "OSQDel ",mbox->xQueue == NULL ); 
	mbox->xQueue=NULL;
}

//����Ϣ�����з���һ����Ϣ(���뷢�ͳɹ�)
//mbox:��Ϣ����
//msg:Ҫ���͵���Ϣ
void sys_mbox_post(sys_mbox_t *mbox,void* msg)
{	 
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(msg==NULL)
		msg= (void*)&NullMessage;  //��msgΪ��ʱ msg����pvNullPointerָ���ֵ 
	if((SCB_ICSR_REG&0xFF) == 0)  //�߳�ִ��
	{
		while(xQueueSendToBack(mbox->xQueue, &msg, portMAX_DELAY) != pdPASS);//portMAX_DELAY,����ֱ�����ͳɹ�
	}
	else
	{
		while(xQueueSendToBackFromISR(mbox->xQueue, &msg, &xHigherPriorityTaskWoken) != pdPASS);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

//������һ����Ϣ���䷢����Ϣ
//�˺��������sys_mbox_post����ֻ����һ����Ϣ��
//����ʧ�ܺ󲻻᳢�Եڶ��η���
//*mbox:��Ϣ����
//*msg:Ҫ���͵���Ϣ
//����ֵ:ERR_OK,����OK
// 	     ERR_MEM,����ʧ��
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{ 
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(msg==NULL)
		msg= (void*)&NullMessage;//����ϢΪ�գ����ó���NullMessage�ĵ�ַ�滻
	if((SCB_ICSR_REG&0xFF) == 0)
	{
		if(xQueueSendToBack(mbox->xQueue, &msg, 0)!= pdPASS)
			return ERR_MEM;
	}
	else
	{
		if(xQueueSendToBackFromISR(mbox->xQueue, &msg, &xHigherPriorityTaskWoken)!= pdPASS)
			return ERR_MEM;
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	return ERR_OK;
}

//�ȴ������е���Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//timeout:��ʱʱ�䣬���timeoutΪ0�Ļ�,��һֱ�ȴ�
//����ֵ:��timeout��Ϊ0ʱ����ɹ��Ļ��ͷ��صȴ���ʱ�䣬
//		ʧ�ܵĻ��ͷ��س�ʱSYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{ 
	u32_t rtos_timeout,timeout_new;
	BaseType_t temp;
	
	temp=xQueueReceive(mbox->xQueue, msg, 0);
	if((temp == pdPASS)&&(*msg != NULL))
	{	
		if(*msg == (void*)&NullMessage) *msg = NULL;
		return 0;
	}
	
	if(timeout!=0)
	{
		rtos_timeout=(timeout*configTICK_RATE_HZ)/1000; //ת��Ϊ������,��Ϊfreertos��ʱʹ�õ��ǽ�����,��LWIP����ms
		if(rtos_timeout<1)
		{
			rtos_timeout=1;//����1������
		}
		else if(rtos_timeout >= portMAX_DELAY)
		{
			rtos_timeout = portMAX_DELAY - 1;
		}
	}else rtos_timeout = 0; 
	timeout = HAL_GetTick(); //��ȡϵͳʱ�� 
	if(rtos_timeout != 0)
	  temp = xQueueReceive(mbox->xQueue, msg, rtos_timeout); //������Ϣ����,�ȴ�ʱ��Ϊrtos_timeout
	else
		temp = xQueueReceive(mbox->xQueue, msg, portMAX_DELAY);//Ϊ0�����޵�

	if(temp == errQUEUE_EMPTY)
	{
		timeout=SYS_ARCH_TIMEOUT;  //����ʱ
		*msg = NULL;
	}
	else
	{
		if(*msg!=NULL)
	  {	
		  if(*msg == (void*)&NullMessage) 
				*msg = NULL;   	
	  }    
		timeout_new= HAL_GetTick();
		if (timeout_new>timeout) timeout_new = timeout_new - timeout;//���������Ϣ��ʹ�õ�ʱ��
		else timeout_new = 0xffffffff - timeout + timeout_new; 
		timeout=timeout_new*1000/configTICK_RATE_HZ + 1; 
	}
	return timeout; 
}

//���Ի�ȡ��Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//����ֵ:�ȴ���Ϣ���õ�ʱ��/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	BaseType_t temp;
	
	temp=xQueueReceive(mbox->xQueue, msg, 0);
	if((temp == pdPASS)&&(*msg != NULL))
	{	
		if(*msg == (void*)&NullMessage) *msg = NULL;
		return 0;
	}
	else
	{
		return SYS_MBOX_EMPTY;
	}
}

//���һ����Ϣ�����Ƿ���Ч
//*mbox:��Ϣ����
//����ֵ:1,��Ч.
//      0,��Ч
int sys_mbox_valid(sys_mbox_t *mbox)
{  
	if(mbox->xQueue != NULL)
	{
	   return 1;
	}		
	return 0;
} 

//����һ����Ϣ����Ϊ��Ч
//*mbox:��Ϣ����
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	mbox->xQueue=NULL;
} 

//����һ���ź���
//*sem:�������ź���
//count:�ź���ֵ
//����ֵ:ERR_OK,����OK
// 	     ERR_MEM,����ʧ��
err_t sys_sem_new(sys_sem_t* sem, u8_t count)
{   
	*sem=xSemaphoreCreateCounting(0xFF, count);
	if(*sem==NULL)return ERR_MEM; 
	LWIP_ASSERT("OSSemCreate ",*sem != NULL );
	return ERR_OK;
} 

//�ȴ�һ���ź���
//*sem:Ҫ�ȴ����ź���
//timeout:��ʱʱ��
//����ֵ:��timeout��Ϊ0ʱ����ɹ��Ļ��ͷ��صȴ���ʱ�䣬
//		ʧ�ܵĻ��ͷ��س�ʱSYS_ARCH_TIMEOUT
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{ 
	u32_t rtos_timeout, timeout_new;
	BaseType_t temp;
  if(xSemaphoreTake(*sem, 0) == pdPASS)
	{
		return 0;
	}		
	if(	timeout!=0) 
	{
		rtos_timeout = (timeout * configTICK_RATE_HZ) / 1000;//ת��Ϊ������,��ΪUCOS��ʱʹ�õ��ǽ�����,��LWIP����ms
		if(rtos_timeout < 1)
		rtos_timeout = 1;
	}else rtos_timeout = 0; 
	timeout = HAL_GetTick(); 
  if(rtos_timeout	!= 0)
	{
		temp = xSemaphoreTake(*sem, rtos_timeout);
	}
	else
	{
		temp = xSemaphoreTake(*sem, portMAX_DELAY);
	}
 	if(temp != pdPASS)
	{
		timeout=SYS_ARCH_TIMEOUT;//����ʱ
	}		
	else
	{     
 		timeout_new = HAL_GetTick(); 
		if (timeout_new>=timeout) timeout_new = timeout_new - timeout;
		else timeout_new = 0xffffffff - timeout + timeout_new;
 		timeout = (timeout_new*1000/configTICK_RATE_HZ + 1);//���������Ϣ��ʹ�õ�ʱ��(ms)
	}
	return timeout;
}

//����һ���ź���
//sem:�ź���ָ��
void sys_sem_signal(sys_sem_t *sem)
{
	while(xSemaphoreGive(*sem) != pdTRUE);
}

//�ͷŲ�ɾ��һ���ź���
//sem:�ź���ָ��
void sys_sem_free(sys_sem_t *sem)
{
	vSemaphoreDelete(*sem); 
	*sem = NULL;
}

//��ѯһ���ź�����״̬,��Ч����Ч
//sem:�ź���ָ��
//����ֵ:1,��Ч.
//      0,��Ч
int sys_sem_valid(sys_sem_t *sem)
{
	if(*sem != NULL)
    return 1;
  else
    return 0;		
} 

//����һ���ź�����Ч
//sem:�ź���ָ��
void sys_sem_set_invalid(sys_sem_t *sem)
{
	*sem=NULL;
} 

//arch��ʼ��
void sys_init(void)
{ 
    //����,�����ڸú���,�����κ�����
} 

TaskHandle_t LWIP_ThreadHandler;
//����һ���½���
//*name:��������
//thred:����������
//*arg:�����������Ĳ���
//stacksize:��������Ķ�ջ��С
//prio:������������ȼ�
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	taskENTER_CRITICAL();  //�����ٽ��� 
	xTaskCreate((TaskFunction_t)thread,
						(const char*  )name,
						(uint16_t     )stacksize,
						(void*        )NULL,
						(UBaseType_t  )prio,
						(TaskHandle_t*)&LWIP_ThreadHandler);//����TCP IP�ں����� 
	taskEXIT_CRITICAL();  //�˳��ٽ���
	return 0;
} 

//lwip��ʱ����
//ms:Ҫ��ʱ��ms��
void sys_msleep(u32_t ms)
{
	delay_ms(ms);
}

//��ȡϵͳʱ��,LWIP1.4.1���ӵĺ���
//����ֵ:��ǰϵͳʱ��(��λ:����)
u32_t sys_now(void)
{
	u32_t lwip_time;
	lwip_time=(HAL_GetTick()*1000/configTICK_RATE_HZ+1);//��������ת��ΪLWIP��ʱ��MS
	return lwip_time; 		//����lwip_time;
}

/********************************--ADD--************************************/
/*����cc.h��SYS_ARCH_PROTECT(lev)*/
uint32_t Enter_Critical(void)
{
	if(SCB_ICSR_REG&0xFF)//���ж���
	{
		return taskENTER_CRITICAL_FROM_ISR();
	}
	else  //���߳�
	{
		taskENTER_CRITICAL();
		return 0;
	}
}
/*����cc.YS_ARCH_UNPROTECT(lev)*/
void Exit_Critical(uint32_t lev)
{
	if(SCB_ICSR_REG&0xFF)//���ж���
	{
		taskEXIT_CRITICAL_FROM_ISR(lev);
	}
	else  //���߳�
	{
		taskEXIT_CRITICAL();
	}
}


