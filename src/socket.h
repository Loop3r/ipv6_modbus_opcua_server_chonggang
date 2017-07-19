//
// Created by champer on 27/04/17.
//

#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <modbus/modbus.h>
#include "open62541.h"
#include "get_config.h"

#define IOT_DAEMON_PORT     5222
#define MODBUS_SERVER_PORT   502
#define IOT_DAEMON_ADDR    "127.0.0.1"
//#define MODBUS_SERVER_ADDR  "192.168.1.170"
#define OPCUA_SERVER_PORT 2226
#define WORKER_THREADS 2 /* if multithreading is enabled */

#define CO         0x01
#define DUST       0x02
#define LIAOWEI    0x03
#define DIANBIAO   0x04
#define FLOW       0x05
#define ENCODER    0x06
#define DISPLACE   0x07  //位移
#define HUMITURE   0x08  //温湿度
#define WIND       0x09  //风速
#define FLOW_METER  0x0A  //流量仪表
#define SO2        0x0B  //二氧化硫
#define TEMP       0x0C  //温变

#define CO_PACKET_LEN         9
#define DUST_PACKET_LEN       9
#define LIAOWEI_PACKET_LEN    9
#define DIANBIAO_PACKET_LEN   27
#define FLOW_PACKET_LEN       9
#define ENCODER_PACKET_LEN    11
#define DISPLACE_PACKET_LEN   9
#define HUMITURE_PACKET_LEN   11
#define WIND_PACKET_LEN       9
#define ELEC_FLOW_PACKET_LEN  27
#define SO2_PACKET_LEN        9
#define TEMP_PACKET_LEN       9


#define NB_CONNECTION        5
#define IPV6_RESP_LEN        100

//#define REGISTER_WRITE_HEAD   ((buf[4]-1)*20)
#define REGISTER_WRITE_HEAD   ((buf[4]-1)*4)

void close_sigint(int dummy);
void swap(uint8_t *a, uint8_t *b);
void DIANBIAO_Hex2Float(uint8_t *srcbuf, float *dstbuf);
void *Modbus_Server(void *arg);
void *IPv6_Client(void *arg);
int Parse_IPv6_Resp(uint8_t *buf, int len);
uint8_t Get_Data_Type(uint8_t *data);

void *Opcua_Server(void * arg);
void *Opcua_Server_Write(void * arg);
void  Change_Server_IntValue(UA_Server *server, UA_NodeId node,UA_UInt16 value);
void  Change_Server_FloatValue(UA_Server *server, UA_NodeId node,UA_Float value);
void  Opcua_Server_Parse(UA_Byte *opcuabuf);
void  Opcua_Server_AddNode(UA_Byte *nodebuf);
static UA_StatusCode readUIntDataSource(void *handle, UA_Boolean sourceTimeStamp,
                                        const UA_NumericRange *range, UA_DataValue *value) ;
static UA_StatusCode readFloatDataSource(void *handle, UA_Boolean sourceTimeStamp,
                                         const UA_NumericRange *range, UA_DataValue *value) ;
UA_UInt16 nodeidFindUintData(UA_UInt16 addr) ;
UA_Float nodeidFindFloatData(const char *nodeId);
#endif //MODBUS_SERVER_SOCKET_H
