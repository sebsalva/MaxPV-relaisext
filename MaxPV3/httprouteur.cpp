#include "httprouteur.h"

// Configuration Dimmer
short  dimmer_m = OFF;
char dimmer_ip[16] = "dimmer1.local"; // nom ou IP du dimmer
String dimmer_ip2 = "192.168.1.80";   // IP du dimmer copie de dimmer_ip ou obtenu par résolution
int dimmer_pow;                       // puissance dimmer
short dimmer_act = OFF;               // mode dimmer
short dimmer_count  = 0;              // compteur utilisé pour ping
short dimmer_sumpourcent = 100;       // somme des porcentages de fonctionnement max sur tous les dimmers
int dimmer_sumpow = 1000;      // somme des puissances sur tous les dimmers
short dimmer_compute = 0;

//gestion du Dimmer secondaire
short dimmer_engine() {
  int  maxpow = ecoPVConfig[P_INSTALLPV].toInt(); // puiss. max à envoyer selon installation
  // si failsafe, retentative ?
  //Ping dimmer
  if ( (dimmer_m != OFF ) && (dimmer_count >= DIMMER_CHECK)) {
    dimmer_count = 0;
    tcpClient.println(F("Ping Routeur HTTP"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL_STATE);
    if ( appel_http(url) != 1 )
      dimmer_init( );                // tentative reinit dimmer 
    return -1;
  }
  else dimmer_count++;
  dimmer_compute++;

  if ( ( dimmer_m == OFF ) && (dimmer_act == ON))
  {
    tcpClient.println(F("Routeur HTTP OFF"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + F("0");
    if ( appel_http(url) == 1 ) {
      dimmer_act = OFF;
      dimmer_count = 0;
      dimmer_pow = 0;
    }
  }
  if ( ( dimmer_m == FORCE ) && (dimmer_pow < dimmer_sumpow))
  {
    tcpClient.println(F("Routeur HTTP FORCE"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + dimmer_sumpourcent;
    if ( appel_http(url) == 1 ) {
      dimmer_act = ON;
      dimmer_count = 0;
      dimmer_pow = dimmer_sumpow;
    }
  }
  //routage ssi ecopv ne route plus. Ecopv reste prioritaire
  if ( dimmer_m == AUTOM && ecoPVStats[P_ROUTED].toInt() > 0)
  {
    tcpClient.println(F("Routeur HTTP OFF"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + F("0");
    if ( appel_http(url) == 1 ) {
      dimmer_act = OFF;
      dimmer_count = 0;
      dimmer_pow = 0;
    }
  }
  if ( dimmer_m == AUTOM && ecoPVStats[P_ROUTED].toInt() == 0 && dimmer_compute == DIMMER_COMPUTE)
  {
    dimmer_compute = 0;
    //if  (ecoPVStats[TRIAC_MODE].toInt() == STOP) || (ecoPVStats[TRIAC_MODE].toInt() == AUTOM)
    //{
    int curpower = ecoPVStats[P_ACT].toInt();
    int margin =  ecoPVConfig[P_MARGIN].toInt();
    int dpow = 0;
    short oldp = pow_to_pourcent( dimmer_pow );
    short newp = 0;

    //DEBUG
    #if defined (DEBUG_DIMMER)
    tcpClient.print(F("Routeur HTTP: "));
    tcpClient.print(curpower);tcpClient.print(F(";"));
    tcpClient.print(margin);tcpClient.print(F(";"));
    tcpClient.println(dimmer_pow);
    #endif
 
    // Injection et adaptation % dimmer
    if ( curpower < 0 ) //si 0???
    {
      // calcul power et %
      
      if ((curpower * -1)  > margin )
        dpow = min((dimmer_pow + curpower * -1 - margin), (maxpow - margin));
      else dpow = 0;
      #if defined (DEBUG_DIMMER)
      tcpClient.print(F("Augmentation Routeur HTTP:"));
      tcpClient.println(dpow);
      #endif
      //calcul en %
      newp = pow_to_pourcent( dpow );
      String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + newp;
      if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) && (appel_http(url) == 1 ))
      {
        dimmer_pow = dpow;
        dimmer_act = ON;
        dimmer_count = 0;
      }
    }
    // consommation et adaptation % reduction dimmer
    else if ( (curpower >= 0 )  && (curpower  > margin) && (dimmer_pow > 0) )
    {
      //reduction calcul power
      dpow =  max ( (dimmer_pow - (curpower - margin)), 0 );
      #if defined (DEBUG_DIMMER)
      tcpClient.print(F("Reduction Routeur HTTP:"));
      tcpClient.println(dpow);
      #endif
      //calcul en %
      newp = pow_to_pourcent( dpow );
      String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + newp;
      if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) && appel_http(url) == 1 ) {
        if (dpow > 0)  dimmer_act = ON;
        else  dimmer_act = OFF;
        dimmer_pow = dpow;
        dimmer_count = 0;
        //tcpClient.println(F("Reduction Dimmer"));
      }
    }
    // calcul stat
    // fait dans MQTT avec dimmer_pow;
  }
  //}
  return 1;
}//end dimmer_engine()


//dimmer init
void dimmer_init( )
{
  tcpClient.println("Dimmer Init");
  dimmer_act = OFF ;
  dimmer_pow = 0;
  int n = 1;
  //mdns si necessaire
  if ( dimmer_ip[0] != '1' )
  {
    n = MDNS.queryService("http", "tcp");
    if ( n > 0 ) {
      for (int i = 0; i < n; ++i) {
        tcpClient.println(MDNS.hostname(i).c_str());
        if (strcmp(MDNS.hostname(i).c_str(), dimmer_ip) == 0)
          dimmer_ip2 = MDNS.IP(i).toString();
      }
    }
  }
  else dimmer_ip2 = String (dimmer_ip);
  String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + F("0");
  yield();
  if ( ( n > 0 ) && ( appel_http(url) == 1 )) {
    //tcpClient.println(F("Dimmer Init"));
    dimmer_count = 0;
  }
  else {
    dimmer_failsafe( );
  }
}

//dimmer failsafe
void dimmer_failsafe( )
{
  dimmer_m = OFF;
  dimmer_act = OFF ;
  dimmer_pow = 0;
  tcpClient.println(F("Dimmer absent: failsafe"));
}

//calcul % à envoyer au dimmer à partir de puissance
int pow_to_pourcent( int power )
{
  //if ( power > 30 ) return ( power * dimmer_sumpourcent / dimmer_sumpow );
  //else return 1;
  return (int) power * dimmer_sumpourcent / dimmer_sumpow;
}
