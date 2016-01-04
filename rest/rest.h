#ifndef _REST_H_
#define _REST_H_

#include "c_types.h"
#include "ip_addr.h"
#include "cmd.h"

typedef enum {
  HEADER_GENERIC = 0,
  HEADER_CONTENT_TYPE,
  HEADER_USER_AGENT
} HEADER_TYPE;

typedef struct {
  char           *host;
  uint32_t       port;
  uint32_t       security;
  ip_addr_t      ip;
  struct espconn *pCon;
  char           *header;
  char           *data;
  uint16_t       data_len;
  uint16_t       data_sent;
  char           *content_type;
  char           *user_agent;
  uint32_t       resp_cb;
} RestClient;

uint32_t REST_Setup(CmdPacket *cmd);
uint32_t REST_Request(CmdPacket *cmd);
uint32_t REST_SetHeader(CmdPacket *cmd);

#endif // _REST_H_
