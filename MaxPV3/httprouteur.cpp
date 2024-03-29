#include "httprouteur.h"
// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Variables Dimmer HTTP                            ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
uint8_t dimmer_m = OFF;                        // commande Routeur HTTP
char dimmer_ip[16] = "dimmer1.local";          // nom ou IP du dimmer
String dimmer_ip2 = "192.168.1.88";            // IP du dimmer copie de dimmer_ip ou obtenu par résolution
short dimmer_pow = 0;                        // puissance dimmer
uint8_t dimmer_act = OFF;                      // mode Routeur HTTP
uint8_t dimmer_count = 0;                      // compteur utilisé pour ping si pas d'autre appel HTTP
short dimmer_sumpow = 1000;                    // somme des puissances sur tous les dimmers
uint8_t dimmer_period = 5;                     // Période activité en secondes
uint8_t dimmer_try = 0;                        // compteur nb tentatives appel dimmer
unsigned int  dimmerwattsec = 0;               // compteur Energie Dimmer
unsigned long dimmer_index = 0;                //Index Energie routee par DIMMER HTTP en Wh
unsigned long dimmer_indexJ = 0;
uint8_t dimmer_compute = 0;
unsigned short d_p_limit = 15 ;                // limite routage en kwh
StaticJsonDocument<128> jsonIndex;             // fichier JSON pour stockage index DIMMER HTTP
// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// END Variables Dimmer HTTP                        ///////////////
// ///////////////////////////////////////////////////////////////////////////////////

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Gestion Dimmer HTTP secondaire                   ///////////////
// ////////////////// Fonction appelee tts les sec.                    ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
short dimmer_engine() {
  int cp = 0;
  short ratio = 1;               // ratio pour simulation asservissement simple
  int maxpow = dimmer_sumpow;  //ecoPVConfig[P_INSTALLPV].toInt(); // puiss. max à envoyer selon installation
  int curpower = ecoPVStats[P_ACT].toInt();
  int prouted = ecoPVStats[P_ROUTED].toInt();
  int maxrouted = ecoPVConfig[P_RESISTANCE].toInt();

  //Ping dimmer
  if ((dimmer_act == ON) && (dimmer_count >= DIMMER_CHECK)) {
    dimmer_count = 0;
#if defined(DEBUG_DIMMER)
    tcpClient.println(F("Ping Dimmer HTTP"));
#endif
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL_STATE);
    if (appel_http(url) != 1)
      dimmer_init();  // tentative reinit dimmer en cas d'erreur
    return -1;
  } else dimmer_count++;
  dimmer_compute++;

  // Routeur sur OFF
  if (dimmer_m == OFF && (dimmer_act == ON)) {
    tcpClient.println(F("Dimmer HTTP OFF"));
    call_dimmer(-(dimmer_sumpow + 1));
    return 1;
  }
  // Routeur Mode Force
  if ((dimmer_m == FORCE) && (dimmer_pow < dimmer_sumpow)) {
    tcpClient.println(F("Dimmer HTTP FORCE"));
    call_dimmer(dimmer_sumpow);
    return 1;
  }
  // Routeur OFF
  // si dépassement limite
  if (dimmer_m == AUTOM && (dimmer_indexJ > d_p_limit * 1000 ))
  {
    if (dimmer_act == ON) call_dimmer(-(dimmer_sumpow + 1));
    tcpClient.print(F("Dimmer HTTP OFF limit reached"));
    return 1;
  }
  //
  // Routeur Mode Automatique
  if (dimmer_m == AUTOM && ((DIMMER_IS_SSR && ecoPVStats[P_ROUTED].toInt() == 0) || ! DIMMER_IS_SSR)) {
    // calcul Energie routee par dimmer en supposant appel toutes les secondes.
    dimmerwattsec = dimmerwattsec + dimmer_pow;

    if (dimmer_compute >= dimmer_period) {  // calcul toutes les XX secondes
      //DEBUG
#if defined(DEBUG_DIMMER)
      tcpClient.print(F("Dimmer HTTP: "));
      tcpClient.print(curpower);
      tcpClient.print(F("/"));
      tcpClient.println(dimmer_pow);
#endif
      dimmer_compute = 0;
      // maj index
      if ( dimmerwattsec > 3600 )
      {
        dimmerwattsec -= 3600;
        dimmer_index++; // ajout 1 wh
        dimmer_indexJ++;
      }
      //int margin =  ecoPVConfig[P_MARGIN].toInt();
      short dpow = 0;
      //
      // Injection calcul augmentation % dimmer
      if ((curpower < 0) && ((curpower * -1) > DMARGIN) && (dimmer_pow < dimmer_sumpow) ) {
        // calcul power
        cp = - curpower; // + DMARGIN;
        if (cp > 800) ratio = 3;
        else if (cp > 200) ratio = 2;
        else ratio = 1;
        cp = cp / ratio;
        if (dimmer_pow + cp > dimmer_sumpow)
          cp = dimmer_sumpow - dimmer_pow;

#if defined(DEBUG_DIMMER)
        tcpClient.print(F("Dimmer HTTP +:"));
        tcpClient.println(cp);
#endif
        call_dimmer(cp);
      }
      //
      // consommation energie reduction dimmer
      else if ((curpower >= 0) && (curpower > DMARGIN) && (dimmer_pow > 0)) {
        //reduction calcul power
        cp = curpower;  // - DMARGIN;
        if (cp > 800) ratio = 2;
        else if (cp > 200) ratio = 1;
        else ratio = 1;
        cp = cp / ratio;
        if (dimmer_pow - cp < 0)
          cp = dimmer_sumpow ;
#if defined(DEBUG_DIMMER)
        tcpClient.print(F("Dimmer HTTP -:"));
        tcpClient.println(-cp);
#endif
        call_dimmer(-cp);
      }
      //reduction car EcoPv à réduit
      else if ( (ecoPVStats[TRIAC_MODE].toInt() == AUTOM )  && (prouted > 0) && (prouted < maxrouted) && (dimmer_pow > 0)) {
        cp = maxrouted - prouted + DMARGIN;
#if defined(DEBUG_DIMMER)
        tcpClient.print(F("Dimmer HTTP -ECOPV:"));
        tcpClient.println(-cp);
#endif
        call_dimmer(-cp);
      }

    }
  }
  return 1;
}  //end dimmer_engine()

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Initialisation Dimmer HTTP                       ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
void dimmer_init0() {
  tcpClient.println("Dimmer Init");
  dimmer_act = OFF;
  Dimmer_act_EcoPV(OFF);
  dimmer_pow = 0;
  dimmer_init();
}

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Controle Acces Dimmer HTTP                       ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
void dimmer_init() {
  tcpClient.println(F("Dimmer HTTP Connection Check"));
  int n = 1;
  //mdns si necessaire
  if (dimmer_ip[0] != '1') {
    n = MDNS.queryService("http", "tcp");
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        tcpClient.println(MDNS.hostname(i).c_str());
        if (strcmp(MDNS.hostname(i).c_str(), dimmer_ip) == 0)
          dimmer_ip2 = MDNS.IP(i).toString();
      }
    }
  } else dimmer_ip2 = String(dimmer_ip);
  String url = F("http://") + dimmer_ip2 + F(DIMMER_URLOFF);
  if ((n > 0) && (appel_http(url) == 1)) {
    dimmer_count = 0;
    dimmer_try = 0;
  } else {
    dimmer_try++;
    if (dimmer_try > DIMMERTRY) dimmer_failsafe();
  }
}

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Failsafe Dimmer HTTP                             ///////////////
// ////////////////// Pas de connection, reset                         ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
void dimmer_failsafe() {
  dimmer_m = OFF;
  dimmer_act = OFF;
  Dimmer_act_EcoPV(OFF);
  dimmer_pow = 0;
  dimmer_try = 0;
  tcpClient.println(F("Dimmer HTTP: failsafe"));
}


// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Appel Dimmer HTTP			                           ///////////////
// ////////////////// dpow puissance,newp % dimmer associé à puisssance///////////////
// ///////////////////////////////////////////////////////////////////////////////////
void call_dimmer(int dpow) {
  String url;
  //Ecopv reçoit le fait que le HTTP Routeur sera ON ; Bloque la puissance du routage du SSR. Avant appel dimmer
  if ((dimmer_act == OFF) && dimmer_pow > 0) Dimmer_act_EcoPV(ON);
  if ( dimmer_pow + dpow <= 0 ) //- dpow > dimmer_sumpow)
    url = HTTP + dimmer_ip2 + F(DIMMER_URLOFF);
  else
    url = HTTP + dimmer_ip2 + F(DIMMER_URL) + dpow; // envoi puissance régulation positive ou neg
  if (appel_http(url) == 1) {
    dimmer_pow = dimmer_pow + dpow;
    if ( dimmer_pow < 0 ) dimmer_pow = 0;
    if ( dimmer_pow > dimmer_sumpow ) dimmer_pow = dimmer_sumpow;
    //Ecopv reçoit le fait que le HTTP Routeur sera OFF; Le SSR pourra s'allumer. Fait après appel dimmer au cas ou la requete HTTP ne fonctionne pas
    if ((dimmer_act == ON) && dimmer_pow <= 0) Dimmer_act_EcoPV(OFF);
    if (dimmer_pow > 0) {
      dimmer_act = ON;
    } else {
      dimmer_act = OFF;
    }
    dimmer_count = 0;
    dimmer_try = 0;
  } else dimmer_init();
  delay(100);  // pour que thread loop passe la main
}

///////////////////////////////////////////////////////////////////////////////////////
// dimmer_indexReset                                                                 //
// Fonction de reset des index                                                       //
///////////////////////////////////////////////////////////////////////////////////////
void dimmer_indexReset(void) {
  dimmer_index = 0;
  dimmer_indexJ = 0;
  dimmer_indexWrite();
}

///////////////////////////////////////////////////////////////////////////////////////
// dimmer_indexWrite                                                                 //
// Fonction d'écriture des index                                                     //
///////////////////////////////////////////////////////////////////////////////////////
void dimmer_indexWrite(void) {
  File configFile = LittleFS.open(CFGFILE, "w");
  jsonIndex["dimmer_idx"] = dimmer_index;
  serializeJson(jsonIndex, configFile);
  configFile.close();
}

///////////////////////////////////////////////////////////////////////////////////////
// dimmer_indexRead                                                                  //
// Fonction d'écriture des index                                                     //
///////////////////////////////////////////////////////////////////////////////////////
void dimmer_indexRead(void) {
  File configFile = LittleFS.open(CFGFILE, "r");
  if (configFile) {
    DeserializationError error = deserializeJson(jsonIndex, configFile);
    if (!error) {
      serializeJsonPretty(jsonIndex, Serial);
      dimmer_index = jsonIndex["dimmer_idx"] | 0;
    }
  }
  configFile.close();
}
