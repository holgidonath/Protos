#ifndef PARSER_ADMIN_H
#define PARSER_ADMIN_H

#include "socket_admin.h"

bool requests(bool * logged, char request[BUFFER_MAX], int request_length, char response[BUFFER_MAX], int * response_length, metrics_t metrics)
void ok(char response[BUFFER_MAX], int * response_length);
void error(char response[BUFFER_MAX], int * response_length);
void login(bool * logged, char request[BUFFER_MAX], int request_length, char response[BUFFER_MAX], int * response_length);
void logout(char response[BUFFER_MAX], int * response_length);
void get_concurrent_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);
void get_total_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);
void get_bytes_transfered(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);

#endif