#ifndef HTTPROUTEUR_H
#define HTTPROUTEUR_H

#include <Arduino.h>
#include "maxpv.h"

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Configuration du DIMMER HTTP                     ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
#define DIMMER_URL "/?POWER=1&puissance="    // URL appel Dimmer
#define DIMMER_URLOFF "/?POWER=0"           // URL appel dimmer OFF
#define DIMMER_URL_STATE "/state"// URL appel Dimmer PING
#define DIMMER_CHECK 60          // Verification Ping DIMMER toutes les XX appels (1 appel une seconde)
#define DIMMERTRY 15             // borne tentatives appel Dimmer
#define DMARGIN 10		           // Consigne de régulation Dimmer
#define HTTP "http://"
#define CFGFILE "/index.json"
// ////////////////// Configuration du DIMMER HTTP UTILISE             ///////////////
// ////////////////// SSR ou ROBODYN PWM                               ///////////////
// si DIMMER HTTP mode PWM fonctionne en cumulé avec ROUTEUR SSR
// sinon DIMMER HTTP mode SSR fonctionne uniquement si ROUTEUR SSR est stoppé
#define DIMMER_IS_SSR false
// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Fin Configuration du DIMMER HTTP         	       ///////////////
// ///////////////////////////////////////////////////////////////////////////////////

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// Variables Configuration DIMMER HTTP         	   ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
extern uint8_t  dimmer_m;
extern char dimmer_ip[16] ;                // nom ou IP du dimmer
extern String dimmer_ip2;                  // IP du dimmer copie de dimmer_ip ou obtenu par résolution
extern short dimmer_pow;                 // puissance dimmer
extern uint8_t dimmer_act;                 // mode dimmer
extern uint8_t dimmer_count;               // compteur utilisé pour ping
extern short dimmer_sumpow ;               // somme des puissances sur tous les dimmers
extern unsigned int dimmerwattsec ;        // Index puissance routee par DIMMER HTTP en w/s
extern unsigned long dimmer_index ;        //Index Energie routee par DIMMER HTTP en Wh
extern unsigned long dimmer_indexJ ;       //Index Energie routee par DIMMER HTTP en Wh
extern uint8_t dimmer_period ;             //Période activité en secondes
extern unsigned short d_p_limit ;          // Limite de routage

// ///////////////////////////////////////////////////////////////////////////////////
// ////////////////// FIN Variables Configuration DIMMER HTTP          ///////////////
// ///////////////////////////////////////////////////////////////////////////////////
extern String ecoPVConfig[];
extern WiFiClient tcpClient;
extern String ecoPVStats[];
extern String ecoPVConfig[];
extern short appel_http ( String& );
short dimmer_engine( void ) ;
void dimmer_failsafe( void );
void dimmer_init( void );
void dimmer_init0( void );
void Dimmer_act_EcoPV( byte );
void call_dimmer(int);
void dimmer_indexWrite ( void );
void dimmer_indexRead ( void );
void dimmer_indexReset ( void );
#endif
