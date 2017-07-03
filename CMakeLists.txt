cmake_minimum_required(VERSION 3.5)
project(ipv6_modbus_opcua_server_chonggang)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/ipv6_modbus_opcua_server.c src/socket.c src/socket.h src/modbus_data.h
        src/open62541.c src/open62541.h src/get_config.c src/get_config.h)

add_executable(ipv6_modbus_opcua_server_chonggang ${SOURCE_FILES})

target_link_libraries(ipv6_modbus_opcua_server_chonggang modbus pthread)