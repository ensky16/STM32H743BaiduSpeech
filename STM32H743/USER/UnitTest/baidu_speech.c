#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "main.h"
#include "malloc.h"
#include "string.h"
#include "delay.h"
#include "lcd.h"
#include "text.h"
#include "recorder.h"

#define RETURN_OK 0
#define RETURN_ERROR  1
typedef struct
{
	char * socketConectIP;  //used for socket connect()
	int port;               //used for socket connect port
	char * host;			//post header 
	char * suburl;
	char * contentType;
	char * sendDataByPost;  //send data by post
	char * sendDataLength;   //send data length
	char * postWorkingBuf;  //send data and receive data temp buffer.  send data: including post haeader, post body ......
	int postWorkBufLen;

}PostDataTypeDef;

int obtain_json_str(const char *json, const char *key, char *value, int value_size);
int obtain_json_str_update(const char *json, const char *key, char *value, int value_size); 
unsigned char *base64_encode(unsigned char *str, long str_len); 
int postRequest(PostDataTypeDef stPostData);

const int POST_DATA_BUF_LEN  =3000*1024;;
char * postDataBuf ;
const int POST_DATA_WRK_BUF_LEN  =3000*1024;;
char * postDataWrkBuf;

#define BUFSIZE 1024

//#define DEBUG_PRINTF
//#define DEBUG_GET_TOKEN
#define DEBUG_SOCKET_POST

int funcFlag=0;
#define RUN_ASR_FLAG  0x10

//after base64 decode , length is 129600
#define SPEECH_DEMO_DATA_DECODE_LEN 129600
 
int baiduSpeechMain()
{
    postDataBuf=(char *)mymalloc(SRAMEX, POST_DATA_BUF_LEN+1);
    if(postDataBuf==NULL)
        return 1;
    postDataWrkBuf=(char *)mymalloc(SRAMEX, POST_DATA_WRK_BUF_LEN+1);
    if(postDataWrkBuf==NULL)
        return 1;
        
	wav_recorder();

	return 0;
}


void sendWavDataToServer(unsigned char * audio_data, unsigned int u32SpeechDataLen)
{	 
	char sendDataLen[10];
    char strBuf2[200];
	char strBuf[200];
    PostDataTypeDef postDataObj;
	int  i=0;
     
    char * acBase64Buf=(char *)base64_encode(audio_data, u32SpeechDataLen);
  	   
	if(u32SpeechDataLen>POST_DATA_BUF_LEN)
	{
		printf("\n audio_data_base64_len>POST_DATA_BUF_LEN \n");
		return ;
	}
     	
	memset(postDataBuf, 0, POST_DATA_BUF_LEN);
    memcpy(postDataBuf, acBase64Buf,  strlen(acBase64Buf));
    myfree(SRAMEX, acBase64Buf);

	memset(strBuf, 0, 200);
	//set the sub url
	strcat(strBuf, "/php2/post.php?id=wavdata");
	
	memset(postDataWrkBuf, 0, POST_DATA_WRK_BUF_LEN);
	sprintf(sendDataLen,"%d",(int)(strlen(postDataBuf)));

	postDataObj.socketConectIP="192.168.1.6";  //ip get from  vop.baidu.com
	postDataObj.port=80;
	postDataObj.host="192.168.1.6";
	postDataObj.suburl=strBuf;
	postDataObj.contentType="application/json";
    postDataObj.sendDataByPost=(char *)postDataBuf;
    postDataObj.sendDataLength=sendDataLen;
    postDataObj.postWorkingBuf=postDataWrkBuf;
    postDataObj.postWorkBufLen=POST_DATA_WRK_BUF_LEN;
	//printf("\n **********run_asr start ******************\n");
	postRequest(postDataObj);
	//printf("\n **********run_asr end ******************\n");
   
    printf("%s\r\n", postDataObj.postWorkingBuf);
    memset(strBuf, 0, 200);
    memset(strBuf2, 0, 200);
    obtain_json_str_update(postDataObj.postWorkingBuf, "UTF8Result", strBuf, 200);
   	obtain_json_str_update(postDataObj.postWorkingBuf, "GBKResult", strBuf2, 200);
    printf("\r\n ***********************************\r\n");
	printf("ST H743 run_asr result: %s \r\n", strBuf);
    printf("ST H743 GBKResult length: %d \r\n", strlen(strBuf2));
    printf("ST H743 data is \r\n");
    for(i=0; i<strlen(strBuf2); i++)
    {
        printf(" %02X", strBuf2[i]);
    }
    printf("\r\n ST H743 data end is \r\n");
    
    Show_Str(1, 340, 200, 16, "baidu speech result: ", 16, 0); 
    LCD_Fill(0, 360, 271, 479, WHITE);  //clear the previously data
 	Show_Str(1, 360, 270 ,479-360, (u8 *)strBuf2, 16, 0); 
}

  
int postRequest(PostDataTypeDef stPostData)
{
	struct timeval  tv;
	fd_set   t_set1;
	int sockfd, ret, i, h;
	char tempBuf[8]={""};
 	 
	struct sockaddr_in servaddr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
		#ifdef DEBUG_SOCKET_POST
		printf("create socket error \r\n");
		#endif
		return 1;
	};
			
	//set servaddr as 0x00
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(stPostData.socketConectIP);	 
    servaddr.sin_port = htons(stPostData.port);		 
        
	#ifdef DEBUG_SOCKET_POST
	printf("stPostData.port is %d \r\n",stPostData.port);
	printf("stPostData.socketConectIP is %s \r\n",stPostData.socketConectIP);
	#endif
    
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		#ifdef DEBUG_SOCKET_POST
		printf(" connect error!\n");
		#endif
		return 1;
	}
	#ifdef DEBUG_SOCKET_POST
	printf("conect success \r\n");
    printf("\r\n");
	#endif
  
	memset(stPostData.postWorkingBuf, 0, stPostData.postWorkBufLen);
	strcat(stPostData.postWorkingBuf, "POST ");
	strcat(stPostData.postWorkingBuf, stPostData.suburl);
	strcat(stPostData.postWorkingBuf, " HTTP/1.1\r\n");

	//add host
	strcat(stPostData.postWorkingBuf, "Host:");
	strcat(stPostData.postWorkingBuf, stPostData.host); 
	strcat(stPostData.postWorkingBuf, "\r\n");

	//add port   
	sprintf(tempBuf,"%d",stPostData.port);
	strcat(stPostData.postWorkingBuf, "port:");
	strcat(stPostData.postWorkingBuf, tempBuf);
	strcat(stPostData.postWorkingBuf, "\r\n");   

	//add Content-Type
	strcat(stPostData.postWorkingBuf, "Content-Type:");
	strcat(stPostData.postWorkingBuf, stPostData.contentType);
	strcat(stPostData.postWorkingBuf, "\r\n");
 
	//add data length
	strcat(stPostData.postWorkingBuf, "Content-Length: ");	
	strcat(stPostData.postWorkingBuf, stPostData.sendDataLength);
    strcat(stPostData.postWorkingBuf, "\r\n");  //here should use "\n\n", fixed, can not use one "\n", can not use three "\n"
    strcat(stPostData.postWorkingBuf, "\r\n");  //here should use "\n\n" again, if we do not add this, send will fail
 
	strcat(stPostData.postWorkingBuf, stPostData.sendDataByPost);
 
    #ifdef DEBUG_SOCKET_POST
    printf("\r\n *************send post data is *********************** \r\n");
    //printf("%s", stPostData.postWorkingBuf);
    printf("\r\n *************send post data end *********************** \r\n");
    #endif
    
	ret = write(sockfd,stPostData.postWorkingBuf,strlen(stPostData.postWorkingBuf));
		
	if (ret < 0) {
		#ifdef DEBUG_SOCKET_POST
		printf("write fail \r\n");
		#endif
		return 1;
	}else{
		#ifdef DEBUG_SOCKET_POST
		printf("write success, send %d bytes \r\n", ret);
		#endif
	}
 
	while(1)
	{		 
        FD_ZERO(&t_set1);
        FD_SET(sockfd, &t_set1);
        delay_ms(2000);
		tv.tv_sec= 0;
		tv.tv_usec= 0;
		h= 0;

		h= select(sockfd +1, &t_set1, NULL, NULL, &tv);
		
		if (h == 0) 
		{
			#ifdef DEBUG_SOCKET_POST
			printf("select return 0, continue \r\n");
			#endif
			continue;
		}
		
		if (h < 0) 
		{
			close(sockfd);
			#ifdef DEBUG_SOCKET_POST
			printf("select() return -1, close socket \r\n");
			#endif
			return -1;
		};

		if (h > 0)
		{		 
			memset(stPostData.postWorkingBuf, 0, stPostData.postWorkBufLen);
			i= read(sockfd, stPostData.postWorkingBuf, stPostData.postWorkBufLen);
			if (i==0){
				close(sockfd);
				#ifdef DEBUG_SOCKET_POST
				printf("remote socket closed, thread closed \r\n");
				#endif
				return -1;
			}
			#ifdef DEBUG_SOCKET_POST
			printf("%s\r\n", stPostData.postWorkingBuf);
			#endif
			break;		 
		}
	}
 
	close(sockfd);

	return 0;
}


int obtain_json_str_update(const char *json, const char *key, char *value, int value_size) 
{
    int copy_size ;
    char *start;
    char *end ;
    int len = 4 + strlen(key) + 1;
    char search_key[200];
    if(len>200)
    {
        printf("\n !!! error out of array index, obtain_json_str_update\n");
        return 1;
    }

    snprintf(search_key, len, "\"%s\":\"", key);
    start = strstr(json, key);
    if (start == NULL) { // ????
        fprintf(stderr, "%s key not exist\n", key);
        return RETURN_ERROR;
    }

    start += strlen(search_key);
    end = strstr(start, "\"");
    // ??????string
    copy_size = (value_size < end - start) ? value_size : end - start;
    snprintf(value, copy_size + 1, "%s", start);
    return RETURN_OK;
}

int obtain_json_str(const char *json, const char *key, char *value, int value_size) 
{
    char *end;
    int copy_size;
    char *start;
    char search_key[200];
    int len = 4 + strlen(key) + 1;

    if(len>200)
    {
        printf("\n !!! error out of array index, obtain_json_str_update\n");
        return 1;
    }

    snprintf(search_key, len, "\"%s\":\"", key);
    start = strstr(json, search_key);
    if (start == NULL) { // ????
        fprintf(stderr, "%s key not exist\n", key);
        return RETURN_ERROR;
    }

    start += strlen(search_key);
    end = strstr(start, "\"");
    // ??????string
    copy_size = (value_size < end - start) ? value_size : end - start;
    snprintf(value, copy_size + 1, "%s", start);
    return RETURN_OK;
}

  
unsigned char *base64_encode(unsigned char *str, long str_len)  
{  
    long len;  
    //long str_len;  
    unsigned char *res;  
    int i,j;  
//??base64???  
    unsigned char *base64_table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  
  
//????base64?????????  
    //str_len=strlen(str);  
    if(str_len % 3 == 0)  
        len=str_len/3*4;  
    else  
        len=(str_len/3+1)*4;  
	  
    //res=malloc(sizeof(unsigned char)*len+1);  
    res= mymalloc(SRAMEX, len+1);
    res[len]='\0';  
  
//?3?8??????????  
    for(i=0,j=0;i<len-2;j+=3,i+=4)  
    {  
        res[i]=base64_table[str[j]>>2]; //?????????6???????????  
        res[i+1]=base64_table[(str[j]&0x3)<<4 | (str[j+1]>>4)]; //?????????????????4???????????????  
        res[i+2]=base64_table[(str[j+1]&0xf)<<2 | (str[j+2]>>6)]; //????????4?????????2?????????????  
        res[i+3]=base64_table[str[j+2]&0x3f]; //?????????6????????  
    }  
  
    switch(str_len % 3)  
    {  
        case 1:  
            res[i-2]='=';  
            res[i-1]='=';  
            break;  
        case 2:  
            res[i-1]='=';  
            break;  
    }  
  
    return res;  
}  
 




