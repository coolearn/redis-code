
typedef struct send_info
{
	char info_from[20]; //发送者ID
	char info_to[20]; //接收者ID
	int info_length; //发送的消息主体的长度
	char info_content[1024]; //消息主体
} sendInfo; 