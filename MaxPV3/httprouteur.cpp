#include "httprouteur.h"

// Configuration Dimmer
short  dimmer_m = OFF;                // commande Routeur HTTP
char dimmer_ip[16] = "dimmer1.local"; // nom ou IP du dimmer
String dimmer_ip2 = "192.168.1.80";   // IP du dimmer copie de dimmer_ip ou obtenu par résolution
int dimmer_pow = 0;                   // puissance dimmer
short dimmer_act = OFF;               // mode Routeur HTTP
short dimmer_count = 0;               // compteur utilisé pour ping si pas d'autre appel HTTP
short dimmer_sumpourcent = 100;       // somme des porcentages de fonctionnement max sur tous les dimmers
short dimmer_sumpow = 1000;             // somme des puissances sur tous les dimmers
short dimmer_period = 8 ;               //Période activité en secondes 
short dimmer_compute = 0;
short dimmer_try = 0;                 // compteur nb tentatives appel dimmer
int dimmerwattsec = 0;                // compteur Energie Dimmer
int cp = 0;
int ratio = 1;

//gestion du Dimmer secondaire
short dimmer_engine() {
  int  maxpow = dimmer_sumpow ; //ecoPVConfig[P_INSTALLPV].toInt(); // puiss. max à envoyer selon installation
  int curpower = ecoPVStats[P_ACT].toInt();

  //Ping dimmer
  //  dimmer_m != OFF
  if ( (dimmer_act == ON) && (dimmer_count >= DIMMER_CHECK)) {
    dimmer_count = 0;
    tcpClient.println(F("Ping Routeur HTTP"));
    String url = F("http://") + dimmer_ip2 + F(DIMMER_URL_STATE);
    if ( appel_http(url) != 1 )
      dimmer_init( );                // tentative reinit dimmer en cas d'erreur
    return -1;
  }
  else dimmer_count++;
  dimmer_compute++;

  // Routeur sur OFF
  if ( dimmer_m == OFF && (dimmer_act == ON))
  {
    tcpClient.println(F("Routeur HTTP OFF"));
    call_dimmer(0, 0);

  }
  // Routeur Mode Force
  if ( ( dimmer_m == FORCE ) && (dimmer_pow < dimmer_sumpow))
  {
    tcpClient.println(F("Routeur HTTP FORCE"));
    call_dimmer(dimmer_sumpow, dimmer_sumpourcent);

  }
  // Routeur OFF
  // Routage ssi ecopv ne route plus. Ecopv reste prioritaire
  // Ne devrait pas être utilisé
  if ( dimmer_m == AUTOM && dimmer_act == ON && ecoPVStats[P_ROUTED].toInt() > 0)
  {
    tcpClient.print(F("Routeur HTTP OFF ecopv:"));
    tcpClient.println(ecoPVStats[P_ROUTED].toInt());
    //call_dimmer(0, 0);
  }
  // Routeur Mode Automatique & EcoPv ne route pas
  if ( dimmer_m == AUTOM && ecoPVStats[P_ROUTED].toInt() == 0 )
  {
    // calcul Energie routée par dimmer en supposant appel toutes les secondes.
    dimmerwattsec = dimmerwattsec + dimmer_pow;
    if (dimmer_compute >= dimmer_period) {    // calcul toutes les XX secondes
        //DEBUG
#if defined (DEBUG_DIMMER)
    tcpClient.print(F("Routeur HTTP: "));
    tcpClient.print(curpower); tcpClient.print(F(";"));
    tcpClient.println(dimmer_pow);
#endif
      dimmer_compute = 0;
      //int margin =  ecoPVConfig[P_MARGIN].toInt();
      short dpow = 0;
      short oldp = pow_to_pourcent( dimmer_pow );
      short newp = 0;

      // Injection calcul augmentation % dimmer
      if (( curpower < 0 ) && ((curpower * -1)  > DMARGIN ))
      {
        // calcul power
        cp = curpower * -1; // - DMARGIN;
        if (cp > 800) ratio = 3;
        else if (cp > 200) ratio = 2;
        else ratio = 1;
        cp = cp / ratio ;
        //if (cp > 200 ) dpow = min((dimmer_pow + cp), (maxpow - DMARGIN));
        //else dpow = min (dimmer_pow + margin, (maxpow - DMARGIN));
        dpow = min((dimmer_pow + cp), maxpow);
        //calcul en %
        newp = pow_to_pourcent( dpow );
        if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) )
        {
#if defined (DEBUG_DIMMER)
          tcpClient.print(F("Augmentation Routeur HTTP:"));
          tcpClient.println(dpow);
#endif
          call_dimmer(dpow, newp);
        }
      }
      // consommation energie reduction dimmer
      else if ( (curpower >= 0 )  && (curpower  > DMARGIN) && (dimmer_pow > 0) )
      {
        //reduction calcul power
        cp = curpower; // - DMARGIN;
        if (cp > 800) ratio = 2;
        else if (cp > 200) ratio = 1;
        else ratio = 1;
        cp = cp / ratio;
        //if (cp > 200) dpow =  max ( (dimmer_pow - cp), 0 );
        //else dpow = max ( dimmer_pow - DMARGIN , 0);
        dpow =  max ( (dimmer_pow - cp), 0 );
        //calcul en %
        newp = pow_to_pourcent( dpow );
        if ( (abs (oldp - newp) >= DIMMER_POW_HYSTERESIS) ) {
#if defined (DEBUG_DIMMER)
          tcpClient.print(F("Reduction Routeur HTTP:"));
          tcpClient.println(dpow);
#endif
          call_dimmer(dpow, newp);
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
  else if (power > 0) return 1;
  else return 0;
  //return (int) power * dimmer_sumpourcent / dimmer_sumpow;
}

// Modification mode fonctionnement Dimmer transfert via requète HTTP
// dpow puissance, newp % dimmer associé à puisssance
void call_dimmer(int dpow, int newp)
{
  //Ecopv reçoit le fait que le HTTP Routeur sera ON ; Le SSR sera eteind
  if ((dimmer_act == OFF) && dpow >0)  Dimmer_act_EcoPV(ON);
  //Ecopv reçoit le fait que le HTTP Routeur sera OFF; Le SSR pourra s'allumer
  if ((dimmer_act == ON) && dpow  == 0)  Dimmer_act_EcoPV(OFF);  
 
  String url = F("http://") + dimmer_ip2 + F(DIMMER_URL) + newp;
  if (appel_http(url) == 1) {
    if (dpow > 0)  {dimmer_act = ON;}
    else  {dimmer_act = OFF;}
    //Dimmer_act_EcoPV(dimmer_act);
    dimmer_pow = dpow;
    dimmer_count = 0;
    dimmer_try = 0;
  }
  else dimmer_init();
}
