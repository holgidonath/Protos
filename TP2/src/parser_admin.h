#ifndef PARSER_ADMIN_H
#define PARSER_ADMIN_H

#include "socket_admin.h"

bool requests(bool * logged, char request[BUFFER_MAX], int request_length, char response[BUFFER_MAX], int * response_length, metrics_t metrics)
void ok(char response[BUFFER_MAX], int * response_l);
void error(char response[BUFFER_MAX], int * response_l);
void login(bool * logged, char request[BUFFER_MAX], int request_l, char response[BUFFER_MAX], int * response_l);
void logout(char response[BUFFER_MAX], int * response_l);
void get_current_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);
void get_total_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);
void get_total_bytes(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics);

#endif