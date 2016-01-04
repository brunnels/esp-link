//
// MQTT Commands coming in from the attached microcontroller over the serial port
//

#include <esp8266.h>
#include "mqtt.h"
#include "mqtt_client.h"
#include "mqtt_cmd.h"

#ifdef MQTTCMD_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

#define MQTT_CB 0xf00df00d

// callbacks to the attached uC
uint32_t connectedCb = 0, disconnectCb = 0, publishedCb = 0, dataCb = 0;

void ICACHE_FLASH_ATTR cmdMqttConnectedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Connected  connectedCb=%p, disconnectedCb=%p, publishedCb=%p, dataCb=%p\n",
       (void*)cb->connectedCb,
       (void*)cb->disconnectedCb,
       (void*)cb->publishedCb,
       (void*)cb->dataCb);
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->connectedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR cmdMqttDisconnectedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Disconnected\n");
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->disconnectedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR cmdMqttPublishedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Published\n");
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->publishedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR cmdMqttDataCb(uint32_t* args, const char* topic, uint32_t topic_len, const char* data, uint32_t data_len) {
  uint16_t crc = 0;
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;

  crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->dataCb, 0, 2);
  crc = CMD_ResponseBody(crc, (uint8_t*)topic, topic_len);
  crc = CMD_ResponseBody(crc, (uint8_t*)data, data_len);
  CMD_ResponseEnd(crc);
}

uint32_t ICACHE_FLASH_ATTR MQTTCMD_Lwt(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 5)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);

  // free old topic & message
  if (mqttClient.connect_info.will_topic)
    os_free(mqttClient.connect_info.will_topic);
  if (mqttClient.connect_info.will_message)
    os_free(mqttClient.connect_info.will_message);

  uint16_t len;

  // get topic
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  mqttClient.connect_info.will_topic = (char*)os_zalloc(len + 1);
  CMD_PopArg(&req, mqttClient.connect_info.will_topic, len);
  mqttClient.connect_info.will_topic[len] = 0;

  // get message
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  mqttClient.connect_info.will_message = (char*)os_zalloc(len + 1);
  CMD_PopArg(&req, mqttClient.connect_info.will_message, len);
  mqttClient.connect_info.will_message[len] = 0;

  // get qos
  CMD_PopArg(&req, (uint8_t*)&mqttClient.connect_info.will_qos, 4);

  // get retain
  CMD_PopArg(&req, (uint8_t*)&mqttClient.connect_info.will_retain, 4);

  DBG("MQTT: MQTTCMD_Lwt topic=%s, message=%s, qos=%d, retain=%d\n",
    mqttClient.connect_info.will_topic,
    mqttClient.connect_info.will_message,
    mqttClient.connect_info.will_qos,
    mqttClient.connect_info.will_retain);

  // trigger a reconnect to set the LWT
  MQTT_Reconnect(&mqttClient);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR MQTTCMD_Publish(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 6)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  uint16_t len;

  // get topic
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  uint8_t *topic = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, topic, len);
  topic[len] = 0;

  // get data
  len = CMD_ArgLen(&req);
  uint8_t *data = (uint8_t*)os_zalloc(len+1);
  if (!data) { // safety check
    os_free(topic);
    return 0;
  }
  CMD_PopArg(&req, data, len);
  data[len] = 0;

  uint32_t qos, retain, data_len;

  // get data length
  // this isn't used but we have to pull it off the stack
  CMD_PopArg(&req, (uint8_t*)&data_len, 4);

  // get qos
  CMD_PopArg(&req, (uint8_t*)&qos, 4);

  // get retain
  CMD_PopArg(&req, (uint8_t*)&retain, 4);

  DBG("MQTT: MQTTCMD_Publish topic=%s, data_len=%d, qos=%ld, retain=%ld\n",
    topic,
    os_strlen((char*)data),
    qos,
    retain);

  MQTT_Publish(&mqttClient, (char*)topic, (char*)data, (uint8_t)qos, (uint8_t)retain);
  os_free(topic);
  os_free(data);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR MQTTCMD_Subscribe(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 3)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  uint16_t len;

  // get topic
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  uint8_t* topic = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, topic, len);
  topic[len] = 0;

  // get qos
  uint32_t qos = 0;
  CMD_PopArg(&req, (uint8_t*)&qos, 4);

  DBG("MQTT: MQTTCMD_Subscribe topic=%s, qos=%ld\n", topic, qos);

  MQTT_Subscribe(&mqttClient, (char*)topic, (uint8_t)qos);
  os_free(topic);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR MQTTCMD_Init(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 4)
    return 0;

  // create callback
  MqttCmdCb* callback = (MqttCmdCb*)os_zalloc(sizeof(MqttCmdCb));
  uint32_t cb_data;

  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->connectedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->disconnectedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->publishedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->dataCb = cb_data;

  mqttClient.user_data = callback;
  mqttClient.cmdConnectedCb = cmdMqttConnectedCb;
  mqttClient.cmdDisconnectedCb = cmdMqttDisconnectedCb;
  mqttClient.cmdPublishedCb = cmdMqttPublishedCb;
  mqttClient.cmdDataCb = cmdMqttDataCb;

  return MQTT_CB; //(uint32_t)&mqttClient;
}


