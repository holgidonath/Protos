#include <stdio.h>
#include <string.h>

#include "include/parser_admin.h"

static char * login = "PASS123";

bool requests(bool * logged, char request[BUFFER_MAX], int request_length, char response[BUFFER_MAX], int * response_length, metrics_t metrics) {
	bool ret = false;
	switch(* request) {
		case LOGIN:
			login(logged, request, request_length, response, response_length);
			break;
		case LOGOUT:
			logout(response, response_length);
			ret = true;
		case GET_CURRENT_CONNECTIONS:
			get_current_connections(logged, response, response_length, metrics);
			break;
		case GET_TOTAL_CONNECTIONS:
			get_total_connections(logged, response, response_length, metrics);
			break;
		case GET_TOTAL_BYTES:
			get_total_bytes(logged, response, response_length, metrics);
		default:
			error(response, response_length);
			break;
	}
	return ret;
}

void ok(char response[BUFFER_MAX], int * response_length) {
	response[0] = OK;
	* response_l = 1;
}

void error(char response[BUFFER_MAX], int * response_length) {
	response[0] = ERR;
	* response_l = 1;
}

void login(bool * logged, char request[BUFFER_MAX], int request_length, char response[BUFFER_MAX], int * response_length) {
	if(!(* logged) && request_length == 8 && strncmp(request + 1, login, 7) == 0) {
		ok(response, response_length);
		* logged = true;
	} else {
		error(response, response_length);
	}
}

void logout(char response[BUFFER_MAX], int * response_length) {
	ok(response, response_length);
}

void get_current_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics) {
	if(* logged) {
		ok(response, response_length);
		sprintf(response + 1, "%d", metrics->current_connections);
		* response_length += strlen(response +1) + 1;
	} else {
		error(response, response_length);
	}
}

void get_total_connections(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics) {
	if(* logged) {
		ok(response, response_length);
		sprintf(response + 1, "%d", metrics->total_connections);
		* response_length += strlen(response +1) + 1;
	} else {
		error(response, response_length);
	}
}

void get_total_bytes(bool * logged, char response[BUFFER_MAX], int * response_length, metrics_t metrics) {
	if(* logged) {
		ok(response, response_length);
		sprintf(response + 1, "%d", metrics->total_bytes);
		* response_length += strlen(response +1) + 1;
	} else {
		error(response, response_length);
	}
}