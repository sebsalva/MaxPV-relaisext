/*
  MaxPV_ESP.ino - ESP8266 program that provides a web interface and a API for EcoPV 3+
  Copyright (C) 2022 - Bernard Legrand.

  https://github.com/Jetblack31/

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*************************************************************************************
**                                                                                  **
**        Ce programme fonctionne sur ESP8266 de tye Wemos avec 4 Mo de mémoire     **
**        La compilation s'effectue avec l'IDE Arduino                              **
**        Site Arduino : https://www.arduino.cc                                     **
**                                                                                  **
**************************************************************************************/


// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***
// ***********************************************************************************
// ******************                  LIBRAIRIES                      ***************
// ***********************************************************************************
#include <AsyncElegantOTA.h>   //pb edition de liens A VOIR
#include "maxpv.h"
#include "httprouteur.h"
#include "mqtt.h"

// ***********************************************************************************
// ******************* Variables globales de configuration MaxPV! ********************
// ***********************************************************************************

// Configuration IP statique mode STA
char static_ip[16] = "192.168.1.250";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";
char static_dns1[16] = "192.168.1.1";
char static_dns2[16] = "8.8.8.8";

// Port HTTP
uint16_t httpPort = 80;

// Définition des paramètres du mode BOOST
byte boostRatio           = 100;       // En % de la puissance max
int boostDuration         = 120;       // En minutes
uint8_t boostTimerHour    = 17;        // Heure Timer Boost
uint8_t boostTimerMinute  = 0;         // Minute Timer Boost
uint8_t boostTimerActive  = OFF;       // BOOST timer actif mode 1 (=ON), mode 2 (=2) ou non (=OFF)


// Configuration de MQTT
extern String mqttIP;                                      // IP du serveur MQTT
extern String mqttUser;                                    // Utilisateur du serveur MQTT - Optionnel : si vide, pas d'authentification
extern String mqttPass;                                    // Mot de passe du serveur MQTT
extern uint16_t mqttPort;                                  // Port du serveur MQTT
extern short mqttPeriod;                                   // Période de transmission en secondes
extern short mqttActive;
extern short mqttDet;                                      // Envoi données détaillées MQTT actif (= ON) ou non (= OFF)

// Variables relais_ext
uint8_t  a_div2_ext 	      = OFF;
char     a_div2_urloff[255] = "http://example.com";
char     a_div2_urlon[255]  = "http://example.com";
uint8_t etatrelais    	    = 0;

// Variables Configuration Dimmer HTTP
extern uint8_t           dimmer_m;
extern char              dimmer_ip[16] ;            // nom ou IP du dimmer
//extern unsigned short    dimmer_sumpourcent ;       // somme des porcentages de fonctionnement max sur tous les dimmers
extern short    dimmer_sumpow ;            // somme des puissances sur tous les dimmers
extern uint8_t           dimmer_period ;            //Période activité en secondes

// Gestion Temperature
//short     temp 	     = -1;
//uint8_t hysteresis = 1;
uint8_t tempactive = OFF;

//puissance PV recue par MQTT
int powerpv = -1;

// ***********************************************************************************
// *************** Fin des variables globales de configuration MaxPV! ****************
// ***********************************************************************************

// Stockage des informations en provenance de EcoPV - Arduino Nano
String ecoPVConfig[NB_PARAM];
String ecoPVStats[NB_STATS + NB_STATS_SUPP];
String ecoPVConfigAll;
String ecoPVStatsAll;

// Définition du nombre de tâches de Ticker
TickerScheduler ts(12);
extern Ticker mqttReconnectTimer;

// Compteur général à la seconde
unsigned int generalCounterSecond = 0;
// Flag indiquant la nécessité de sauvegarder la configuration de MaxPV!
bool shouldSaveConfig = false;
// Flag indiquant la nécessité de lire les paramètres de routage EcoPV
bool shouldReadParams = false;
unsigned long refTimeContactEcoPV = millis();
bool contactEcoPV = false;

//flag réseau
bool shouldCheckWifi = false;
bool shouldCheckMQTT = false;

// Variables pour l'historisation
struct historicData
{ // Structure pour le stockage des données historiques
  unsigned long time; // epoch Time
  float eRouted;      // index de l'énergie routée en kWh stocké en float
  float eImport;      // index de l'énergie importée en kWh stocké en float
  float eExport;      // index de l'énergie exportée en kWh stocké en float
  float eImpulsion;   // index de l'énergie produite (compteur impulsion) en kWh stocké en float
};
historicData energyIndexHistoric[HISTORY_RECORD];
int historyCounter = 0; // position courante dans le tableau de l'historique
// = position du plus ancien enregistrement
// = position du prochain enregistrement à réaliser

// Variables pour la gestion du mode BOOST
#define BURST_PERIOD 300     // Période des bursts SSR pour le mode BOOST en secondes
long boostTime = -1;         // Temps restant pour le mode BOOST, en secondes (-1 = arrêt)

// buffer pour manipuler le fichier de configuration de MaxPV! (ajuster la taille en fonction des besoins)
StaticJsonDocument<1024> jsonConfig;
// Variables pour la manipulation des adresses IP
IPAddress _ip, _gw, _sn, _dns1, _dns2, _ipmqtt;

// ***********************************************************************************
// ************************ DECLARATION DES SERVEUR ET CLIENTS ***********************
// ***********************************************************************************

AsyncWebServer webServer(HTTP_PORT);
DNSServer dnsServer;
AsyncWiFiManager wifiManager(&webServer, &dnsServer);
WiFiServer telnetServer(TELNET_PORT);
WiFiClient tcpClient;
extern AsyncMqttClient mqttClient;
WiFiUDP ntpUDP;
NTP timeClient(ntpUDP);

#if defined (MAXPV_FTP_SERVER)
FtpServer       ftpSrv;
#endif

// ***********************************************************************************
// **********************  FIN DES DEFINITIONS ET DECLARATIONS  **********************
// ***********************************************************************************



// ***********************************************************************************
// ***************************   FONCTIONS ET PROCEDURES   ***************************
// ***********************************************************************************

/////////////////////////////////////////////////////////
// setup                                               //
// Routine d'initialisation générale                   //
/////////////////////////////////////////////////////////
void setup()
{
  unsigned long refTime = millis();
  boolean APmode = true;

  //String optimization
  ecoPVConfigAll.reserve(96);
  ecoPVStatsAll.reserve(160);

  // Début du debug sur liaison série
  Serial.begin(115200);
  Serial.println(F("\nMaxPV!"));
  Serial.print(F("Version : "));
  Serial.println(MAXPV_VERSION);
  Serial.println();

  // On teste l'existence du système de fichier
  // et sinon on formatte le système de fichier
  if (!LittleFS.begin())
  {
    Serial.println(F("Système de fichier absent, formatage..."));
    LittleFS.format();
    if (LittleFS.begin())
      Serial.println(F("Système de fichier prêt et monté !"));
    else
    {
      Serial.println(F("Erreur de préparation du système de fichier, redémarrage..."));
      delay(1000);
      ESP.restart();
    }
  }
  else
    Serial.println(F("Système de fichier prêt et monté !"));

  Serial.println();

  // On teste l'existence du fichier de configuration de MaxPV!
  if (LittleFS.exists(F("/config.json")))
  {
    Serial.println(F("Fichier de configuration présent, lecture de la configuration..."));
    if (configRead())
    {
      Serial.println(F("Configuration lue et appliquée !"));
      APmode = false;
    }
    else
    {
      Serial.println(F("Fichier de configuration incorrect, effacement du fichier et redémarrage..."));
      LittleFS.remove(F("/config.json"));
      delay(1000);
      ESP.restart();
    }
  }
  else
    Serial.println(F("Fichier de configuration absent, démarrage en mode point d'accès pour la configuration réseau..."));

  Serial.println();

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setTryConnectDuringConfigPortal(true);
  wifiManager.setDebugOutput(true);
  if (static_ip != "any")
  {
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
    _dns1.fromString(static_dns1);
    _dns2.fromString(static_dns2);
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns1, _dns2);
  }
  if (APmode)
  { // Si on démarre en mode point d'accès / on efface le dernier réseau wifi connu pour forcer le mode AP
    wifiManager.resetSettings();
  }
  else
  { // on devrait se connecter au réseau local avec les paramètres connus
    Serial.print(F("Tentative de connexion au dernier réseau connu..."));
    Serial.println(F("Configuration IP, GW, SN, DNS1, DNS2 :"));
    Serial.println(_ip.toString());
    Serial.println(_gw.toString());
    Serial.println(_sn.toString());
    Serial.println(_dns1.toString());
    Serial.println(_dns2.toString());
    WiFi.mode(WIFI_STA);
  }

  wifiManager.autoConnect(SSID_CP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println();
  Serial.println(F("Connecté au réseau local en utilisant les paramètres IP, GW, SN, DNS1, DNS2 :"));
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.dnsIP(0));
  Serial.println(WiFi.dnsIP(1));

  // Configuration NTP
  timeClient.timeZone(1);
  timeClient.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  timeClient.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

  // Mise à jour des variables globales de configuration IP (systématique même si pas de changement)
  if (static_ip != "any") {
    WiFi.localIP().toString().toCharArray(static_ip, 16);
    WiFi.gatewayIP().toString().toCharArray(static_gw, 16);
    WiFi.subnetMask().toString().toCharArray(static_sn, 16);
    WiFi.dnsIP(0).toString().toCharArray(static_dns1, 16);
    WiFi.dnsIP(1).toString().toCharArray(static_dns2, 16);
  }

  //WiFi.setAutoReconnect(true);
  wifiManager.setDebugOutput(false);

  // Sauvegarde de la configuration si nécessaire
  if (shouldSaveConfig)
  {
    configWrite();
    Serial.println(F("\nConfiguration sauvegardée !"));
  }

  Serial.println(F("\n\n***** Le debug se poursuit en connexion telnet *****"));
  Serial.print(F("Dans un terminal : nc "));
  Serial.print(static_ip);
  Serial.print(F(" "));
  Serial.println(TELNET_PORT);

  // Démarrage du service TELNET
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Attente de 5 secondes pour permettre la connexion TELNET
  refTime = millis();
  while ((!telnetDiscoverClient()) && ((millis() - refTime) < 5000))
  {
    delay(200);
    Serial.print(F("."));
  }

  // Fermeture du debug serial et fermeture de la liaison série
  wifiManager.setDebugOutput(false);
  Serial.println(F("\nFermeture de la connexion série de debug et poursuite du démarrage..."));
  Serial.println(F("Bye bye !\n"));
  delay(100);
  Serial.end();

  tcpClient.println(F("\n***** Reprise de la transmission du debug *****\n"));
  tcpClient.println(F("Connexion au réseau wifi réussie !"));

  // ***********************************************************************
  // ********      DECLARATIONS DES HANDLERS DU SERVEUR WEB         ********
  // ***********************************************************************

  webServer.onNotFound([](AsyncWebServerRequest * request)
  {
    request->redirect("/");
  });

  webServer.on("/", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( LittleFS.exists ( F("/main.html") ) ) {
      request->send ( LittleFS, F("/main.html") );
    }
    else {
      response = F("Site Web non trouvé. Filesystem non chargé. Allez à : http://");
      response += WiFi.localIP().toString();
      response += F("/update pour uploader le filesystem.");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/index.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    String response = "";
    
    if ( LittleFS.exists ( F("/index.html") ) ) {
      request->send ( LittleFS, F("/index.html") );
    }
    else {
      response = F("Site Web non trouvé. Filesystem non chargé. Allez à : http://");
      response += WiFi.localIP().toString();
      response += F("/update pour uploader le filesystem.");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/configuration.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/configuration.html"));
  });

  webServer.on("/main.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/main.html"));
  });

  webServer.on("/admin.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/admin.html"));
  });

  webServer.on("/boost.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/boost.html"));
  });

  webServer.on("/credits.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/credits.html"));
  });

  webServer.on("/wizard.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/wizard.html"));
  });

  webServer.on("/maj.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    //request->send ( LittleFS, F("/maj") );
    request->redirect("/update");
  });

  webServer.on("/graph.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/graph.html"));
  });

  webServer.on("/maxpv.css", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/maxpv.css"), "text/css");
  });

  webServer.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/favicon.ico"), "image/png");
  });

  // Download le fichier de configuration de MaxPV!
  webServer.on("/DLconfig", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/config.json"), String(), true);
  });

  // Download le fichier index de MaxPV
  webServer.on("/DLindex", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/index.json"), String(), true);
  });

  // ***********************************************************************
  // ********                  HANDLERS DE L'API                    ********
  // ***********************************************************************

  webServer.on("/api/action", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    if ( request->hasParam ( "restart" ) )
      restartEcoPV ( );
    else if ( request->hasParam ( "resetindex" ) )
      {
      //reset tous les index
      resetIndexEcoPV ( );
      dimmer_indexReset ( );
      }
    else if ( request->hasParam ( "saveindex" ) )
      {
      // sauvegarde de tous les index
      saveIndexEcoPV ( );
      dimmer_indexWrite(); 
      }
      
    else if ( request->hasParam ( "saveparam" ) )
      saveConfigEcoPV ( );
    else if ( request->hasParam ( "loadparam" ) )
      loadConfigEcoPV ( );
    else if ( request->hasParam ( "format" ) )
      formatEepromEcoPV ( );
    else if ( request->hasParam ( "eraseconfigesp" ) )
      LittleFS.remove ( "/config.json" );
    else if ( request->hasParam ( "rebootesp" ) )
      rebootESP ( );
    else if ( request->hasParam ( "booston" ) )
      boostON ( );
    else if ( request->hasParam ( "boostoff" ) )
      boostOFF ( );
    else response = F("Unknown request");
    request->send ( 200, "text/plain", response );
  });

  webServer.on("/api/get", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( request->hasParam ( "configmaxpv" ) )
      request->send ( LittleFS, F("/config.json"), "text/plain" );
    else if ( request->hasParam ( "versionweb" ) )
      request->send ( LittleFS, F("/versionweb.txt"), "text/plain" );
    else {
      if ( ( request->hasParam ( "param" ) ) && ( request->getParam("param")->value().toInt() > 0 ) && ( request->getParam("param")->value().toInt() <= ( NB_PARAM - 1 ) ) )
        response = ecoPVConfig [ request->getParam("param")->value().toInt() ];
      else if ( ( request->hasParam ( "data" ) ) && ( request->getParam("data")->value().toInt() > 0 ) && ( request->getParam("data")->value().toInt() <= ( NB_STATS + NB_STATS_SUPP - 1 ) ) )
        response = ecoPVStats [ request->getParam("data")->value().toInt() ];
      else if ( request->hasParam ( "allparam" ) )
        response = ecoPVConfigAll;
      else if ( request->hasParam ( "alldata" ) )
        response = ecoPVStatsAll;
      else if ( request->hasParam ( "version" ) )
        response = ecoPVConfig [ 0 ];
      else if ( request->hasParam ( "versionmaxpv" ) )
        response = MAXPV_VERSION;
      else if ( request->hasParam ( "relaystate" ) ) {
        if ( ecoPVStats[RELAY_MODE].toInt() == STOP ) response = F("STOP");
        else if ( ecoPVStats[RELAY_MODE].toInt() == FORCE ) response = F("FORCE");
        else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000100 ) response = F("ON");
        else response = F("OFF");
      }
      else if ( request->hasParam ( "ssrstate" ) ) {
        if ( ecoPVStats[TRIAC_MODE].toInt() == STOP ) response = F("STOP");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == FORCE ) response = F("FORCE");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == AUTOM ) {
          if ( ecoPVStats[STATUS_BYTE].toInt() & B00000010 ) response = F("MAX");
          else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000001 ) response = F("ON");
          else response = F("OFF");
        }
      }
      else if ( request->hasParam ( "dimmerstate" ) ) {
        if ( dimmer_m == STOP ) response = F("STOP");
        else if ( dimmer_m == FORCE ) response = F("FORCE");
        else if ( dimmer_m == AUTOM ) {
          if ( dimmer_act == ON ) response = F("ON");
          else response = F("OFF");
        }
      }
      else if ( request->hasParam ( "allstates" ) ) {
        // tous les états dans la même requète pour optimisation
        if ( ecoPVStats[TRIAC_MODE].toInt() == STOP ) response = F("STOP,");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == FORCE ) response = F("FORCE,");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == AUTOM ) {
          if ( ecoPVStats[STATUS_BYTE].toInt() & B00000010 ) response = F("MAX,");
          else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000001 ) response = F("ON,");
          else response = F("OFF,");
        }
        if ( ecoPVStats[RELAY_MODE].toInt() == STOP ) response += F("STOP,");
        else if ( ecoPVStats[RELAY_MODE].toInt() == FORCE ) response += F("FORCE,");
        else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000100 ) response += F("ON,");
        else response += F("OFF,");
        if ( dimmer_m == STOP ) response += F("STOP,");
        else if ( dimmer_m == FORCE ) response += F("FORCE,");
        else if ( dimmer_m == AUTOM ) {
          if ( dimmer_act == ON ) response += F("ON,");
          else response += F("OFF,");
        }
        if ( contactEcoPV ) response += F("running");
        else response += F("offline");
      }
      else if ( request->hasParam ( "ping" ) )
        if ( contactEcoPV ) response = F("running");
        else response = F("offline");
      else if ( request->hasParam ( "time" ) )
        response = timeClient.formattedTime("%T");
      else response = F("Unknown request");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/api/set", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    String mystring = "";
    mystring.reserve(32);
    if ( ( request->hasParam ( "param" ) ) && ( request->hasParam ( "value" ) )
         && ( request->getParam("param")->value().toInt() > 0 ) && ( request->getParam("param")->value().toInt() <= ( NB_PARAM - 1 ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.replace( ",", "." );
      mystring.trim();
      setParamEcoPV ( request->getParam("param")->value(), mystring );
      // Note : la mise à jour de la base interne de MaxPV se fera de manière asynchrone
      // setParamEcoPV() demande à EcoPV de renvoyer tous les paramètres à MaxPV
    }
    else if ( ( request->hasParam ( "relaymode" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") ) relayModeEcoPV ( STOP );
      else if ( mystring == F("force") ) relayModeEcoPV ( FORCE );
      else if ( mystring == F("auto") ) relayModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( "ssrmode" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") ) SSRModeEcoPV ( STOP );
      else if ( mystring == F("force") ) SSRModeEcoPV ( FORCE );
      else if ( mystring == F("auto") ) SSRModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( "dimmermode" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") ) {
        dimmer_m = STOP;
        configWrite();
      }
      else if ( mystring == F("force") ) {
        dimmer_m = FORCE;
        configWrite();
      }
      else if ( mystring == F("auto") ) {
        dimmer_m = AUTOM;
        configWrite();
      }
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( "configmaxpv" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      deserializeJson ( jsonConfig, mystring );
      // check
      if (jsonConfig ["ip"])
        strlcpy ( static_ip,
                  jsonConfig ["ip"],
                  16);
      if (jsonConfig ["gateway"])
        strlcpy ( static_gw,
                  jsonConfig ["gateway"],
                  16);
      if (jsonConfig ["subnet"])
        strlcpy ( static_sn,
                  jsonConfig ["subnet"],
                  16);
      if (jsonConfig ["dns1"])
        strlcpy ( static_dns1,
                  jsonConfig ["dns1"],
                  16);
      if (jsonConfig ["dns2"])
        strlcpy ( static_dns2,
                  jsonConfig ["dns2"],
                  16);
      if (jsonConfig ["http_port"])
        httpPort = jsonConfig["http_port"];
      if (jsonConfig ["boost_duration"])
        boostDuration = jsonConfig["boost_duration"];
      if (jsonConfig ["boost_ratio"])
        boostRatio = jsonConfig["boost_ratio"];
      if (jsonConfig["mqtt_ip"])
        mqttIP = jsonConfig["mqtt_ip"].as<String>();
      if (jsonConfig ["mqtt_port"])
        mqttPort = jsonConfig["mqtt_port"];
      if (jsonConfig ["mqtt_period"])
        mqttPeriod = jsonConfig["mqtt_period"];
      if (jsonConfig["mqtt_user"])
        mqttUser = jsonConfig["mqtt_user"].as<String>();
      if (jsonConfig["mqtt_pass"])
        mqttPass = jsonConfig["mqtt_pass"].as<String>();
      if (jsonConfig["mqtt_active"])
        mqttActive = jsonConfig["mqtt_active"];
      if (jsonConfig["mqtt_det"])
        mqttDet = jsonConfig["mqtt_det"];
      if (jsonConfig["boost_timer_hour"])
        boostTimerHour = jsonConfig["boost_timer_hour"];
      if (jsonConfig["boost_timer_minute"])
        boostTimerMinute = jsonConfig["boost_timer_minute"];
      if (jsonConfig["boost_timer_active"])
        boostTimerActive = jsonConfig["boost_timer_active"];
      //relais_ext
      if (jsonConfig["a_div2_ext"])
      { a_div2_ext = jsonConfig["a_div2_ext"];
        setParamEcoPV ( "15", String(a_div2_ext) );
        saveConfigEcoPV() ; // to be checked
      }
      if (jsonConfig["a_div2_urlon"])
        strlcpy ( a_div2_urlon,
                  jsonConfig ["a_div2_urlon"],
                  255);
      if (jsonConfig["a_div2_urloff"])
        strlcpy ( a_div2_urloff,
                  jsonConfig ["a_div2_urloff"],
                  255);
      //config dimmer
      if (jsonConfig["dimmer_m"])
        dimmer_m = jsonConfig["dimmer_m"];
      if (jsonConfig["dimmer_ip"])
        strlcpy ( dimmer_ip,
                  jsonConfig ["dimmer_ip"],
                  16);
//      if (jsonConfig["dimmer_sumpourcent"])
//        dimmer_sumpourcent = jsonConfig["dimmer_sumpourcent"];
      if (jsonConfig["dimmer_sumpow"])
        dimmer_sumpow = jsonConfig["dimmer_sumpow"];
      if (jsonConfig["dimmer_period"])
        dimmer_period = jsonConfig["dimmer_period"];
      if (jsonConfig["d_p_limit"])
        d_p_limit = jsonConfig["d_p_limit"];
      //config temperature
      if (jsonConfig["temp_active"])
        tempactive = jsonConfig["temp_active"];

      configWrite ( );
    }
    else response = F("Bad request or request unknown");
    request->send ( 200, "text/plain", response );
  });

  // ***********************************************************************
  // ********             HANDLERS DES DATAS HISTORIQUES            ********
  // ***********************************************************************

  webServer.on("/api/history", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/csv");
    String timeStamp;
    float pRouted;
    float pImport;
    float pExport;
    float pImpuls;
    int localCounter;
    int lastLocalCounter;
    unsigned long timePowerFactor = 60000UL / HISTORY_INTERVAL;   // attention : HISTORY_INTERVAL sous-multiple de 60000
    // 60000 = 60 minutes par heure * 1000 Wh par kWh

    if ( request->hasParam ( "power" ) )
    {
      response->print(F("Time,Import réseau,Export réseau,Production PV,Routage ECS\r\n"));
      for (int i = 1; i < HISTORY_RECORD; i++) {
        localCounter = (historyCounter + i) % HISTORY_RECORD;
        lastLocalCounter = ((localCounter + HISTORY_RECORD - 1) % HISTORY_RECORD);
        if ( ( energyIndexHistoric[localCounter].time != 0 ) && ( (energyIndexHistoric[lastLocalCounter].time) != 0 ) ) {
          timeStamp = String (energyIndexHistoric[localCounter].time) + "000";
          pRouted = (energyIndexHistoric[localCounter].eRouted - energyIndexHistoric[lastLocalCounter].eRouted) * timePowerFactor;
          pImport = (energyIndexHistoric[localCounter].eImport - energyIndexHistoric[lastLocalCounter].eImport) * timePowerFactor;
          pExport = -(energyIndexHistoric[localCounter].eExport - energyIndexHistoric[lastLocalCounter].eExport) * timePowerFactor;
          pImpuls = (energyIndexHistoric[localCounter].eImpulsion - energyIndexHistoric[lastLocalCounter].eImpulsion) * timePowerFactor;
          response->printf("%s,%.0f,%.0f,%.0f,%.0f\r\n", timeStamp.c_str(), pImport, pExport, pImpuls, pRouted);
        }
      }
      request->send(response);
    }
    else
    {
      request->send ( 200, "text/plain", F("Unknown request") );
    }
  });

  // ***********************************************************************
  // ********             HANDLERS DES EVENEMENTS MQTT              ********
  // ***********************************************************************

  mqttClient.onConnect(onMqttConnect); // Appel de la fonction lors d'une connection MQTT établie
  mqttClient.onMessage(onMqttMessage); // Appel de la fonction lors de la réception d'un message MQTT

  // ***********************************************************************
  // ********                   FIN DES HANDLERS                    ********
  // ***********************************************************************

  // Démarrage des service réseau
  tcpClient.println(F("\nConfiguration des services web..."));
  AsyncElegantOTA.setID(MAXPV_VERSION_FULL);
  AsyncElegantOTA.begin(&webServer);
  webServer.begin();

  // Demarrage multicast DNS avec nom maxpv
  if (MDNS.begin(M_DNS)) {
    MDNS.addService("http", "tcp", 80);
    tcpClient.println(F("mDNS démarré ! Accès http://maxpv.local/"));
  }

  timeClient.begin();
  tcpClient.println(F("Services web configurés et démarrés !"));
  tcpClient.print(F("Port web : "));
  tcpClient.println(httpPort);

 // Innitialisation Index Routeur HTTP
  dimmer_indexRead ();

  // Démarrage du client MQTT si configuré
  if (mqttActive == ON) {
    tcpClient.println( F("Démarrage client Mqtt") );
    startMqtt();
  }

  // Démarrage du service FTP
#if defined (MAXPV_FTP_SERVER)
  ftpSrv.begin(LOGIN_FTP, PWD_FTP);
  tcpClient.println(F("Service FTP configuré et démarré !"));
  tcpClient.println(F("Port FTP : 21"));
  tcpClient.print(F("Login FTP : "));
  tcpClient.print(LOGIN_FTP);
  tcpClient.print(F("  password FTP : "));
  tcpClient.println(PWD_FTP);
#endif

  tcpClient.println(F("\nDémarrage de la connexion à l'Arduino..."));
  Serial.setRxBufferSize(SERIAL_BUFFER);
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(SERIALTIMEOUT);
  clearSerialInputCache();
  tcpClient.println(F("Liaison série configurée pour la communication avec l'Arduino, en attente d'un contact..."));

  while (!serialProcess()) {
    tcpClient.print(F(".")); // A VOIR
  }
  tcpClient.println(F("\nContact établi !\n"));
  contactEcoPV = true;

  // Premier peuplement des informations de configuration de EcoPV
  clearSerialInputCache();
  getAllParamEcoPV();
  yield();
  serialProcess();
  clearSerialInputCache();
  getVersionEcoPV();
  yield();
  serialProcess();
  // Récupération des informations de fonctionnement
  // En moins d'une seconde EcoPV devrait envoyer les informations
  while (!serialProcess()) { }
  tcpClient.println(F("\nDonnées récupérées de l'Arduino !\n"));

  // Peuplement des références des index journaliers
  setRefIndexJour();
 
  // Initialisation de l'historique
  initHistoric();
  tcpClient.println(F("Historiques initialisés.\n\n"));

  tcpClient.println(MAXPV_VERSION_FULL);
  tcpClient.print(F("EcoPV version "));
  tcpClient.println(ecoPVConfig[ECOPV_VERSION]);

  tcpClient.println(F("\n*** Fin du Setup ***\n"));

  // Routeur HTTP init
  dimmer_init0( );
  yield();

  // ***********************************************************************
  // ********      DEFINITION DES TACHES RECURRENTES DE TICKER      ********
  // ***********************************************************************

  // Découverte d'une connexion TELNET
  ts.add(
    0, 533, [&](void *)
  {
    telnetDiscoverClient();
  },
  nullptr, true);

  // Lecture et traitement des messages de l'Arduino sur port série
  // Vérirification du chargement du CE

  ts.add(
    1, 70, [&](void *)
  {
    if ( Serial.available ( ) ) serialProcess ( );
  },
  nullptr, true);

  // Forcage de l'envoi des paramètres du routeur EcoPV
  ts.add(
    2, 89234, [&](void *)
  {
    shouldReadParams = true;
  },
  nullptr, true);

  // Mise à jour de l'horloge par NTP
  ts.add(
    3, 20003, [&](void *)
  {
    timeClient.update();
  },
  nullptr, true);

  // Surveillance du fonctionnement du routeur et de l'Arduino
  ts.add(
    4, 617, [&](void *)
  {
    watchDogContactEcoPV();
  },
  nullptr, true);

  // Traitement des tâches FTP et DNS
  ts.add(
    5, 97, [&](void *)
  {
#if defined (MAXPV_FTP_SERVER)
    ftpSrv.handleFTP();
    yield();
#endif
    dnsServer.processNextRequest();
  },
  nullptr, true);

  // Traitement des demandes de lecture des paramètres du routeur EcoPV
  ts.add(
    6, 2679, [&](void *)
  {
    if ( shouldReadParams ) getAllParamEcoPV ( );
  },
  nullptr, true);

  // Affichage périodique d'informations de debug sur TELNET
  ts.add(
    7, 31234, [&](void *)
  {
    tcpClient.print ( F("Heap disponible : ") );
    tcpClient.print ( ESP.getFreeHeap ( ) );
    tcpClient.println ( F(" bytes") );
    tcpClient.print ( F("Heap fragmentation : ") );
    tcpClient.print ( ESP.getHeapFragmentation ( ) );
    tcpClient.println ( F(" %") );
  },
  nullptr, true);

  // Appel chaque minute le scheduler pour les tâches planifiées à la minute près
  // La périodicité est légèrement inférieure à la minute pour être sûr de ne pas rater
  // de minute. C'est à la fonction à gérer les fait qu'il puisse y avoir 2 appels à la même minute.
  ts.add(
    8, 59654, [&](void *)
  {
    timeScheduler();
  },
  nullptr, true);

  // Enregistrement de l'historique
  ts.add(
    9, HISTORY_INTERVAL * 60000UL, [&](void *)
  {
    recordHistoricData();
    dimmer_indexWrite();
  },
  nullptr, true);

  // Traitement des tâches à la seconde
  ts.add(
    10, 1000, [&](void *)
  {
    generalCounterSecond++;

    // traitement du mode BOOST
    //ajout température. Si température -> XXX on stoppe le boost
    // ajout si output chargé -> on stoppe
    //boostTime--;
    if ((tempactive == ON ) && (ecoPVStats[OUTPUTFULL].toInt() == 2))
        boostOFF();

    //controle ?
    if (boostTime == 0){
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off"); 
    //  boostOFF();
    }
    
    if (!WiFi.isConnected()) shouldCheckWifi = true;

    if (!mqttClient.connected ()) shouldCheckMQTT = true;

    //appel methode gestion relais_ext
    if (a_div2_ext == ON) relais_http ( );

    // traitement mDNS
    MDNS.update();

    // traitement Gestion Dimmer
    if ( dimmer_m != OFF || dimmer_act)
      dimmer_engine();
  },
  nullptr, true);

  // Traitement MQTT
  ts.add(
    11, 1000 * mqttPeriod, [&](void *)
  {
    // traitement MQTT
    if (mqttActive == ON)
      mqttTransmit();
  },
  nullptr, true);
}


///////////////////////////////////////////////////////////////////
// loop                                                          //
// Loop routine exécutée en boucle                               //
///////////////////////////////////////////////////////////////////
void loop()
{
  watchDogContactEcoPV();
  yield();
  // Exécution des tâches récurrentes Ticker
  ts.update();

  if (shouldCheckWifi) watchDogWifi();

  if (shouldCheckMQTT) startMqtt();
  yield();

}


///////////////////////////////////////////////////////////////////
// Fonctions                                                     //
// et Procédures                                                 //
///////////////////////////////////////////////////////////////////
bool configRead(void)
{
  // Note ici on utilise un debug sur liaison série car la fonction n'est appelé qu'au début du SETUP
  Serial.println(F("Lecture du fichier de configuration..."));
  File configFile = LittleFS.open(F("/config.json"), "r");
  if (configFile)
  {
    Serial.println(F("Configuration lue !"));
    Serial.println(F("Analyse..."));
    DeserializationError error = deserializeJson(jsonConfig, configFile);
    if (!error)
    {
      Serial.println(F("\nparsed json:"));
      serializeJsonPretty(jsonConfig, Serial);
      if (jsonConfig["ip"])
      {
        //Serial.println(F("\n\nRestauration de la configuration IP..."));
        strlcpy(static_ip,
                jsonConfig["ip"] | "any",
                16);
        strlcpy(static_gw,
                jsonConfig["gateway"] | "any",
                16);
        strlcpy(static_sn,
                jsonConfig["subnet"] | "255.255.255.0",
                16);
        strlcpy(static_dns1,
                jsonConfig["dns1"] | "any",
                16);
        strlcpy(static_dns2,
                jsonConfig["dns2"] | "8.8.8.8",
                16);
        httpPort = jsonConfig["http_port"] | 80;
        //Serial.println(F("\nRestauration des paramètres du mode boost..."));
        boostDuration = jsonConfig["boost_duration"] | 120;
        boostRatio = jsonConfig["boost_ratio"] | 100;
        //Serial.println(F("\nRestauration des paramètres MQTT..."));
        if (jsonConfig["mqtt_ip"])        mqttIP = jsonConfig["mqtt_ip"].as<String>();
        mqttPort = jsonConfig["mqtt_port"] | 1883;
        mqttPeriod = jsonConfig["mqtt_period"] | 10;
        if (jsonConfig["mqtt_user"])      mqttUser = jsonConfig["mqtt_user"].as<String>();
        if (jsonConfig["mqtt_pass"])      mqttPass = jsonConfig["mqtt_pass"].as<String>();
        mqttActive = jsonConfig["mqtt_active"] | OFF;
        mqttDet = jsonConfig["mqtt_det"] | OFF;
        boostTimerHour = jsonConfig["boost_timer_hour"] | 17;
        boostTimerMinute = jsonConfig["boost_timer_minute"] | 0;
        boostTimerActive = jsonConfig["boost_timer_active"] | OFF;
        //pour relais_ext
        a_div2_ext = jsonConfig["a_div2_ext"] | OFF;
        strlcpy(a_div2_urlon,
                jsonConfig["a_div2_urlon"] | "http://192.168.1.1",
                255);
        strlcpy(a_div2_urloff,
                jsonConfig["a_div2_urloff"] | "http://192.168.1.1",
                255);
        //config dimmer
        dimmer_m = jsonConfig["dimmer_m"];
        strlcpy ( dimmer_ip,
                  jsonConfig ["dimmer_ip"] | "dimmer1.local",
                  16);
    //    dimmer_sumpourcent = jsonConfig["dimmer_sumpourcent"];
        dimmer_sumpow = jsonConfig["dimmer_sumpow"];
        dimmer_period = jsonConfig["dimmer_period"];
        d_p_limit = jsonConfig["d_p_limit"];
        //config temperature
        tempactive = jsonConfig["temp_active"] | OFF;
        //tempmax = jsonConfig["temp_max"] | 55;
      }
      else
      {
        Serial.println(F("\n\nPas d'adresse IP dans le fichier de configuration !"));
        return false;
      }
    }
    else
    {
      Serial.println(F("Erreur durant l'analyse du fichier !"));
      return false;
    }
  configFile.close();
  }
  else
  {
    Serial.println(F("Erreur de lecture du fichier !"));
    return false;
  }
  return true;
}

void configWrite(void)
{
  jsonConfig["ip"] = static_ip;
  jsonConfig["gateway"] = static_gw;
  jsonConfig["subnet"] = static_sn;
  jsonConfig["dns1"] = static_dns1;
  jsonConfig["dns2"] = static_dns2;
  jsonConfig["http_port"] = httpPort;
  jsonConfig["boost_duration"] = boostDuration;
  jsonConfig["boost_ratio"] = boostRatio;
  jsonConfig["mqtt_ip"] = mqttIP;
  jsonConfig["mqtt_port"] = mqttPort;
  jsonConfig["mqtt_period"] = mqttPeriod;
  jsonConfig["mqtt_user"] = mqttUser;
  jsonConfig["mqtt_pass"] = mqttPass;
  jsonConfig["mqtt_active"] = mqttActive;
  jsonConfig["mqtt_det"] = mqttDet;
  jsonConfig["boost_timer_hour"] = boostTimerHour;
  jsonConfig["boost_timer_minute"] = boostTimerMinute;
  jsonConfig["boost_timer_active"] = boostTimerActive;
  //relais_ext
  jsonConfig["a_div2_ext"] = a_div2_ext;
  jsonConfig["a_div2_urlon"] = a_div2_urlon;
  jsonConfig["a_div2_urloff"] = a_div2_urloff;
  //config dimmer
  jsonConfig["dimmer_m"] = dimmer_m;
  jsonConfig["dimmer_ip"] = dimmer_ip;
  jsonConfig["dimmer_sumpow"] = dimmer_sumpow;
  jsonConfig["dimmer_period"] = dimmer_period;
  jsonConfig["d_p_limit"] = d_p_limit;
  //temperature
  jsonConfig["temp_active"] = tempactive;
  File configFile = LittleFS.open(F("/config.json"), "w");
  serializeJson(jsonConfig, configFile);
  configFile.close();
}

void rebootESP(void)
{
  dimmer_indexWrite() ;
  delay(1000);
  ESP.reset();
  //delay(1000);
}

bool telnetDiscoverClient(void)
{
  if (telnetServer.hasClient())
  {
    tcpClient = telnetServer.available();
    clearScreen();
    tcpClient.println(F("Configuration IP : "));
    tcpClient.print(F("Adresse IP : "));
    tcpClient.println(WiFi.localIP());
    tcpClient.print(F("Passerelle : "));
    tcpClient.println(WiFi.gatewayIP());
    tcpClient.print(F("Masque SR  : "));
    tcpClient.println(WiFi.subnetMask());
    tcpClient.print(F("IP DNS 1   : "));
    tcpClient.println(WiFi.dnsIP(0));
    tcpClient.print(F("IP DNS 2   : "));
    tcpClient.println(WiFi.dnsIP(1));
    //tcpClient.print(F("Port HTTP : "));
    //tcpClient.println(httpPort);
    //tcpClient.println(F("Port FTP : 21"));
    tcpClient.println();
    tcpClient.println(F("Configuration du mode BOOST : "));
    tcpClient.print(F("Durée du mode BOOST : "));
    tcpClient.print(boostDuration);
    tcpClient.println(F(" minutes"));
    tcpClient.print(F("Puissance pour le mode BOOST : "));
    tcpClient.print(boostRatio);
    tcpClient.println(F(" %"));
    if (boostTime > 0)
       {
        tcpClient.print(F(" Temps boost restant :"));
        tcpClient.println(boostTime);
       }
    tcpClient.print(F("CE chargé: "));
    tcpClient.println(ecoPVStats[OUTPUTFULL]);
    return true;
  }
  return false;
}

void saveConfigCallback()
{
  Serial.println(F("La configuration sera sauvegardée !"));
  shouldSaveConfig = true;
}

void configModeCallback(AsyncWiFiManager *myWiFiManager)
{
  Serial.print(F("Démarrage du mode point d'accès : "));
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println(F("Adresse du portail : "));
  Serial.println(WiFi.softAPIP());
}

void clearScreen(void)
{
  tcpClient.write(27);       // ESC
  tcpClient.print(F("[2J")); // clear screen
  tcpClient.write(27);       // ESC
  tcpClient.print(F("[H"));  // cursor to home
}

void clearSerialInputCache(void)
{
  while (Serial.available() > 0)
  {
    Serial.read();
  }
}

bool serialProcess(void)
{
#define END_OF_TRANSMIT '#'
  int stringCounter = 0;
  int index = 0;

String incomingData;
incomingData.reserve(160);

  // Les chaînes valides envoyées par l'arduino se terminent toujours par #
 incomingData = Serial.readStringUntil(END_OF_TRANSMIT);

  // On teste la validité de la chaîne qui doit contenir 'END' à la fin
  if (incomingData.endsWith(F("END")))
  {
    //DEBUG SERIAL
#if defined (DEBUG_SERIAL)
    tcpClient.print(F("Réception de : "));
    tcpClient.println(incomingData);
#endif
    incomingData.replace(F(",END"), "");
    contactEcoPV = true;
    refTimeContactEcoPV = millis();

    if (incomingData.startsWith(F("STATS")))
    {
      incomingData.replace(F("STATS,"), "");
      stringCounter++; // on incrémente pour placer la première valeur à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_STATS))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVStats[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVStats[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
      // Conversion des index en kWh
      ecoPVStats[INDEX_ROUTED] = String((ecoPVStats[INDEX_ROUTED].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPORT] = String((ecoPVStats[INDEX_IMPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_EXPORT] = String((ecoPVStats[INDEX_EXPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPULSION] = String((ecoPVStats[INDEX_IMPULSION].toFloat() / 1000.0), 3);

      ecoPVStatsAll = "";
      // on génère la chaîne complète des STATS pour l'API en y incluant les index de début de journée
      for (int i = 1; i < (NB_STATS + NB_STATS_SUPP); i++)
      {
        ecoPVStatsAll += ecoPVStats[i];
        if (i < (NB_STATS + NB_STATS_SUPP - 1))
          ecoPVStatsAll += F(",");
      }

    }

    else if (incomingData.startsWith(F("PARAM")))
    {
      incomingData.replace(F("PARAM,"), "");
      ecoPVConfigAll = incomingData;
      stringCounter++; // on incrémente pour placer la première valeur Vrms à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_PARAM))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVConfig[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVConfig[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
    }

    else if (incomingData.startsWith(F("VERSION")))
    {
      incomingData.replace(F("VERSION,"), "");
      index = incomingData.indexOf(',');
      if (index != -1)
      {
        ecoPVConfig[ECOPV_VERSION] = incomingData.substring(0, index);
        ecoPVStats[ECOPV_VERSION] = ecoPVConfig[ECOPV_VERSION];
      }
    }
    else if (incomingData.startsWith(F("BOOSTTIME")))
    {
      incomingData.replace(F("BOOSTTIME,"), "");
      boostTime = incomingData.toInt();
    }

    return true;
  }
  else
  {
    return false;
  }
}

void formatEepromEcoPV(void)
{
  Serial.print(F("FORMAT,END#"));
}

void getAllParamEcoPV(void)
{
  Serial.print(F("PARAM,END#"));
  shouldReadParams = false;
}

void setParamEcoPV(const String& param, String value) 
{
String command;
  command.reserve(32);
  command = F("SETPARAM,");
  if ((param.toInt() < 10) && (!param.startsWith("0")))
    command += "0";
  command += param;
  command += F(",");
  command += value;
  command += F(",END#");
  Serial.print(command);
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

void setBoostEcoPV(long Time, int Ratio)
{
  String command;
  command.reserve(32);
  command = F("SETBOOST,");
  command += Time;
  command += F(",");
  command += Ratio;
  command += F(",END#");
  Serial.print(command);
}

void getVersionEcoPV(void)
{
  Serial.print(F("VERSION,END#"));
}

void saveConfigEcoPV(void)
{
  Serial.print(F("SAVECFG,END#"));
}

void loadConfigEcoPV(void)
{
  Serial.print(F("LOADCFG,END#"));
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

void saveIndexEcoPV(void)
{
  Serial.print(F("SAVEINDX,END#"));
}

void resetIndexEcoPV(void)
{
  Serial.print(F("INDX0,END#"));
}

void restartEcoPV(void)
{
  Serial.print(F("RESET,END#"));
}

void relayModeEcoPV(byte opMode)
{
  char buff[2];
  String str;
  String command;
  str.reserve(16);
  command.reserve(32);
 command = F("SETRELAY,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);

  // Envoi du status via MQTT
  str = String(opMode);
  str.toCharArray(buff, 2);
  if (mqttClient.connected ()) mqttClient.publish(MQTT_RELAY_MODE, 0, true, buff);

}

void SSRModeEcoPV(byte opMode)
{
  char buff[2];
  char tmp[6];
  String str;
  String command;
  str.reserve(16);
  command.reserve(32);
  command = F("SETSSR,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == FORCE2)
    command += F("FORCE2");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);
  // Envoi du status via MQTT
  str = String(opMode);
  str.toCharArray(buff, 2);
  if (buff[0] == '0') strcpy(tmp, "stop");
  else if (buff[0] == '1' || buff[0] == '2' ) strcpy(tmp, "force"); else strcpy(tmp, "auto");
  if (mqttClient.connected ()) mqttClient.publish(MQTT_TRIAC_MODE, 0, true, tmp);
}

void Dimmer_act_EcoPV(byte opMode)
{
  String command;
  command.reserve(32);
  command = F("SETDIM,");
  if (opMode == OFF)
    command += F("OFF");
  if (opMode == ON)
    command += F("ON");
  command += F(",END#");
  Serial.print(command);
}

// envoi Power PV  recue par MQTT
void SetPVEcoPV(String power){
    String command = F("SETPV,");
    command += power;
  command += F(",END#");
  Serial.print(command);
}

// envoi température recue par MQTT
void SetTempEcoPV(String temp){
  String command;
  command.reserve(32);
  command = F("SETTEMP,");
  command += temp;
  command += F(",END#");
  Serial.print(command);
}

// envoi CMD pour reinitialiser des paramètres à 00:00
void ResetPEcoPV(void){
    Serial.print(F("RSTP,END#"));
}

void setRefIndexJour(void)
{
  dimmer_indexJ = 0;
  ecoPVStats[INDEX_ROUTED_J] = ecoPVStats[INDEX_ROUTED];
  ecoPVStats[INDEX_IMPORT_J] = ecoPVStats[INDEX_IMPORT];
  ecoPVStats[INDEX_EXPORT_J] = ecoPVStats[INDEX_EXPORT];
  ecoPVStats[INDEX_IMPULSION_J] = ecoPVStats[INDEX_IMPULSION];
  ResetPEcoPV();
}

void initHistoric(void)
{
  for (int i = 0; i < HISTORY_RECORD; i++)
  {
    energyIndexHistoric[i].time = 0;
    energyIndexHistoric[i].eRouted = 0;
    energyIndexHistoric[i].eImport = 0;
    energyIndexHistoric[i].eExport = 0;
    energyIndexHistoric[i].eImpulsion = 0;
  }
  historyCounter = 0;
}

void recordHistoricData(void)
{
  //float f = (float)dimmerwattsec / (float)3600000 + ecoPVStats[INDEX_ROUTED].toFloat();
  energyIndexHistoric[historyCounter].time = timeClient.epoch();
  energyIndexHistoric[historyCounter].eRouted = ecoPVStats[INDEX_ROUTED].toFloat();
  energyIndexHistoric[historyCounter].eImport = ecoPVStats[INDEX_IMPORT].toFloat();
  energyIndexHistoric[historyCounter].eExport = ecoPVStats[INDEX_EXPORT].toFloat();
  energyIndexHistoric[historyCounter].eImpulsion = ecoPVStats[INDEX_IMPULSION].toFloat();
  historyCounter++;
  historyCounter %= HISTORY_RECORD;   // Création d'un tableau circulaire
}

///////////////////////////////////////////////////////////////////
// WatchDog de surveillance de fonctionnement du routeur         //
///////////////////////////////////////////////////////////////////

void watchDogContactEcoPV(void)
{
  if ((millis() - refTimeContactEcoPV) > 1500)
    contactEcoPV = false;
}

///////////////////////////////////////////////////////////////////
// Fonctions de gestion des tâches de haut niveau                //
// liées au routage et effectuées par l'ESP :                    //
// Mode BOOST, index journaliers, historique                     //
// ModeER 0 mode temps fourni par utilisateur                    //
// ModeER 1 mode temps calculé selon E voulue - E routée         //
///////////////////////////////////////////////////////////////////
void boostON(void)
{
  float boostEnergy = 0.0;         // Energie journalière pour CE attendue en Kw
  float engy = 0.0;    
  if ( boostRatio != 0 ) {
    if (boostTimerActive == 2) {
      boostEnergy = ecoPVConfig[P_RESISTANCE].toFloat()*float(boostDuration)/60000.0;
      engy = ecoPVStats[INDEX_ROUTED].toFloat() - ecoPVStats[INDEX_ROUTED_J].toFloat();
      if (engy <= 0) engy = 0.0;
      boostTime = long( (boostEnergy - engy) / (ecoPVConfig[P_RESISTANCE].toFloat()/1000.0) * 3600 );
      if (boostTime <= 0) boostTime = 0;
      }
     else boostTime = ( 60 * boostDuration );   // conversion en secondes
    #if defined(DEBUG_SERIAL)
    tcpClient.print(F("Temps: "));
    tcpClient.println(boostTime);
    #endif
    setBoostEcoPV(boostTime, boostRatio);
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
  }
}

void boostOFF(void)
{
  boostTime = -1;
  setBoostEcoPV(0, boostRatio);
  if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
}


///////////////////////////////////////////////////////////////////
// Fonctions du WatchDog Wifi                                    //
///////////////////////////////////////////////////////////////////
void watchDogWifi ( void )
{
  // On force la déconnexion du service MQTT
  // Si les services sont déjà déconnectés, soit ils n'ont pas été configurés
  // soit les tentatives de reconnexion sont déjà en cours
  bool res;
  if (mqttClient.connected ()) mqttClient.disconnect(true);
  yield();
  res = wifiManager.autoConnect(SSID_CP);
  if (!res) {
            dimmer_indexWrite();
            //ECOPV reset ?
            ESP.restart();  
            }  
  yield();

  // Demarrage multicast DNS avec nom maxpv
  if (MDNS.begin(M_DNS)) {
    MDNS.addService("http", "tcp", 80);
    tcpClient.println(F("mDNS ok Accès http://maxpv.local/"));
  }
  yield();
  //webserver
  webServer.begin();
  delay (100);
  if (mqttActive == ON) startMqtt();

  shouldCheckWifi = false;
  shouldCheckMQTT = false;
}

void timeScheduler(void)
{
  // Le scheduler est exécuté toutes les minutes
  int day = timeClient.day();
  int hour = timeClient.hours();
  int minute = timeClient.minutes();
  int ofull = ecoPVStats[OUTPUTFULL].toInt();

  // Mise à jour des index de début de journée en début de journée à 00H00
  // indicateur de de output (CE) chargé à OFF
  if ( ( hour == 0 ) && ( minute == 0 ) ) {
    setRefIndexJour ( );
  }

  // Déclenchement du mode BOOST sur Timer
  // Déclenchement si output (CE) non chargé

  if ( ( ofull == OFF) && ( boostTimerActive != OFF ) && ( hour == boostTimerHour ) && ( minute == boostTimerMinute ) ) 
    boostON ( );
}

///////////////////////////////////////////////////////////////////
// Fonctions HTTP                                                //
//                                                               //
///////////////////////////////////////////////////////////////////

// gestion relais_http
void relais_http (void)
{
  if ( ecoPVStats[STATUS_BYTE].toInt() & B00000100 )
  {
    //ecopv relais on
    if (etatrelais == OFF )
    {
      String s(a_div2_urlon);
      //appel relais ext on
      if (appel_http(s) != -1)
        etatrelais = ON;
      yield();
    }
  }
  else
  {
    //ecopv relais off
    if (etatrelais == ON )
    {
      String s (a_div2_urloff);
      //appel relais ext off
      if (appel_http(s) != -1)
        etatrelais = OFF;
      yield();
    }
  }
}

//appel HTTP
short appel_http (String& url)
{
  short httpCode=0;
  WiFiClient client;
  HTTPClient http;
#if defined (DEBUG_HTTPC)
  tcpClient.print(F("Appel HTTP code:"));
  tcpClient.print(url);
#endif
  //http.setReuse(true);
  //http.setTimeout(5000);
  if (http.begin(client, url))
  { // HTTP
    httpCode = http.GET();
#if defined (DEBUG_HTTPC)
    tcpClient.println(httpCode);
#endif
    http.end();
    //yield ( );
    if (httpCode != 200) return -1;
    else return 1;
  }
  else
  {
#if defined (DEBUG_HTTPC)
    tcpClient.print(-1);
#endif
    return -1;
  }
}
