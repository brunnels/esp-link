#ifndef _MQTT_CMD_H_
#define _MQTT_CMD_H_

#include "cmd.h"

typedef struct {
  uint32_t connectedCb;
  uint32_t disconnectedCb;
  uint32_t publishedCb;
  uint32_t dataCb;
} MqttCmdCb;

uint32_t MQTTCMD_Init(CmdPacket *cmd);
uint32_t MQTTCMD_Publish(CmdPacket *cmd);
uint32_t MQTTCMD_Subscribe(CmdPacket *cmd);
uint32_t MQTTCMD_Lwt(CmdPacket *cmd);

#endif // _MQTT_CMD_H_
