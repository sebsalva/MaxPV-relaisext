#ifndef MQTT_H
#define MQTT_H

#include <AsyncMqtt_Generic.h>

// ***********************************************************************************
// ****************************   MQTT                    ****************************
// ***********************************************************************************
#define DEFAULT_MQTT_SERVER     "192.168.1.100" // Serveur MQTT par défaut
#define DEFAULT_MQTT_PORT       1883            // Port serveur MQTT
#define RECONNECT_TIME          5               // Délai de reconnexion en secondes suite à perte de connexion du serveur mqtt
// Définition des topics MQTT
#define DEFAULT_MQTT_PUBLISH_PERIOD   10     // en secondes, intervalle de publication MQTT

// Définition des channels MQTT
#define MQTT_STATE            "maxpv/state"
#define MQTT_V_RMS            "maxpv/vrms"
#define MQTT_I_RMS            "maxpv/irms"
#define MQTT_P_ACT            "maxpv/pact"
#define MQTT_P_APP            "maxpv/papp"
#define MQTT_P_ROUTED         "maxpv/prouted"
#define MQTT_P_ROUTED2        "maxpv/prouted2"
#define MQTT_P_EXPORT         "maxpv/pexport"
#define MQTT_P_IMPULSION      "maxpv/pimpulsion"
#define MQTT_COS_PHI          "maxpv/cosphi"
#define MQTT_INDEX_ROUTED     "maxpv/indexrouted"
#define MQTT_INDEX_ROUTED2    "maxpv/indexrouted2"
#define MQTT_INDEX_IMPORT     "maxpv/indeximport"
#define MQTT_INDEX_EXPORT     "maxpv/indexexport"
#define MQTT_INDEX_IMPULSION  "maxpv/indeximpulsion"
#define MQTT_TRIAC_MODE       "maxpv/triacmode"
#define MQTT_SET_TRIAC_MODE   "maxpv/triacmode/set"
#define MQTT_RELAY_MODE       "maxpv/relaymode"
#define MQTT_SET_RELAY_MODE   "maxpv/relaymode/set"
#define MQTT_DIMMER_MODE      "maxpv/dimmermode"
#define MQTT_SET_DIMMER_MODE  "maxpv/dimmermode/set"
#define MQTT_BOOST_MODE       "maxpv/boost"
#define MQTT_SET_BOOST_MODE   "maxpv/boost/set"
#define MQTT_STATUS_BYTE      "maxpv/statusbyte"
#define MQTT_TEMP             "maxpv/temperature"
#define MQTT_SET_TEMP         "maxpv/settemperature"
#define MQTT_SET_PV           "shellies/shellyem-34945470F5B8/emeter/0/power"

// Variables Configuration de MQTT
String mqttIP;                                      // IP du serveur MQTT
String mqttUser;                                    // Utilisateur du serveur MQTT - Optionnel : si vide, pas d'authentification
String mqttPass;                                    // Mot de passe du serveur MQTT
uint16_t mqttPort   = DEFAULT_MQTT_PORT;            // Port du serveur MQTT
short mqttPeriod      = DEFAULT_MQTT_PUBLISH_PERIOD;  // Période de transmission en secondes
short mqttActive    = OFF;
short mqttDet       = OFF;

extern int powerpv;                                 // Puiss. PV reçue par MQTT
//extern short temp;

AsyncMqttClient mqttClient;
// Définition tâches de Ticker
Ticker mqttReconnectTimer;

extern void relayModeEcoPV(byte);
extern void SSRModeEcoPV(byte );
extern void boostON(void);
extern void boostOFF(void);
extern int boostTime ;
extern bool shouldCheckMQTT ;

///////////////////////////////////////////////////////////////////
// Fonctions de gestion de la connexion Mqtt                     //
//                                                               //
///////////////////////////////////////////////////////////////////

void onMqttConnect(bool sessionPresent)
{
  // Souscriptions aux topics pour gérer les états relais, SSR, boost, température et Puiss. PV
  mqttClient.subscribe(MQTT_SET_RELAY_MODE, 0);
  mqttClient.subscribe(MQTT_SET_TRIAC_MODE, 0);
  mqttClient.subscribe(MQTT_SET_DIMMER_MODE, 0);
  mqttClient.subscribe(MQTT_SET_BOOST_MODE, 0);
  mqttClient.subscribe(MQTT_SET_TEMP, 0);
  mqttClient.subscribe(MQTT_SET_PV, 0);

  // Publication du status
  mqttClient.publish(MQTT_STATE, 0, true, "connected");

  // On crée les informations pour le Discovery HomeAssistant
  // On crée un identifiant unique
  String deviceID = F("maxpv");
  deviceID += ESP.getChipId();

  // On récupère l'URL d'accès
  String ip_url = F("http://") + WiFi.localIP().toString();

  // On crée les templates du topic et du Payload
  String configTopicTemplate = String(F("homeassistant/#COMPONENT#/#DEVICEID#/#DEVICEID##SENSORID#/config"));
  configTopicTemplate.replace(F("#DEVICEID#"), deviceID);

  // Capteurs
  String configPayloadTemplate = String(F(
                                          "{"
                                          "\"dev\":{"
                                          "\"ids\":\"#DEVICEID#\","
                                          "\"name\":\"MaxPV\","
                                          "\"mdl\":\"MaxPV!\","
                                          "\"mf\":\"JetBlack\","
                                          "\"sw\":\"#VERSION#\","
                                          "\"cu\":\"#IP#\""
                                          "},"
                                          "\"avty_t\":\"maxpv/state\","
                                          "\"pl_avail\":\"connected\","
                                          "\"pl_not_avail\":\"disconnected\","
                                          "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                                          "\"dev_cla\":\"#DEVICECLASS#\","
                                          "\"stat_cla\":\"#STATECLASS#\","
                                          "\"name\":\"#SENSORNAME#\","
                                          "\"stat_t\":\"#STATETOPIC#\","
                                          "\"unit_of_meas\":\"#UNIT#\""
                                          "}"));
  configPayloadTemplate.replace(" ", "");
  configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
  configPayloadTemplate.replace(F("#VERSION#"), MAXPV_VERSION);
  configPayloadTemplate.replace(F("#IP#"), ip_url);

  String topic;
  String payload;

  // On publie la config pour chaque sensor
  if (mqttDet == ON) {
    // V_RMS
    topic = configTopicTemplate;
    topic.replace(F("#COMPONENT#"), F("sensor"));
    topic.replace(F("#SENSORID#"), F("Tension"));

    payload = configPayloadTemplate;
    payload.replace(F("#SENSORID#"), F("Tension"));
    payload.replace(F("#SENSORNAME#"), F("Tension"));
    payload.replace(F("#CLASS#"), F("voltage"));
    payload.replace(F("#STATETOPIC#"), F(MQTT_V_RMS));
    payload.replace(F("#UNIT#"), F("V"));
    mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

    // I_RMS
    topic = configTopicTemplate;
    topic.replace(F("#COMPONENT#"), F("sensor"));
    topic.replace(F("#SENSORID#"), F("Courant"));

    payload = configPayloadTemplate;
    payload.replace(F("#SENSORID#"), F("Courant"));
    payload.replace(F("#SENSORNAME#"), F("Courant"));
    payload.replace(F("#DEVICECLASS#"), F("current"));
    payload.replace(F("#STATECLASS#"), F("measurement"));
    payload.replace(F("#STATETOPIC#"), F(MQTT_I_RMS));
    payload.replace(F("#UNIT#"), F("A"));
    mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

    // P_APP
    topic = configTopicTemplate;
    topic.replace(F("#COMPONENT#"), F("sensor"));
    topic.replace(F("#SENSORID#"), F("PuissanceApparente"));

    payload = configPayloadTemplate;
    payload.replace(F("#SENSORID#"), F("PuissanceApparente"));
    payload.replace(F("#SENSORNAME#"), F("Puissance apparente"));
    payload.replace(F("#DEVICECLASS#"), F("apparent_power"));
    payload.replace(F("#STATECLASS#"), F("measurement"));
    payload.replace(F("#STATETOPIC#"), F(MQTT_P_APP));
    payload.replace(F("#UNIT#"), F("VA"));
    mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

    // MQTT_COS_PHI
    topic = configTopicTemplate;
    topic.replace(F("#COMPONENT#"), F("sensor"));
    topic.replace(F("#SENSORID#"), F("FacteurPuissance"));

    payload = configPayloadTemplate;
    payload.replace(F("#SENSORID#"), F("FacteurPuissance"));
    payload.replace(F("#SENSORNAME#"), F("Facteur de puissance"));
    //payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
    payload.replace(F("#DEVICECLASS#"), F("power_factor"));
    payload.replace(F("#STATECLASS#"), F("measurement"));
    payload.replace(F("#STATETOPIC#"), F(MQTT_COS_PHI));
    //payload.replace(F("#UNIT#"), "");
    payload.replace(F("\"unit_of_meas\":\"#UNIT#\""), "");
    mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  }

  // MQTT_P_ACT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Import_P"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Import_P"));
  payload.replace(F("#SENSORNAME#"), F("Import_P"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));  
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_ACT));
  payload.replace(F("#UNIT#"), F("W"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_EXPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Export_P"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Export_P"));
  payload.replace(F("#SENSORNAME#"), F("Export_P"));
  payload.replace(F("#CLASS#"), F("power"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_EXPORT));
  payload.replace(F("#UNIT#"), F("W"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_ROUTED
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Routage_P"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Routage_P"));
  payload.replace(F("#SENSORNAME#"), F("Routage_P"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_ROUTED));
  payload.replace(F("#UNIT#"), F("W"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_ROUTED2
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("HTTPRout_P"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("HTTPRout_P"));
  payload.replace(F("#SENSORNAME#"), F("HTTPRout_P"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_ROUTED2));
  payload.replace(F("#UNIT#"), F("W"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_IMPULSION
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Production_P"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Production_P"));
  payload.replace(F("#SENSORNAME#"), F("Production_P"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_IMPULSION));
  payload.replace(F("#UNIT#"), F("W"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_ROUTED
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Routage_E"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Routage_E"));
  payload.replace(F("#SENSORNAME#"), F("Routage_E"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_ROUTED));
  payload.replace(F("#UNIT#"), F("KWh"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_ROUTED
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("HTTPRout_E"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("HTTPRout_E"));
  payload.replace(F("#SENSORNAME#"), F("HTTPRout_E"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_ROUTED2));
  payload.replace(F("#UNIT#"), F("KWh"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_IMPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Import_E"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Import_E"));
  payload.replace(F("#SENSORNAME#"), F("Import_E"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPORT));
  payload.replace(F("#UNIT#"), F("kWh"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_EXPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Export_E"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Export_E"));
  payload.replace(F("#SENSORNAME#"), F("Export_E"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_EXPORT));
  payload.replace(F("#UNIT#"), F("kWh"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_IMPULSION
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Production_E"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Production_E"));
  payload.replace(F("#SENSORNAME#"), F("Production_E"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPULSION));
  payload.replace(F("#UNIT#"), F("kWh"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_TEMP
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Routeur_Temp"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Routeur_Temp"));
  payload.replace(F("#SENSORNAME#"), F("Routeur_Temp"));
  payload.replace(F("#DEVICECLASS#"), F("temperature"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_TEMP));
  payload.replace(F("#UNIT#"), F("°C"));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_TRIAC_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("SSR"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("SSR"));
  payload.replace(F("#SENSORNAME#"), F("SSR"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_TRIAC_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"options\":[\"stop\",\"force\",\"auto\"]"));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_TRIAC_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_RELAY_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("Relais"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Relais"));
  payload.replace(F("#SENSORNAME#"), F("Relais"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_RELAY_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"options\":[\"stop\",\"force\",\"auto\"]"));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_RELAY_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_Routeur_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("Routeur HTTP"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Routeur HTTP"));
  payload.replace(F("#SENSORNAME#"), F("Routeur HTTP"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_DIMMER_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"options\":[\"stop\",\"force\",\"auto\"]"));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_DIMMER_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_BOOST_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("switch"));
  topic.replace(F("#SENSORID#"), F("Boost"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Boost"));
  payload.replace(F("#SENSORNAME#"), F("Boost"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_BOOST_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"payload_on\":\"on\","
                    "\"payload_off\":\"off\""));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_BOOST_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
}


void mqttConnect (void)
{
  mqttClient.connect();
};

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttClient.clearQueue();
  mqttReconnectTimer.once(RECONNECT_TIME, mqttConnect);
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total)
{
  char tmp[6];
  short t=0;
  
  // A la réception d'un message sur un des topics en écoute
  // on vérifie le topic concerné et la commande reçue
  if (String(topic).startsWith(F(MQTT_SET_RELAY_MODE))) {
    if ( String(payload).startsWith(F("stop")) ) relayModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) ) relayModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) ) relayModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_TRIAC_MODE))) {
    if ( String(payload).startsWith(F("stop")) ) SSRModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) ) SSRModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) ) SSRModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_DIMMER_MODE))) {
    if ( String(payload).startsWith(F("stop")) ) dimmer_m = STOP;
    else if ( String(payload).startsWith(F("force")) ) dimmer_m = FORCE;
    else if ( String(payload).startsWith(F("auto")) ) dimmer_m = AUTOM;
    // dimmer
    if (dimmer_m == OFF) strcpy(tmp, "stop");
    else if (dimmer_m == FORCE) strcpy(tmp, "force"); else strcpy(tmp, "auto");
    if (mqttClient.connected ()) mqttClient.publish(MQTT_DIMMER_MODE, 0, true, tmp);
    //if (mqttClient.connected ()) mqttClient.publish(MQTT_DIMMER_MODE, 0, true, payload);
  }
  else if (String(topic).startsWith(F(MQTT_SET_BOOST_MODE))) {
    if ( String(payload).startsWith(F("on")) ) boostON ( );
    else if ( String(payload).startsWith(F("off")) ) boostOFF ( );
  }
   else if (String(topic).startsWith(F(MQTT_SET_PV))) {
    t = atoi(payload);
    if (t > 0) powerpv = t;
    else powerpv = 0; 
    ecoPVConfig[P_IMPULSION] = String(powerpv);
    SetPVEcoPV(ecoPVConfig[P_IMPULSION]) ;
   }
    else if (String(topic).startsWith(F(MQTT_SET_TEMP))) {
    t = atoi(payload);
    if (t > 0 && t < 100)  SetTempEcoPV(String(t));    
   
   }  
}

void mqttTransmit(void)
{
  char buf[16];
  char tmp[16];
  int power = 0;

  if (mqttClient.connected ()) {          // On vérifie si on est connecté
    if (mqttDet == ON)        //transmission détaillée
    {
      mqttClient.publish(MQTT_V_RMS, 0, true, ecoPVStats[V_RMS].c_str());
      mqttClient.publish(MQTT_I_RMS, 0, true, ecoPVStats[I_RMS].c_str());
      mqttClient.publish(MQTT_P_APP, 0, true, ecoPVStats[P_APP].c_str());
      mqttClient.publish(MQTT_COS_PHI, 0, true, ecoPVStats[COS_PHI].c_str());
      mqttClient.publish(MQTT_STATUS_BYTE, 0, true, ecoPVStats[STATUS_BYTE].c_str());
    }
    mqttClient.publish(MQTT_P_ACT, 0, true, ecoPVStats[P_ACT].c_str());
    power = ecoPVStats[P_ACT].toInt();
    if (power < 0)  sprintf(tmp, "%d", -power);
    else  strcpy(tmp, "auto");
    mqttClient.publish(MQTT_P_EXPORT, 0, true, tmp);
    // stat routage + routage Dimmer
    //if (dimmer_m == AUTOM ) {
    sprintf(tmp, "%d", dimmer_pow);
    mqttClient.publish(MQTT_P_ROUTED2, 0, true, tmp);
    sprintf(tmp, "%f", ((float)dimmer_index/1000.0));
    mqttClient.publish(MQTT_INDEX_ROUTED2, 0, true, tmp);
    //}
    //if (ecoPVStats[TRIAC_MODE].toInt() != STOP) {
    mqttClient.publish(MQTT_P_ROUTED, 0, true, ecoPVStats[P_ROUTED].c_str());

    sprintf(tmp, "%f",  (ecoPVStats[INDEX_ROUTED].toFloat()));
    mqttClient.publish(MQTT_INDEX_ROUTED, 0, true, tmp);
    //}
    power  = ecoPVStats[P_IMPULSION].toInt();
    if (power >= 0) {
      mqttClient.publish(MQTT_P_IMPULSION, 0, true, ecoPVStats[P_IMPULSION].c_str());
      mqttClient.publish(MQTT_INDEX_IMPULSION, 0, true, ecoPVStats[INDEX_IMPULSION].c_str());
    }
    yield ( );

    mqttClient.publish(MQTT_INDEX_IMPORT, 0, true, ecoPVStats[INDEX_IMPORT].c_str());
    mqttClient.publish(MQTT_INDEX_EXPORT, 0, true, ecoPVStats[INDEX_EXPORT].c_str());
    mqttClient.publish(MQTT_TEMP, 0, true, ecoPVStats[TEMP].c_str());
    ecoPVStats[TRIAC_MODE].toCharArray(buf, 16);
    if (buf[0] == '0') strcpy(tmp, "stop");
    else if (buf[0] == '1') strcpy(tmp, "force"); else strcpy(tmp, "auto");
    mqttClient.publish(MQTT_TRIAC_MODE, 0, true, tmp);
    ecoPVStats[RELAY_MODE].toCharArray(buf, 16);
    if (buf[0] == '0') strcpy(tmp, "stop");
    else if (buf[0] == '1') strcpy(tmp, "force"); else strcpy(tmp, "auto");
    mqttClient.publish(MQTT_RELAY_MODE, 0, true, tmp);
    if (boostTime == -1) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
    else mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
    // dimmer
    if (dimmer_m == OFF) strcpy(tmp, "stop");
    else if (dimmer_m == FORCE) strcpy(tmp, "force"); else strcpy(tmp, "auto");
    mqttClient.publish(MQTT_DIMMER_MODE, 0, true, tmp);
  }
  //else startMqtt();        // Sinon on ne transmet pas mais on tente la reconnexion
}


void startMqtt (void) {
  IPAddress _ipmqtt;
  mqttClient.onConnect(onMqttConnect);        // Appel de la fonction lors d'une connexion MQTT établie
  mqttClient.onDisconnect(onMqttDisconnect);  // Appel de la fonction lors d'une déconnexion MQTT
  mqttClient.onMessage(onMqttMessage);        // Appel de la fonction lors de la réception d'un message MQTT
  _ipmqtt.fromString(mqttIP);
  if ( mqttUser.length() != 0 ) {
    mqttClient.setCredentials(mqttUser.c_str(), mqttPass.c_str());
  }
  mqttClient.setServer(_ipmqtt, mqttPort);
  mqttClient.setWill(MQTT_STATE, 0, true, "disconnected");
  mqttConnect();
  delay(3000);
  shouldCheckMQTT = false;
}

#endif
