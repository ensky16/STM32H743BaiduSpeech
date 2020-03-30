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
#define ERROR_TOKEN_PARSE_SCOPE 2
#define ERROR_TOKEN_PARSE_ACCESS_TOKEN 3
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


char *read_file_data(char *fp, int *content_len);
int obtain_json_str(const char *json, const char *key, char *value, int value_size);
int obtain_json_str_update(const char *json, const char *key, char *value, int value_size) ;
int parse_token(const char *response, const char *scope, char *token); 
unsigned char *base64_encode(unsigned char *str, long str_len);
unsigned char *base64_decode( char *code) ;
int getBaiduSpeechJson(char * aucBuf, char * token, char * speech, int speechLen);
int postRequest(PostDataTypeDef stPostData);
void postRequestDemoMain(void);
void getToken(char * tokenBuf);
void run_asr(  char * audio_data, unsigned int u32SpeechDataLen);

const int BUFFER_ERROR_SIZE = 20;
char g_demo_error_msg[20] = {0};
char *scope = "audio_voice_assistant_get"; 
const char API_TOKEN_URL[] = "http://openapi.baidu.com/oauth/2.0/token";
const int MAX_TOKEN_SIZE = 100;
char tokenBuf[200];

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

#define SPEECH_DEMO_DATA_DECODE_LEN 129600
char speechDemoData[1]; //for debug we can add one demo data
extern void audio_play_individual(u8 * au08WaveCache);

int baiduSpeechMain()
{
    postDataBuf=(char *)mymalloc(SRAMEX, POST_DATA_BUF_LEN+1);
    if(postDataBuf==NULL)
        return 1;
    postDataWrkBuf=(char *)mymalloc(SRAMEX, POST_DATA_WRK_BUF_LEN+1);
    if(postDataWrkBuf==NULL)
        return 1;
        
	wav_recorder();

    myfree(SRAMEX, postDataBuf);
    myfree(SRAMEX, postDataWrkBuf);
	return 0;
}


u8 * sendWavDataToServer(unsigned char * audio_data, unsigned int u32SpeechDataLen, unsigned int * u32RobotLen)
{	 
	char sendDataLen[10];
    char strBuf2[200];
    char strBuf3[200];
    char strBuf4[200];
	char strBuf[200];
    PostDataTypeDef postDataObj;
	int content_len = 0, i=0;
    char * acBaiduTTSBase64Decode; 
     
    char * acBase64Buf=(char *)base64_encode(audio_data, u32SpeechDataLen);
  	   
	if(u32SpeechDataLen>POST_DATA_BUF_LEN)
	{
		printf("\n audio_data_base64_len>POST_DATA_BUF_LEN \n");
		return NULL;
	}
     	
    content_len=u32SpeechDataLen;

	memset(postDataBuf, 0, POST_DATA_BUF_LEN);
    memcpy(postDataBuf, acBase64Buf,  strlen(acBase64Buf));
    myfree(SRAMEX, acBase64Buf);

	memset(strBuf, 0, 200);
	//set the sub url
	strcat(strBuf, "/php2/post.php?id=wavdata");
	
	memset(postDataWrkBuf, 0, POST_DATA_WRK_BUF_LEN);
	sprintf(sendDataLen,"%d",(int)(strlen(postDataBuf)));

	postDataObj.socketConectIP="192.168.3.102";  //ip get from  vop.baidu.com
	postDataObj.port=80;
	postDataObj.host="192.168.3.102";
	postDataObj.suburl=strBuf;
	postDataObj.contentType="application/json";
    postDataObj.sendDataByPost=(char *)postDataBuf;
    postDataObj.sendDataLength=sendDataLen;
    postDataObj.postWorkingBuf=postDataWrkBuf;
    postDataObj.postWorkBufLen=POST_DATA_WRK_BUF_LEN;
	//printf("\n **********run_asr start ******************\n");
	postRequest(postDataObj);
	//printf("\n **********run_asr end ******************\n");
   
//    printf("%s\r\n", postDataObj.postWorkingBuf);
    memset(strBuf, 0, 200);
    memset(strBuf2, 0, 200);
    memset(strBuf3, 0, 200);
    memset(strBuf4, 0, 200);
    memset(postDataBuf, 0, POST_DATA_BUF_LEN+1);
    obtain_json_str(postDataObj.postWorkingBuf, "asrResultUtf8", strBuf, 200);
   	obtain_json_str(postDataObj.postWorkingBuf, "asrResultGBK", strBuf2, 200);
    obtain_json_str(postDataObj.postWorkingBuf, "robosResultUtf8", strBuf3, 200);
    obtain_json_str(postDataObj.postWorkingBuf, "robosResultGBK", strBuf4, 200);
    obtain_json_str(postDataObj.postWorkingBuf, "robosResultAudio", postDataBuf, POST_DATA_BUF_LEN);
    acBaiduTTSBase64Decode=(char * )base64_decode(postDataBuf);
    printf("\r\n ***********************************\r\n");
	printf("ST H743 run_asr result: %s \r\n", strBuf);
    printf("ST H743 run_asr robot result: %s \r\n", strBuf3);
    printf("ST H743 GBKResult length: %d \r\n", strlen(strBuf2));
    printf("ST H743 asrResultGBK is \r\n");
    for(i=0; i<strlen(strBuf2); i++)
    {
        printf(" %02X", strBuf2[i]);
    }
    printf("\r\n ST H743 asrResultGBK end \r\n");
    
    printf("ST H743 robosResultAudio is \r\n\r\n");

    content_len=strlen(postDataBuf);
    printf("content_len is %X \r\n", content_len);
    
    Show_Str(1, 340, 200, 16, "baidu speech result: ", 16, 0); 
    LCD_Fill(0, 360, 271, 479, WHITE);  //clear the previously data
 	Show_Str(1, 360, 270 ,479-360, (u8 * )strBuf2, 16, 0); 
    Show_Str(1, 380, 270 ,479-360, (u8 * )strBuf4, 16, 0); 

    *u32RobotLen=content_len;
    printf("u32RobotLen is %X \r\n", *u32RobotLen);
 
   return (u8 * )acBaiduTTSBase64Decode;   
}


void run_asr( char * audio_data, unsigned int u32SpeechDataLen)
{	 
	char sendDataLen[10];

	char strBuf[200];
    PostDataTypeDef postDataObj;
	int audio_data_base64_len=0;

	int content_len = 0;
 
    char * audio_data_base64=(char *)base64_encode((unsigned char * )audio_data, u32SpeechDataLen);

	audio_data_base64_len=strlen(speechDemoData);
 	
    content_len=u32SpeechDataLen;
    
	if(audio_data_base64_len>POST_DATA_BUF_LEN)
	{
		printf("\n audio_data_base64_len>POST_DATA_BUF_LEN \n");
		return ;
	}

	memset(postDataBuf, 0, POST_DATA_BUF_LEN);
	getBaiduSpeechJson(postDataBuf, tokenBuf, audio_data_base64 ,content_len);
 
	memset(strBuf, 0, 200);
	//set the sub url
	strcat(strBuf, "/server_api");
	
	memset(postDataWrkBuf, 0, POST_DATA_WRK_BUF_LEN);
	sprintf(sendDataLen,"%d",(int)(strlen(postDataBuf)));

	postDataObj.socketConectIP="39.156.66.99";  //ip get from  vop.baidu.com
	postDataObj.port=80;
	postDataObj.host="vop.baidu.com";
	postDataObj.suburl=strBuf;
	postDataObj.contentType="application/json";
    postDataObj.sendDataByPost=(char *)postDataBuf;
    postDataObj.sendDataLength=sendDataLen;
    postDataObj.postWorkingBuf=postDataWrkBuf;
    postDataObj.postWorkBufLen=POST_DATA_WRK_BUF_LEN;
	//printf("\n **********run_asr start ******************\n");
	postRequest(postDataObj);
	//printf("\n **********run_asr end ******************\n");
	
	//get result
    memset(strBuf, 0, 200);
	obtain_json_str_update(postDataWrkBuf, "result", strBuf, 200);
	printf("\n run_asr result: %s \n", strBuf);
   
    Show_Str(10,300,200,16, "baidu speech result: ", 16, 0); 
 
}


int getBaiduSpeechJson(char * aucBuf, char * token, char * speech, int speechLen)
{
	char sendDataLen[10]; 
	sprintf(sendDataLen,"%d", speechLen);
	
	//set format
	strcat(aucBuf, "{");
	strcat(aucBuf, "\"format\":\"pcm\"");
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"rate\":16000");
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"dev_pid\":1537");
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"channel\":1");
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"token\":\"");
	strcat(aucBuf, token);
	strcat(aucBuf, "\",");
	strcat(aucBuf, "\"cuid\":\"baidu_workshop\"");
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"len\":");
	strcat(aucBuf, sendDataLen);
	strcat(aucBuf, ",");
	strcat(aucBuf, "\"speech\":\"");
	strcat(aucBuf, speech);
	strcat(aucBuf, "\"");
	strcat(aucBuf, "}");
    return 0;
}


void getToken(char * tokenBuf)
{
	char *scope = "audio_voice_assistant_get"; 
	char tempBuf2[10];
    PostDataTypeDef postDataObj;
	char sendData[10];
	char strBuf[200];
    char * api_key = "kVcnfD9iW2XVZSMaLMrtLYIz";
    char * secret_key = "O9o1O213UgG5LFn0bDGNtoRN3VWl2du6";
	char * getTokenUrl="/oauth/2.0/tokengrant_type=client_credentials&client_id=";
	
	memset(strBuf, 0, 200);
	memset(sendData, 0, 10);
	strcat(strBuf, getTokenUrl);
	strcat(strBuf, api_key);
	strcat(strBuf, "&client_secret=");
	strcat(strBuf, secret_key);
	
	memset(postDataWrkBuf, 0, POST_DATA_WRK_BUF_LEN);
	sprintf(tempBuf2,"%d",(int)(strlen(sendData)));

	postDataObj.socketConectIP="39.156.66.111";  //ip get from  openapi.baidu.com
	postDataObj.port=80;
	postDataObj.host="openapi.baidu.com";
	postDataObj.suburl=strBuf;
	postDataObj.contentType=" text/plain ";
    postDataObj.sendDataByPost=(char *)sendData;
    postDataObj.sendDataLength="0";
    postDataObj.postWorkingBuf=postDataWrkBuf;
    postDataObj.postWorkBufLen=POST_DATA_WRK_BUF_LEN;
	//printf("\n*************get token start***************\n");
	postRequest(postDataObj);
	//printf("\n*************get token end***************\n");
	
	memset(tokenBuf, 0, sizeof(tokenBuf));
	parse_token(postDataObj.postWorkingBuf,  scope, tokenBuf);
		
	//printf("\n*************token start***************\n");
	//printf("%s",tokenBuf );
	//printf("\n*************token end***************\n");
}
 
int postRequest(PostDataTypeDef stPostData)
{
	struct timeval  tv;
	fd_set   t_set1;
	int sockfd, ret, i, h, u32ReadCounter=0;
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
        delay_ms(100);
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
            u32ReadCounter=0;
            
            #ifdef DEBUG_SOCKET_POST
            printf("\r\n socket reading start \r\n");
            #endif
            
            while(1)
            {
                i = lwip_recv(sockfd, &stPostData.postWorkingBuf[u32ReadCounter], stPostData.postWorkBufLen, 0);	//接收数据
                u32ReadCounter+=i;
                if(u32ReadCounter>stPostData.postWorkBufLen)
                {
                    printf("socket read error !!! \r\n");
                    printf("socket read error, read buffer is full \r\n");                   
                }

                if (i==0) //read finish
                {  
                    #ifdef DEBUG_SOCKET_POST
                    printf("read finish \r\n");
                    #endif
                    break;
                }
            }
             
			#ifdef DEBUG_SOCKET_POST
			//printf("%s\r\n", stPostData.postWorkingBuf);
			#endif
			break;		 
		}
	}
 
	close(sockfd);

	return 0;
}

 

/**
*get token and check the scope
*@param response, set the response of post data
*@param scope, eg.  "audio_voice_assistant_get"
*@param token, this is token output buffer
*
*/
int parse_token(const char *response, const char *scope, char *token) {
    char scopes[400];
    //  ====   token =========
    int res = obtain_json_str(response, "access_token", token, MAX_TOKEN_SIZE);
    if (res != RETURN_OK) {
        snprintf(g_demo_error_msg, BUFFER_ERROR_SIZE, "parse token error: %s\n", response);
        return ERROR_TOKEN_PARSE_ACCESS_TOKEN;
    }

    // ==== scope =========

    res = obtain_json_str(response, "scope", scopes, 400);
    if (res != RETURN_OK) {
        snprintf(g_demo_error_msg, BUFFER_ERROR_SIZE, "parse scope error: %s\n", response);
        return ERROR_TOKEN_PARSE_ACCESS_TOKEN;
    }
    if (strlen(scope) > 0) {
        char *scope_pos = strstr(scopes, scope);
        if (scope_pos == NULL) {
            snprintf(g_demo_error_msg, BUFFER_ERROR_SIZE, "scope: %s not exist in:%s \n", scope,
                     response);
            return ERROR_TOKEN_PARSE_SCOPE;
        }
    }
    return RETURN_OK;
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
    if (start == NULL) { // 
        fprintf(stderr, "%s key not exist\n", key);
        return RETURN_ERROR;
    }

    start += strlen(search_key);
    end = strstr(start, "\"");
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
    if (start == NULL) { // 
        fprintf(stderr, "%s key not exist\n", key);
        return RETURN_ERROR;
    }

    start += strlen(search_key);
    end = strstr(start, "\"");
    // string
    copy_size = (value_size < end - start) ? value_size : end - start;
    snprintf(value, copy_size + 1, "%s", start);
    return RETURN_OK;
}

  
unsigned char *base64_encode(unsigned char *str, long str_len)  
{  
    long len;  
    unsigned char *res;  
    int i,j;  
    unsigned char *base64_table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  
  
    if(str_len % 3 == 0)  
        len=str_len/3*4;  
    else  
        len=(str_len/3+1)*4;  
	    
    res= mymalloc(SRAMEX, len+1);
    res[len]='\0';  
   
    for(i=0,j=0;i<len-2;j+=3,i+=4)  
    {  
        res[i]=base64_table[str[j]>>2];  
        res[i+1]=base64_table[(str[j]&0x3)<<4 | (str[j+1]>>4)]; 
        res[i+2]=base64_table[(str[j+1]&0xf)<<2 | (str[j+2]>>6)]; 
        res[i+3]=base64_table[str[j+2]&0x3f];  
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
 
const int table[]={0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,62,0,0,0,
         63,52,53,54,55,56,57,58,
         59,60,61,0,0,0,0,0,0,0,0,
         1,2,3,4,5,6,7,8,9,10,11,12,
         13,14,15,16,17,18,19,20,21,
         22,23,24,25,0,0,0,0,0,0,26,
         27,28,29,30,31,32,33,34,35,
         36,37,38,39,40,41,42,43,44,
         45,46,47,48,49,50,51
           };  
unsigned char *base64_decode( char *code)  
{  

    long len;  
    long str_len;  
    unsigned char *res;  
    int i,j;  
  
    len=strlen(code);  

    if(strstr(code,"=="))  
        str_len=len/4*3-2;  
    else if(strstr(code,"="))  
        str_len=len/4*3-1;  
    else  
        str_len=len/4*3;  
  
    res= mymalloc(SRAMEX, len+1);
    memset(res, 0, len+1);
    res[str_len]='\0';  
  
    for(i=0,j=0;i < len-2;j+=3,i+=4)  
    {  
        res[j]=((unsigned char)table[code[i]])<<2 | (((unsigned char)table[code[i+1]])>>4); 
        res[j+1]=(((unsigned char)table[code[i+1]])<<4) | (((unsigned char)table[code[i+2]])>>2); 
        res[j+2]=(((unsigned char)table[code[i+2]])<<6) | ((unsigned char)table[code[i+3]]); 
    }  
  
    return res;  
  
}  

