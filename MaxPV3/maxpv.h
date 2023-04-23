#ifndef MAXPV_H
#define MAXPV_H

#define DEBUG_DIMMER
//#define DEBUG_SERIAL
//#define DEBUG_HTTPC
#define MAXPV_FTP_SERVER


// ***********************************************************************************
// ******************                   LIBRAIRIES EXT                 ***************
// ***********************************************************************************
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TickerScheduler.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>  // multicast DNS
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <DNSServer.h>
//#include <NTPClient.h>
#include <NTP.h>
//#define MAXPV_FTP_SERVER
#ifdef MAXPV_FTP_SERVER
#include <SimpleFTPServer.h>
#endif

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <AsyncPing.h>

// ***********************************************************************************
// ******************            OPTIONS DE COMPILATION                ***************
// ***********************************************************************************

// Pas de debug série pour MQTT
#define _ASYNC_MQTT_LOGLEVEL_               0

// ***********************************************************************************
// ******************        FIN DES OPTIONS DE COMPILATION            ***************
// ***********************************************************************************

// ***********************************************************************************
// ****************************   Définitions générales   ****************************
// ***********************************************************************************

// Heure solaire
#define GMT_OFFSET 0

// SSID pour le Config Portal
#define SSID_CP "MaxPV"

// nom mDNS pour accès http://xxx.local
#define M_DNS "maxpv"


// Login et password pour le service FTP
#define LOGIN_FTP "maxpv"
#define PWD_FTP "maxpv"

#define PING_WIFI_TIMEOUT    7000       // Délai en ms où on considère un problème de 
// connexion wifi qui active le watchdog wifi

// Communications
#define TELNET_PORT 23     // Port Telnet
#define HTTP_PORT 80 // Web server port
#define SERIAL_BAUD 500000 // Vitesse de la liaison port série pour la connexion avec l'arduino
#define SERIALTIMEOUT 100  // Timeout pour les interrogations sur liaison série en ms
#define SERIAL_BUFFER 256  // Taille du buffer RX pour la connexion avec l'arduino (256 max)

// Historisation des index
#define HISTORY_INTERVAL 30  // Périodicité en minutes de l'enregistrement des index d'énergie pour l'historisation
// Valeurs permises : 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60, 120, 180, 240, 480
#define HISTORY_RECORD  193  // Nombre de points dans l'historique
// Attention à la taille totale de l'historique en mémoire
// Et la capacité d'export en CSV


#define MAXPV_VERSION "3.356"
#define MAXPV_VERSION_FULL "MaxPV! 3.356"


#define OFF 0
#define ON 1
#define STOP 0
#define FORCE 1
#define AUTOM 9

// ***********************************************************************************
// ****************************       ECOPV               ****************************
// ***********************************************************************************

// définition de l'ordre des paramètres de configuration de EcoPV tels que transmis
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
#define NB_PARAM 22 // Nombre de paramètres transmis par EcoPV (22 = 21 + VERSION)
#define ECOPV_VERSION 0
#define V_CALIB 1
#define P_CALIB 2
#define PHASE_CALIB 3
#define P_OFFSET 4
#define P_RESISTANCE 5
#define P_MARGIN 6
#define GAIN_P 7
#define GAIN_I 8
#define E_RESERVE 9
#define P_DIV2_ACTIVE 10
#define P_DIV2_IDLE 11
#define T_DIV2_ON 12
#define T_DIV2_OFF 13
#define T_DIV2_TC 14
#define A_DIV2_EXT 15
#define CNT_CALIB 16
#define P_INSTALLPV 17
#define S_SSR 18
#define S_Relay 19
#define T_CALIB 20
#define T_MAX 21

// définition de l'ordre des informations statistiques transmises par EcoPV
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
// ATTENTION : dans le reste du programme les 4 index de début de journée sont ajoutés à la suite
// pour les informations disponibles par l'API
// ils doivent toujours être situés en toute fin de tableu
#define NB_STATS 24     // Nombre d'informations statistiques transmis par EcoPV (24 = 23 + VERSION)
#define NB_STATS_SUPP 4 // Nombre d'informations statistiques supplémentaires
//#define ECOPV_VERSION 0
#define V_RMS 1
#define I_RMS 2
#define P_ACT 3
#define P_APP 4
#define P_ROUTED 5
#define P_IMP 6
#define P_EXP 7
#define COS_PHI 8
#define INDEX_ROUTED 9
#define INDEX_IMPORT 10
#define INDEX_EXPORT 11
#define INDEX_IMPULSION 12
#define P_IMPULSION 13
#define TRIAC_MODE 14
#define RELAY_MODE 15
#define DELAY_MIN 16
#define DELAY_AVG 17
#define DELAY_MAX 18
#define BIAS_OFFSET 19
#define STATUS_BYTE 20
#define ONTIME 21
#define SAMPLES 22
#define TEMP 23
#define INDEX_ROUTED_J 24
#define INDEX_IMPORT_J 25
#define INDEX_EXPORT_J 26
#define INDEX_IMPULSION_J 27




#endif
