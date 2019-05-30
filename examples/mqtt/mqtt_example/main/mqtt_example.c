/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "iot_import.h"
#include "iot_export.h"
#include "app_entry.h"
#include "esp_system.h"
#include "esp_log.h"

// #define PRODUCT_KEY             NULL
#define PRODUCT_KEY             "a1p1GvMMHtR"
#define PRODUCT_SECRET          "fol8lHWiqVakyAcM"
#define DEVICE_NAME             "node001"
#define DEVICE_SECRET           "eekBEe6TPtmJEPWsTCeEtQLpUARlbdbQ"
     /*
      * {
{
  "ProductKey": "a1p1GvMMHtR", fol8lHWiqVakyAcM
  "DeviceName": "node001",
  "DeviceSecret": "eekBEe6TPtmJEPWsTCeEtQLpUARlbdbQ"
}
      */
/* These are pre-defined topics */
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA               "/"PRODUCT_KEY"/"DEVICE_NAME"/data"

#define MQTT_MSGLEN             (1024)

#define EXAMPLE_TRACE(fmt, ...)  \
    do { \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__); \
        HAL_Printf(fmt, ##__VA_ARGS__); \
        HAL_Printf("%s", "\r\n"); \
    } while(0)

static int      user_argc;
static char   **user_argv;

static const char *TAG = "MQTT";

void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            EXAMPLE_TRACE("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            EXAMPLE_TRACE("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            EXAMPLE_TRACE("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            EXAMPLE_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            EXAMPLE_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            EXAMPLE_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            EXAMPLE_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            EXAMPLE_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECEIVED:
            EXAMPLE_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        case IOTX_MQTT_EVENT_BUFFER_OVERFLOW:
            EXAMPLE_TRACE("buffer overflow, %s", msg->msg);
            break;

        default:
            EXAMPLE_TRACE("Should NOT arrive here.");
            break;
    }
}

static void _demo_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt     ptopic_info = (iotx_mqtt_topic_info_pt) msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_PUBLISH_RECEIVED:
            /* print topic name and topic message */
            EXAMPLE_TRACE("----");
            EXAMPLE_TRACE("PacketId: %d", ptopic_info->packet_id);
            EXAMPLE_TRACE("Topic: '%.*s' (Length: %d)",
                          ptopic_info->topic_len,
                          ptopic_info->ptopic,
                          ptopic_info->topic_len);
            EXAMPLE_TRACE("Payload: '%.*s' (Length: %d)",
                          ptopic_info->payload_len,
                          ptopic_info->payload,
                          ptopic_info->payload_len);
            EXAMPLE_TRACE("----");
            break;
        default:
            EXAMPLE_TRACE("Should NOT arrive here.");
            break;
    }
}

int mqtt_client_bak(void)
{
    int rc, msg_len, cnt = 0;
    void *pclient;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    iotx_mqtt_topic_info_t topic_msg;
    char msg_pub[128];

    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        return -1;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.read_buf_size = MQTT_MSGLEN;
    mqtt_params.write_buf_size = MQTT_MSGLEN;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        return -1;
    }

    /* Initialize topic information */
    memset(&topic_msg, 0x0, sizeof(iotx_mqtt_topic_info_t));
    strcpy(msg_pub, "update: hello! start!");

    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);

    rc = IOT_MQTT_Publish(pclient, TOPIC_UPDATE, &topic_msg);
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        EXAMPLE_TRACE("error occur when publish");
        return -1;
    }

    EXAMPLE_TRACE("\n publish message: \n topic: %s\n payload: \%s\n rc = %d", TOPIC_UPDATE, topic_msg.payload, rc);

    /* Subscribe the specific topic */
    rc = IOT_MQTT_Subscribe(pclient, TOPIC_DATA, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        EXAMPLE_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        return -1;
    }

    IOT_MQTT_Yield(pclient, 200);

    HAL_SleepMs(2000);

    /* Initialize topic information */
    memset(msg_pub, 0x0, 128);
    strcpy(msg_pub, "data: hello! start!");
    memset(&topic_msg, 0x0, sizeof(iotx_mqtt_topic_info_t));
    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);

    rc = IOT_MQTT_Publish(pclient, TOPIC_DATA, &topic_msg);
    EXAMPLE_TRACE("\n publish message: \n topic: %s\n payload: \%s\n rc = %d", TOPIC_DATA, topic_msg.payload, rc);

    IOT_MQTT_Yield(pclient, 200);

    do {
        /* Generate topic message */
        cnt++;
        msg_len = snprintf(msg_pub, sizeof(msg_pub), "{\"attr_name\":\"temperature\",\"attr_value\":\"%d\"}", cnt);
        if (msg_len < 0) {
            EXAMPLE_TRACE("Error occur! Exit program");
            return -1;
        }

        topic_msg.payload = (void *)msg_pub;
        topic_msg.payload_len = msg_len;

        rc = IOT_MQTT_Publish(pclient, TOPIC_DATA, &topic_msg);
        if (rc < 0) {
            EXAMPLE_TRACE("error occur when publish");
        }
        EXAMPLE_TRACE("packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);

        /* handle the MQTT packet received from TCP or SSL connection */
        IOT_MQTT_Yield(pclient, 1000);

        /* infinite loop if running with 'loop' argument */
        if (user_argc >= 2 && !strcmp("loop", user_argv[1])) {
            // HAL_SleepMs(2000);
            // cnt = 0;
        }
        ESP_LOGI(TAG, "min:%u heap:%u", esp_get_minimum_free_heap_size(), esp_get_free_heap_size());
    } while (cnt);

    IOT_MQTT_Yield(pclient, 200);

    IOT_MQTT_Unsubscribe(pclient, TOPIC_DATA);

    IOT_MQTT_Yield(pclient, 200);

    IOT_MQTT_Destroy(&pclient);

    return 0;
}




typedef enum
{
TEMPERATURE_NORMAL =0,
TEMPERATURE_HIGH_ALARM ,
TEMPERATURE_HIGH_ALARMSTOP
}Temperature_StatusTypeDef;

typedef struct{
float Tem_value;
uint32_t Hum_value;
Temperature_StatusTypeDef Tem_status;
uint8_t Tem_threshold;
}DeviceStatusTypeDef;

static DeviceStatusTypeDef  device_s;
static int prepare_Propertie_payload(char * payloadBuffer, int payloadSize, char * deviceID)
{
    char * buff = payloadBuffer;
  int buffSize = payloadSize;
  int snprintfreturn = 0;

if(device_s.Hum_value >=100)
	device_s.Hum_value=0;
else
  device_s.Hum_value=device_s.Hum_value+1;
if(device_s.Tem_value>=50)
	device_s.Tem_value=-20;
else
  device_s.Tem_value= device_s.Tem_value+1;

  device_s.Tem_threshold =30;


 snprintfreturn= snprintf( buff, buffSize,"{ \"method\":\"thing.event.property.post\", \"id\":\"123\",\"params\":{\"CurrentHumidity\":%2d,\"CurrentTemperature\":%.2f,\"TempThreshold\":%2d},\"version\":\"1.0.0\"}",device_s.Hum_value,device_s.Tem_value,device_s.Tem_threshold);



   if (snprintfreturn >= 0 && snprintfreturn < payloadSize)
  {
      return 0;
  }
  else if(snprintfreturn >=payloadSize)
  {
    //  msg_error("Data Pack truncated\n");
      return 0;
  }
  else
  {
   //   msg_error("Data Pack Error\n");
      return -1;
  }
}
int mqtt_client(void)
{
    int rc, msg_len, cnt = 0;
    void *pclient;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    iotx_mqtt_topic_info_t topic_msg;
    char msg_pub[256];

//add
    char Temp_Topic[256]={0};


    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        return -1;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.read_buf_size = MQTT_MSGLEN;
    mqtt_params.write_buf_size = MQTT_MSGLEN;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        return -1;
    }
    /* Subscribe the specific topic */
    memset(Temp_Topic, 0, sizeof(Temp_Topic));
    snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/service/property/set",PRODUCT_KEY,DEVICE_NAME);
    rc = IOT_MQTT_Subscribe(pclient, Temp_Topic, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);

    memset(Temp_Topic, 0, sizeof(Temp_Topic));
    snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/service/ClearAlarm",PRODUCT_KEY,DEVICE_NAME);
    rc = IOT_MQTT_Subscribe(pclient, Temp_Topic, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);

    if (rc < 0) {
      IOT_MQTT_Destroy(&pclient);
      EXAMPLE_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);

      EXAMPLE_TRACE("\nmqtt example loop end!");
        //vTaskDelete(ALIIOTKITHandle);
     return -1;
    }



    /* Initialize topic information */
    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;

    do {

      if(device_s.Tem_threshold > device_s.Tem_value)
      {

           memset(Temp_Topic, 0, sizeof(Temp_Topic));
          memset(msg_pub, 0, sizeof(msg_pub));
          snprintf( msg_pub, sizeof(msg_pub),"{ \"method\":\"thing.event.TempAlarm.post\", \"id\":\"123\",\"params\":{}}");
          topic_msg.payload = (void *)msg_pub;
          topic_msg.payload_len = strlen(msg_pub);
           snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/event/TempAlarm/post",PRODUCT_KEY,DEVICE_NAME);
          rc = IOT_MQTT_Publish(pclient, Temp_Topic, &topic_msg);
          //osSemaphoreRelease(PublishAlarmLedSemaphoreHandle);
         if (rc < 0)
         {
        	 EXAMPLE_TRACE("error occur when publish alarm value");
            rc = -1;
            break;
         }
         EXAMPLE_TRACE("\n*****************packet-id=%u, publish topic alarm Value msg=%s\r\n",(uint32_t)rc, msg_pub);
      }
      prepare_Propertie_payload(msg_pub, sizeof(msg_pub),NULL);
      topic_msg.payload = (void *)msg_pub;
      topic_msg.payload_len = strlen(msg_pub);
      snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/event/property/post",PRODUCT_KEY,DEVICE_NAME);

      rc = IOT_MQTT_Publish(pclient, Temp_Topic, &topic_msg);
      if (rc < 0) {
    	  EXAMPLE_TRACE("error occur when publish sensor data");
        rc = -1;
        break;
      }
      EXAMPLE_TRACE("\n packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);
      //osSemaphoreRelease(PublishPropertiesLedSemaphoreHandle);
      //osDelay(1000);
      /* handle the MQTT packet received from TCP or SSL connection */
      IOT_MQTT_Yield(pclient, 10000);
      //HAL_SleepMs(2000);
      //  osDelay(4000);
    } while(1); //(cnt < MAX_MQTT_PUBLISH_COUNT);



    return 0;
}

#if 0
void mqtt_client_example(void)
{
   int rc;
  iotx_conn_info_pt pconn_info;
  iotx_mqtt_param_t mqtt_params;
  iotx_mqtt_topic_info_t topic_msg;
  char msg_pub[512]={0};
  bool skip_reconf = false;
  const char * tempsecret,* device_name_temp,* product_key_temp;
  char Temp_Topic[256]={0};

  skip_reconf = (checkDeviceConfig()==0);
  if( skip_reconf ==  true)
  {
     printf("Push the User button (Blue) within the next 5 seconds if you want to update "
           "the device security parameters or credentials.\n\n");
    skip_reconf = (Button_WaitForPush(5000) == BP_NOT_PUSHED);
  }
  if( skip_reconf ==  false)
  {
     updateDeviceConfig();
  }

  if((getDeviceSecret(&tempsecret)==HAL_ERROR)
   ||(getProductKey(&product_key_temp)==HAL_ERROR)
    ||(getDeviceName(&device_name_temp)==HAL_ERROR))
  {
    msg_info("getDeviceName failed\n");
    vTaskDelete(ALIIOTKITHandle);
    return ;
  }
//  msg_info("\n**************************************************************\n");
//  msg_info("DEVICE_SECRET:      %s\n",tempsecret);
//  msg_info("DEVICE_NAME:        %s\n",device_name_temp);
//  msg_info("PRODUCT_NAME:       %s\n",product_key_temp);
//  msg_info("\n**************************************************************\n");



  /* Device AUTH */
  if (0 != IOT_SetupConnInfo(product_key_temp, device_name_temp, tempsecret, (void **)&pconn_info)) {
    msg_info("\nAUTH request failed!");
     msg_info("\nmqtt example loop end!");
     vTaskDelete(ALIIOTKITHandle);
   return ;
  }

  /* Initialize MQTT parameter */
  memset(&mqtt_params, 0x0, sizeof(mqtt_params));

  mqtt_params.port = pconn_info->port;
  mqtt_params.host = pconn_info->host_name;
  mqtt_params.client_id = pconn_info->client_id;
  mqtt_params.username = pconn_info->username;
  mqtt_params.password = pconn_info->password;
  mqtt_params.pub_key = pconn_info->pub_key;

  mqtt_params.request_timeout_ms = 5000;
  mqtt_params.clean_session = 0;
  mqtt_params.keepalive_interval_ms = 60000; // change from 12s in 04-10
 // mqtt_params.pread_buf = msg_readbuf;
  mqtt_params.read_buf_size = MSG_LEN_MAX;
 // mqtt_params.pwrite_buf = msg_buf;
  mqtt_params.write_buf_size = MSG_LEN_MAX;

  mqtt_params.handle_event.h_fp = event_handle_mqtt;
  mqtt_params.handle_event.pcontext = NULL;

  /* Construct a MQTT client with specify parameter */
  pclient = IOT_MQTT_Construct(&mqtt_params);
  if (NULL == pclient) {
    msg_info("MQTT construct failed");
    msg_info("\nmqtt example loop end!");
    vTaskDelete(ALIIOTKITHandle);
   return ;
  }
  /* Subscribe the specific topic */
  memset(Temp_Topic, 0, sizeof(Temp_Topic));
  snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/service/property/set",product_key_temp,device_name_temp);
  rc = IOT_MQTT_Subscribe(pclient, Temp_Topic, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);

  memset(Temp_Topic, 0, sizeof(Temp_Topic));
  snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/service/ClearAlarm",product_key_temp,device_name_temp);
  rc = IOT_MQTT_Subscribe(pclient, Temp_Topic, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);

  if (rc < 0) {
    IOT_MQTT_Destroy(&pclient);
    msg_info("IOT_MQTT_Subscribe() failed, rc = %d", rc);

    msg_info("\nmqtt example loop end!");
      vTaskDelete(ALIIOTKITHandle);
   return ;
  }

  /* Initialize topic information */
  topic_msg.qos = IOTX_MQTT_QOS1;
  topic_msg.retain = 0;
  topic_msg.dup = 0;

  do {

    if(osSemaphoreWait(PublishAlarmValueSemaphoreHandle,1)==0)
    {

         memset(Temp_Topic, 0, sizeof(Temp_Topic));
        memset(msg_pub, 0, sizeof(msg_pub));
        snprintf( msg_pub, sizeof(msg_pub),"{ \"method\":\"thing.event.TempAlarm.post\", \"id\":\"123\",\"params\":{}}");
        topic_msg.payload = (void *)msg_pub;
        topic_msg.payload_len = strlen(msg_pub);
         snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/event/TempAlarm/post",product_key_temp,device_name_temp);
        rc = IOT_MQTT_Publish(pclient, Temp_Topic, &topic_msg);
        osSemaphoreRelease(PublishAlarmLedSemaphoreHandle);
       if (rc < 0)
       {
          msg_info("error occur when publish alarm value");
          rc = -1;
          break;
       }
           sys_info("\n*****************packet-id=%u, publish topic alarm Value msg=%s\r\n",(uint32_t)rc, msg_pub);
    }
    prepare_Propertie_payload(msg_pub, sizeof(msg_pub),NULL);
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);
    snprintf( Temp_Topic, sizeof(Temp_Topic),"/sys/%s/%s/thing/event/property/post",product_key_temp,device_name_temp);

    rc = IOT_MQTT_Publish(pclient, Temp_Topic, &topic_msg);
    if (rc < 0) {
      msg_info("error occur when publish sensor data");
      rc = -1;
      break;
    }
    msg_info("\n packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);
    osSemaphoreRelease(PublishPropertiesLedSemaphoreHandle);
    //osDelay(1000);
    /* handle the MQTT packet received from TCP or SSL connection */
    IOT_MQTT_Yield(pclient, 1000);
      osDelay(4000);
  } while(1); //(cnt < MAX_MQTT_PUBLISH_COUNT);
}
#endif

int linkkit_main(void *paras)
{
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    user_argc = 0;
    user_argv = NULL;

    if (paras != NULL) {
        app_main_paras_t *p = (app_main_paras_t *)paras;
        user_argc = p->argc;
        user_argv = p->argv;
    }

    HAL_SetProductKey(PRODUCT_KEY);
    HAL_SetDeviceName(DEVICE_NAME);
    HAL_SetDeviceSecret(DEVICE_SECRET);
    HAL_SetProductSecret(PRODUCT_SECRET);
    /* Choose Login Server */
    int domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);

    /* Choose Login  Method */
    int dynamic_register = 0;
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    mqtt_client();
    //mqtt_client_example();
    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_SetLogLevel(IOT_LOG_NONE);

    EXAMPLE_TRACE("out of sample!");

    return 0;
}
