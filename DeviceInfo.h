#pragma once

// Update these with values suitable for your network.
const char* ssid = "abcd_wifi";
const char* password = "wifitang2";
const char* mqtt_server = "soldier.cloudmqtt.com";
const int mqtt_port = 15258;

String device_id = "Device001";
char mqtt_clientID[] = "Device001";
char mqtt_topic_pub[] = "reponse/Device001";
char mqtt_topic_config[] = "config/Device001";
char mqtt_topic_sub[] = "cmd/Device001";
char mqtt_user[] = "jhqlhwjr";
char mqtt_pwd[] = "26Yvz_31t907";