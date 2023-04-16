#include "httprouteur.h"

// Configuration Dimmer
short  dimmer_m = OFF;                // commande Routeur HTTP
char dimmer_ip[16] = "dimmer1.local"; // nom ou IP du dimmer
String dimmer_ip2 = "192.168.1.80";   // IP du dimmer copie de dimmer_ip ou obtenu par résolution
int dimmer_pow = 0;                   // puissance dimmer
short dimmer_act = OFF;               // mode Routeur HTTP
short dimmer_count = 0;               // compteur utilisé pour ping
short dimmer_sumpourcent = 100;       // somme des porcentages de fonctionnement max sur tous les dimmers
int dimmer_sumpow = 1000;             // somme des puissances sur tous les dimmers
short dimmer_compute = 0;
short dimmer_try = 0;                   // nb tentatives appel dimmer limité à 5
int dimmerwattsec = 0;                // Compteur Energie Dimmer
int cp = 0;
int ratio = 1;

//gestion du Dimmer secondaire
short dimmer_engine() {
  int  maxpow = dimmer_sumpow ; //ecoPVConfig[P_INSTALLPV].toInt(); // puiss. max à envoyer selon installation
  
  //Ping dimmer
  if ( (dimmer_m != OFF ) && (dimmer_count >= DIMMER_CHECK)) {
    dimmer_count = 0;
    tcpClient.println(F("Ping Routeur HTTP"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL_STATE);
    if ( appel_http(url) != 1 )
      dimmer_init( );                // tentative reinit dimmer en cas d'erreur
    return -1;
  }
  else dimmer_count++;
  dimmer_compute++;

  if ( dimmer_m == OFF && (dimmer_act == ON))
  {
    tcpClient.println(F("Routeur HTTP OFF"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + F("0");
    if ( appel_http(url) == 1 ) {
      dimmer_act = OFF;
      Dimmer_act_EcoPV(OFF);
      dimmer_count = 0;
      dimmer_pow = 0;
      dimmer_try = 0;
      //SSRModeEcoPV ( AUTOM );
    }
  }
  if ( ( dimmer_m == FORCE ) && (dimmer_pow < dimmer_sumpow))
  {
    //SSRModeEcoPV ( STOP );
    tcpClient.println(F("Routeur HTTP FORCE"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + dimmer_sumpourcent;
    if ( appel_http(url) == 1 ) {
      dimmer_act = ON;
      Dimmer_act_EcoPV(ON);
      dimmer_count = 0;
      dimmer_pow = dimmer_sumpow;
    }
    else dimmer_init();
  }
  //routage ssi ecopv ne route plus. Ecopv reste prioritaire
  if ( dimmer_m == AUTOM && dimmer_act == ON && ecoPVStats[P_ROUTED].toInt() > 0)
  {
    tcpClient.print(F("Routeur HTTP OFF ecopv:"));
    tcpClient.println(ecoPVStats[P_ROUTED].toInt());
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + F("0");
    if ( appel_http(url) == 1 ) {
      dimmer_act = OFF;
      Dimmer_act_EcoPV(OFF);
      dimmer_count = 0;
      dimmer_pow = 0;
    }
    else dimmer_init();
  }

  if ( dimmer_m == AUTOM && ecoPVStats[P_ROUTED].toInt() == 0 )
  {
    //SSRModeEcoPV ( STOP );
    // calcul Energie routée par dimmer en supposant appel toutes les secondes.
    dimmerwattsec = dimmerwattsec + dimmer_pow;
    if (dimmer_compute >= DIMMER_COMPUTE){
    dimmer_compute = 0;
    int curpower = ecoPVStats[P_ACT].toInt();
    //int margin =  ecoPVConfig[P_MARGIN].toInt();
    int dpow = 0;
    short oldp = pow_to_pourcent( dimmer_pow );
    short newp = 0;

    //DEBUG
#if defined (DEBUG_DIMMER)
    tcpClient.print(F("Routeur HTTP: "));
    tcpClient.print(curpower); tcpClient.print(F(";"));
    //tcpClient.print(margin); tcpClient.print(F(";"));
    tcpClient.println(dimmer_pow);
#endif

    // Injection et adaptation % dimmer
    if (( curpower < 0 ) && ((curpower * -1)  > DMARGIN ))
    {
      // calcul power et %
      cp = curpower * -1 - DMARGIN;
      if (cp > 800) ratio = 3;
      else if (cp > 200) ratio = 2;
      else ratio = 1;
      cp = cp / ratio ;
      //if (cp > 200 ) dpow = min((dimmer_pow + cp), (maxpow - DMARGIN));
      //else dpow = min (dimmer_pow + margin, (maxpow - DMARGIN));
      dpow = min((dimmer_pow + cp), maxpow);

      //calcul en %
      newp = pow_to_pourcent( dpow );
      String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + newp;

      if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) )
      {
#if defined (DEBUG_DIMMER)
        tcpClient.print(F("Augmentation Routeur HTTP:"));
        tcpClient.println(dpow);
#endif
        if (appel_http(url) == 1 ) {
          dimmer_pow = dpow;
          dimmer_act = ON;
          Dimmer_act_EcoPV(ON);
          dimmer_count = 0;
        }
        else dimmer_init();
      }

    }
    // consommation et adaptation % reduction dimmer
    else if ( (curpower >= 0 )  && (curpower  > DMARGIN) && (dimmer_pow > 0) )
    {
      //reduction calcul power
      cp = curpower; // - DMARGIN;
      if (cp > 800) ratio = 3;
      else if (cp > 200) ratio = 2;
      else ratio = 1;
      cp = cp / ratio;
      //if (cp > 200) dpow =  max ( (dimmer_pow - cp), 0 );
      //else dpow = max ( dimmer_pow - DMARGIN , 0);
      dpow =  max ( (dimmer_pow - cp), 0 );

      //calcul en %
      newp = pow_to_pourcent( dpow );
      String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + newp;
      if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) ) {
#if defined (DEBUG_DIMMER)
        tcpClient.print(F("Reduction Routeur HTTP:"));
        tcpClient.println(dpow);
#endif
        if (appel_http(url) == 1) {
          if (dpow > 0)  dimmer_act = ON;
          else  dimmer_act = OFF;
          Dimmer_act_EcoPV(dimmer_act);
          dimmer_pow = dpow;
          dimmer_count = 0;
        }
        else dimmer_init();
      }

    }
    }
  }
  delay(100);  // pour que thread loop passe la main
  return 1;
}//end dimmer_engine()


//dimmer init
void dimmer_init0( )
{
  tcpClient.println("Dimmer Init");
  dimmer_act = OFF ;
  Dimmer_act_EcoPV(OFF);
  dimmer_pow = 0;
  dimmer_init( );
}

void dimmer_init( )
{
  tcpClient.println("Dimmer Check");
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
  if ( ( n > 0 ) && ( appel_http(url) == 1 )) {
    dimmer_count = 0;
    dimmer_try = 0;
  }
  else {
    dimmer_try++;
    if (dimmer_try > DIMMERTRY) dimmer_failsafe( );
  }
}

//dimmer failsafe
void dimmer_failsafe( )
{
  dimmer_m = OFF;
  dimmer_act = OFF ;
  Dimmer_act_EcoPV(OFF);
  dimmer_pow = 0;
  dimmer_try = 0;
  tcpClient.println(F("Dimmer absent: failsafe"));
}

//calcul % à envoyer au dimmer à partir de puissance
int pow_to_pourcent( int power )
{
  if ( power > 3 * DMARGIN ) return ((int)(power * dimmer_sumpourcent / dimmer_sumpow));
  else return 1;
  //return (int) power * dimmer_sumpourcent / dimmer_sumpow;
}
