/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdlib.h>
#include <pthread.h>
#include "socket.h"

pthread_t modbus_server_thread;	
pthread_t ipv6_client_thread;	
pthread_t opcua_server_thread;
pthread_t opcua_data_thread;

void test_thread(pthread_t * ptd)
{
    int kill_rc = pthread_kill(*ptd, 0);
    if(kill_rc == ESRCH){
    printf("thread already quit\n");
    exit(EXIT_FAILURE);
    }
    else if(kill_rc == EINVAL) {
    printf("signal is invalid\n");
    exit(EXIT_FAILURE);
    }
    else{
    //printf("thread is running\n");
    }

}

int main(int argc, char*argv[])
{
	pthread_create(&ipv6_client_thread, NULL, IPv6_Client, NULL);
	pthread_create(&modbus_server_thread, NULL, Modbus_Server, NULL);
	pthread_create(&opcua_server_thread, NULL, Opcua_Server, NULL);
	//pthread_create(&opcua_data_thread, NULL, Opcua_Server_Write, NULL);
	while(1) {
        test_thread(&ipv6_client_thread);
        test_thread(&modbus_server_thread);
        test_thread(&opcua_server_thread);
        sleep(5);
    }
}


