//
// Created by champer on 27/04/17.
//

#include "socket.h"
#include "modbus_data.h"
#include "open62541.h"
#include "get_config.h"


#define conc(x,y) x##y

int server_socket = -1;
int IPv6_Client_SocketFd = -1;

uint16_t DEVICE_NUM = 0;
uint16_t TIMEOUT = 0;
uint16_t DELAY = 0;

UA_Boolean running = true;
UA_Server *server;

typedef struct {
    uint16_t count;
    uint8_t len;
    uint8_t type;
}Timeout_Count;

Timeout_Count to_count[10];

typedef struct{
    UA_UInt16 addr;
    UA_UInt16 type;
    void* data;
    char nodeid[20];
}Opcua_Node;
Opcua_Node OpcuaNodeArray[50];// the array of node


#if 0
typedef struct{
    UA_UInt16 addr;
    UA_UInt16 type;
    UA_Float data;
    //void* data;
    char nodeid[20];
}Opcua_FloatNode;
Opcua_FloatNode OpcuaFloatNodeArray[50];// the array of float node

typedef struct{
    UA_UInt16 addr;
    UA_UInt16 type;
    UA_Double data;
    char nodeid[20];
}Opcua_DoubleNode;
Opcua_DoubleNode OpcuaDoubleNodeArray[50];// the array of Double node

typedef struct{
    UA_UInt16 addr;
    UA_UInt16 type;
    UA_UInt16 data;
    char nodeid[20];
}Opcua_UintNode;
Opcua_UintNode OpcuaUintNodeArray[50];// the array of Uint node
#endif
UA_UInt16 OpcuaNodeCount=0;//the num of node

float DIANBIAO_data[5] = {0.0};
double ELECFLOW_datalf[2] = {0.0};
float ELECFLOW_dataf;

void close_sigint(int dummy)
{
    if (server_socket != -1) {
        close(server_socket);
    }
    modbus_free(ctx);
    modbus_mapping_free(mb_mapping);

    exit(dummy);
}

void swap(uint8_t *a, uint8_t *b){
    uint8_t temp;
    temp = *a;
    *a = *b;
    *b = temp;
}

void DIANBIAO_Hex2Float(uint8_t *srcbuf, float *dstbuf){
    int a[5] = {0};
    for(int i=0; i<5; i++){
        swap(&srcbuf[7+i*4], &srcbuf[9+i*4]);
        swap(&srcbuf[8+i*4], &srcbuf[10+i*4]);
    }
    for(int i=0; i<5; i++){
        a[i] = (srcbuf[7+i*4]<<24) + (srcbuf[8+i*4]<<16) + (srcbuf[9+i*4]<<8) + srcbuf[10+i*4];
        dstbuf[i] = *(float*)&a[i];
    }
}


void *Modbus_Server(void *arg)
{
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;

    ctx = modbus_new_tcp(INADDR_ANY, MODBUS_SERVER_PORT);

    mb_mapping = modbus_mapping_new_start_address(
            UT_BITS_ADDRESS, UT_BITS_NB,
            UT_INPUT_BITS_ADDRESS, UT_INPUT_BITS_NB,
            UT_REGISTERS_ADDRESS, UT_REGISTERS_NB_MAX,
            UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB
    );
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    /* Initialize values of INPUT REGISTERS */
    for (int i=0; i < UT_INPUT_REGISTERS_NB; i++) {
        mb_mapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];
    }

    server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
    if (server_socket == -1) {
        fprintf(stderr, "Unable to listen TCP connection\n");
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, close_sigint);

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket;

    for (;;) {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
            perror("Server select() failure.");
            close_sigint(1);
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {

            if (!FD_ISSET(master_socket, &rdset)) {
                continue;
            }

            if (master_socket == server_socket) {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) {
                    perror("Server accept() error");
                } else {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    printf("New connection from %s:%d on socket %d\n",
                           inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            } else {
                modbus_set_socket(ctx, master_socket);
                rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    modbus_reply(ctx, query, rc, mb_mapping);
                } else if (rc == -1) {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    printf("Connection closed on socket %d\n", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) {
                        fdmax--;
                    }
                }
            }
        }
    }
}

void *IPv6_Client(void *arg) {
    //Parse_Config_File();
    struct sockaddr_in clientAddr;
    int nread;
    uint8_t IPv6_Resp[IPV6_RESP_LEN];
    fd_set rset;

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(IOT_DAEMON_PORT);
    clientAddr.sin_addr.s_addr = inet_addr(IOT_DAEMON_ADDR);
    IPv6_Client_SocketFd = socket(AF_INET, SOCK_STREAM, 0);
    //setsockopt(IPv6_Client_SocketFd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

    if (IPv6_Client_SocketFd < 0) {
        perror("ipv6 client socket");
        exit(EXIT_FAILURE);
    }
    if (connect(IPv6_Client_SocketFd, (struct sockaddr *) &clientAddr, sizeof(clientAddr)) < 0) {
        perror("IPv6 client connect");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&rset);
    while(1){

        FD_SET(IPv6_Client_SocketFd, &rset);
        //int recvd = recv(IPv6_Client_SocketFd, IPv6_Resp, IPV6_RESP_LEN, 0);
        if (-1 == select(IPv6_Client_SocketFd + 1, &rset, NULL, NULL, NULL)){
            perror("select");
        }
        else{
            if (FD_ISSET(IPv6_Client_SocketFd, &rset)){
                nread = read(IPv6_Client_SocketFd, IPv6_Resp, IPV6_RESP_LEN);
                if (-1 == nread) {
                    perror("read");
                } else {
                    Parse_IPv6_Resp(IPv6_Resp, nread);
                }
            }
            //usleep((useconds_t)(DELAY*1000));
        }
    }
}

typedef union _FLOAT2HEX {
	float f;
	uint16_t u16[2]; 
}FLOAT2HEX;

int Parse_IPv6_Resp(uint8_t *buf, int len)
{
    FLOAT2HEX temp_float;
		
    if(buf[0] == 0xA1 && buf[1] == 0xA2 && buf[2] == 0x00 && buf[3] == 0xAA && buf[4] > 0 && buf[4] <=10)
    {

        if(Get_Data_Type(buf) == CO && len == CO_PACKET_LEN)
        {
            printf("get node%d CO data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == DUST && len == DUST_PACKET_LEN)
        {
            printf("get node%d DUST data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == LIAOWEI && len == LIAOWEI_PACKET_LEN)
        {
            printf("get node%d LIAOWEI data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)((buf[7]<<8)+buf[8]);   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == DIANBIAO && len == DIANBIAO_PACKET_LEN)
        {
            DIANBIAO_Hex2Float(buf, DIANBIAO_data);
            printf("get node%d DIANBIAO data:%.6f, %.6f, %.6f, %.6f, %.6f\n", buf[4],
                   DIANBIAO_data[0], DIANBIAO_data[1],  DIANBIAO_data[2],  DIANBIAO_data[3],  DIANBIAO_data[4]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            for(int i=0; i<5; i++){
                UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2 + i] = (uint16_t)((buf[7+i*2]<<8)+buf[8+i*2]);   //data
            }
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == FLOW && len == FLOW_PACKET_LEN)
        {
            printf("get node%d FLOW data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)((buf[7]<<8)+buf[8]);   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == ENCODER && len == ENCODER_PACKET_LEN)
        {
            printf("get node%d ENCODER direction:%d,sign:%d,data:%d\n", buf[4], buf[7], buf[8], (buf[9]<<8)+buf[10]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)(buf[7]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 3] = (uint16_t)(buf[8]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 4] = (uint16_t)((buf[9]<<8)+buf[10]);
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == DISPLACE && len == DISPLACE_PACKET_LEN)
        {
            printf("get node%d DISPLACE data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == HUMITURE && len == HUMITURE_PACKET_LEN)
        {
            printf("get node%d temp data:%d, moisture data:%d\n", buf[4], (buf[7]<<8)+buf[8], (buf[9]<<8)+buf[10]);
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)((buf[7]<<8)+buf[8]);   //data
            temp_float.f= (float)((buf[9]<<8)+buf[10]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 3] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == WIND && len == WIND_PACKET_LEN)
        {
            printf("get node%d WIND data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
//            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 1000;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
        if(Get_Data_Type(buf) == FLOW_METER && len == ELEC_FLOW_PACKET_LEN)
        {
            uint64_t temp1 = buf[7]<<56|buf[8]<<48|buf[9]<<40|buf[10]<<32|buf[11]<<24|buf[12]<<16|buf[13]<<8|buf[14];
            uint64_t temp2 = buf[15]<<56|buf[16]<<48|buf[17]<<40|buf[18]<<32|buf[19]<<24|buf[20]<<16|buf[21]<<8|buf[22];
            uint32_t temp3 = buf[23]<<24|buf[24]<<16|buf[25]<<8|buf[26];
            printf("get node%d Elec flow data:%.6lf, %.6lf, %.6f\n", buf[4],
                   *(double*)&temp1, *(double*)&temp2, *(float*)&temp3 );
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            for(int i=0; i<5; i++){
                UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2 + i] = (uint16_t)((buf[7+i*2]<<8)+buf[8+i*2]);   //data
            }
            Opcua_Server_Parse(buf);
        }

        if(Get_Data_Type(buf) == SO2 && len == SO2_PACKET_LEN)
        {
            printf("get node%d SO2 data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }

        if(Get_Data_Type(buf) == TEMP && len == TEMP_PACKET_LEN)
        {
            printf("get node%d TEMP data:%d\n", buf[4], (buf[7]<<8)+buf[8]);
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[4];                     //addr
            //UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)(buf[5]);               //type
            temp_float.f= (float)((buf[7]<<8)+buf[8]) / 100;
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD ] = temp_float.u16[0];   //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = temp_float.u16[1];   //data
            Opcua_Server_Parse(buf);
        }
    }
    for (int i=0; i < UT_INPUT_REGISTERS_NB; i++) {
        mb_mapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];
    }

    return 0;
}

uint8_t Get_Data_Type(uint8_t *data)
{
    return data[5];
}



void  Opcua_Server_Parse(UA_Byte *opcuabuf)
{
    char nodeName[20];
    UA_NodeId nodeId;
    UA_UInt16 UinNodeData = 0;
    UA_Float FloatNodeData = 0;
    UA_Double DoubleNodeData =  0;
    char *p = NULL;
    int i = 0;
    int result = 0;
    memset(nodeName, '\0', 20);
    if(opcuabuf[5] == CO){
        strcat(nodeName,"CO_");
        p = strstr(nodeName,"CO_");
        if(p != NULL)
            sprintf(p,"CO_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == DUST){
        strcat(nodeName,"DUST_");
        p = strstr(nodeName,"DUST_");
        if(p != NULL)
            sprintf(p,"DUST_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }

    else if(opcuabuf[5] == LIAOWEI){
        strcat(nodeName,"LIAOWEI_");
        p = strstr(nodeName,"LIAOWEI_");
        if(p != NULL)
            sprintf(p,"LIAOWEI_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == DIANBIAO){
        strcat(nodeName,"DIANBIAO_");
        p = strstr(nodeName,"DIANBIAO_");
        if(p != NULL)
            sprintf(p,"DIANBIAO_%d",opcuabuf[4]);
        strcat(nodeName,"APOWER");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = DIANBIAO_data[0];
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"DIANBIAO_");
        p = strstr(nodeName,"DIANBIAO_");
        if(p != NULL)
            sprintf(p,"DIANBIAO_%d",opcuabuf[4]);
        strcat(nodeName,"RPOWER");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = DIANBIAO_data[1];
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"DIANBIAO_");
        p = strstr(nodeName,"DIANBIAO_");
        if(p != NULL)
            sprintf(p,"DIANBIAO_%d",opcuabuf[4]);
        strcat(nodeName,"AU");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = DIANBIAO_data[2];
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"DIANBIAO_");
        p = strstr(nodeName,"DIANBIAO_");
        if(p != NULL)
            sprintf(p,"DIANBIAO_%d",opcuabuf[4]);
        strcat(nodeName,"BU");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = DIANBIAO_data[3];
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"DIANBIAO_");
        p = strstr(nodeName,"DIANBIAO_");
        if(p != NULL)
            sprintf(p,"DIANBIAO_%d",opcuabuf[4]);
        strcat(nodeName,"CU");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = DIANBIAO_data[4];
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));


    }
    else if(opcuabuf[5] == FLOW){
        strcat(nodeName,"FLOW_");
        p = strstr(nodeName,"FLOW_");
        if(p != NULL)
            sprintf(p,"FLOW_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == ENCODER){
        strcat(nodeName,"ENCODER_");
        p = strstr(nodeName,"ENCODER_");
        if(p != NULL)
            sprintf(p,"ENCODER_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        if(opcuabuf == 1)
            FloatNodeData =  0 -(UA_UInt16)((opcuabuf[9]<<8)+opcuabuf[10]);
        else
            FloatNodeData =  (UA_UInt16)((opcuabuf[9]<<8)+opcuabuf[10]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == DISPLACE){
        strcat(nodeName,"DISPLACE_");
        p = strstr(nodeName,"DISPLACE_");
        if(p != NULL)
            sprintf(p,"DISPLACE_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == WIND){
        strcat(nodeName,"WIND_");
        p = strstr(nodeName,"WIND_");
        if(p != NULL)
            sprintf(p,"WIND_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == HUMITURE){
        strcat(nodeName,"HUMITURE_");
        p = strstr(nodeName,"HUMITURE_");
        if(p != NULL)
            sprintf(p,"HUMITURE_%d",opcuabuf[4]);
        strcat(nodeName,"_TEMP");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData=  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"HUMITURE_");
        p = strstr(nodeName,"HUMITURE_");
        if(p != NULL)
            sprintf(p,"HUMITURE_%d",opcuabuf[4]);
        strcat(nodeName,"_HUMI");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData=  (UA_UInt16)((opcuabuf[9]<<8)+opcuabuf[10]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));

    }
    else if(opcuabuf[5] == FLOW_METER){
        strcat(nodeName,"FLOW_METER_");
        p = strstr(nodeName,"FLOW_METER_");
        if(p != NULL)
            sprintf(p,"FLOW_METER_%d",opcuabuf[4]);
        strcat(nodeName,"_ZK");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        DoubleNodeData=  (UA_Double)(opcuabuf[7]<<56|opcuabuf[8]<<48|opcuabuf[9]<<40|opcuabuf[10]<<32|
                                     opcuabuf[11]<<24|opcuabuf[12]<<16|opcuabuf[13]<<8|opcuabuf[14]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddDoubleNode(nodeName);
        else
            Change_Server_DoubleValue(server, nodeId,DoubleNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"FLOW_METER_");
        p = strstr(nodeName,"FLOW_METER_");
        if(p != NULL)
            sprintf(p,"FLOW_METER_%d",opcuabuf[4]);
        strcat(nodeName,"_DK");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        DoubleNodeData=  (UA_Double)(opcuabuf[15]<<56|opcuabuf[16]<<48|opcuabuf[17]<<40|opcuabuf[18]<<32|
                                     opcuabuf[19]<<24|opcuabuf[20]<<16|opcuabuf[21]<<8|opcuabuf[22]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddDoubleNode(nodeName);
        else
            Change_Server_DoubleValue(server, nodeId,DoubleNodeData);
        memset(nodeName,'\0',strlen(nodeName));

        strcat(nodeName,"FLOW_METER");
        p = strstr(nodeName,"FLOW_METER_");
        if(p != NULL)
            sprintf(p,"FLOW_METER%d",opcuabuf[4]);
        strcat(nodeName,"_BK");
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        FloatNodeData = (UA_Float)(opcuabuf[23]<<24|opcuabuf[24]<<16|opcuabuf[25]<<8|opcuabuf[26]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddFloatNode(nodeName);
        else
            Change_Server_FloatValue(server, nodeId,FloatNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == SO2){
        strcat(nodeName,"SO2_");
        p = strstr(nodeName,"SO2_");
        if(p != NULL)
            sprintf(p,"SO2_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }
    else if(opcuabuf[5] == TEMP){
        strcat(nodeName,"TEMP_");
        p = strstr(nodeName,"TEMP_");
        if(p != NULL)
            sprintf(p,"TEMP_%d",opcuabuf[4]);
        nodeName[strlen(nodeName)] = '\0';
        nodeId = UA_NODEID_STRING(1, nodeName);
        UinNodeData =  (UA_UInt16)((opcuabuf[7]<<8)+opcuabuf[8]);
        result = Get_Node_Fromaddresspace(server, &nodeId);
        if(!result)
            AddUintNode(nodeName);
        else
            Change_Server_IntValue(server, nodeId,UinNodeData);
        memset(nodeName,'\0',strlen(nodeName));
    }

}

UA_UInt16 stringNodeIdToTpye(const UA_NodeId nodeId)
{
    char *p =NULL;
    p = strstr( (char*)nodeId.identifier.string.data, "CO" );
    if(p) {
        return 0x01;
    }

    p = strstr( (char*)nodeId.identifier.string.data, "DUST" );
    if(p) {
        return 0x02;
    }

    p = strstr(( char*)nodeId.identifier.string.data, "LIAOWEI" );
    if(p) {
        return 0x03;
    }

    p = strstr( (char*)nodeId.identifier.string.data, "FLOW" );
    if(p) {
        return 0x05;
    }

    p = strstr( (char*)nodeId.identifier.string.data, "ENCODER" );
    if(p) {
        return 0x06;
    }

    return 0xff;
}

UA_UInt16 stringNodeIdToAddr(const UA_NodeId nodeId)
{
    return atoi(( char*)nodeId.identifier.string.data);
}
void AddUintNode(UA_Byte *node)  // add the node to the server
{
    UA_UInt16 Integer = 123;
    UA_Variant * variant = UA_Variant_new();
    UA_Variant_setScalarCopy(variant, &Integer, &UA_TYPES[UA_TYPES_UINT16]);

    /* 2) define where the variable shall be added with which browsename */
    UA_NodeId NodeId = UA_NODEID_STRING(1, node);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_QualifiedName browseName = UA_QUALIFIEDNAME(1, node);

    UA_Server_addVariableNode(server, variant,browseName,NodeId,parentNodeId ,  parentReferenceNodeId);
}
void AddFloatNode(UA_Byte *node)
{
    UA_Float Integer = 123;
    UA_Variant * variant = UA_Variant_new();
    UA_Variant_setScalarCopy(variant, &Integer, &UA_TYPES[UA_TYPES_FLOAT]);

    /* 2) define where the variable shall be added with which browsename */
    UA_NodeId NodeId = UA_NODEID_STRING(1, node);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_QualifiedName browseName = UA_QUALIFIEDNAME(1, node);

    UA_Server_addVariableNode(server, variant,browseName,NodeId,parentNodeId ,  parentReferenceNodeId);
}
void AddDoubleNode(UA_Byte *node)
{
    UA_Double Integer = 123;
    UA_Variant * variant = UA_Variant_new();
    UA_Variant_setScalarCopy(variant, &Integer, &UA_TYPES[UA_TYPES_DOUBLE]);

    /* 2) define where the variable shall be added with which browsename */
    UA_NodeId NodeId = UA_NODEID_STRING(1, node);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_QualifiedName browseName = UA_QUALIFIEDNAME(1, node);

    UA_Server_addVariableNode(server, variant,browseName,NodeId,parentNodeId ,  parentReferenceNodeId);
}
void  Change_Server_IntValue(UA_Server *server, UA_NodeId node, UA_UInt16 value)
{

    //UA_Int32 nodeNum = node.identifier.numeric;
    UA_WriteValue *wIntValue = UA_WriteValue_new();
    UA_WriteValue_init(wIntValue);
    wIntValue->attributeId = UA_ATTRIBUTEID_VALUE;
    wIntValue->nodeId = UA_NODEID_STRING_ALLOC(1,node.identifier.string.data);
    wIntValue->value.hasValue = 1;
    wIntValue->value.value.type = &UA_TYPES[UA_TYPES_UINT16];
    wIntValue->value.value.storageType = UA_VARIANT_DATA_NODELETE;
    wIntValue->value.value.data = &value;
    writeValue(server, wIntValue);
    UA_WriteValue_delete(wIntValue);
}
void  Change_Server_FloatValue(UA_Server *server, UA_NodeId node, UA_Float value)
{
    UA_WriteValue *wFloatValue = UA_WriteValue_new();
    wFloatValue->attributeId = UA_ATTRIBUTEID_VALUE;
    wFloatValue->nodeId = UA_NODEID_STRING_ALLOC(1,node.identifier.string.data);
    wFloatValue->value.hasValue = 1;
    wFloatValue->value.value.type = &UA_TYPES[UA_TYPES_FLOAT];
    wFloatValue->value.value.storageType = UA_VARIANT_DATA_NODELETE;
    wFloatValue->value.value.data = &value;
    writeValue(server, wFloatValue);
    UA_WriteValue_delete(wFloatValue);
}
void  Change_Server_DoubleValue(UA_Server *server, UA_NodeId node, UA_Double value)
{
    UA_WriteValue *wDoubleValue = UA_WriteValue_new();
    wDoubleValue->attributeId = UA_ATTRIBUTEID_VALUE;
    wDoubleValue->nodeId = UA_NODEID_STRING_ALLOC(1,node.identifier.string.data);
    wDoubleValue->value.hasValue = 1;
    wDoubleValue->value.value.type = &UA_TYPES[UA_TYPES_DOUBLE];
    wDoubleValue->value.value.storageType = UA_VARIANT_DATA_NODELETE;
    wDoubleValue->value.value.data = &value;
    writeValue(server, wDoubleValue);
    UA_WriteValue_delete(wDoubleValue);
}
void *Opcua_Server(void * arg)
{
    /* init the server */
    server = UA_Server_new(UA_ServerConfig_standard);
    UA_Server_setLogger(server, Logger_Stdout_new());
    UA_Server_addNetworkLayer(server,ServerNetworkLayerTCP_new(UA_ConnectionConfig_standard, OPCUA_SERVER_PORT));

    /* run the server loop */
    //UA_StatusCode retval = UA_Server_run(server, &running);
    UA_StatusCode retval = UA_Server_run(server, WORKER_THREADS, &running);
    UA_Server_delete(server);

    return NULL;
}
