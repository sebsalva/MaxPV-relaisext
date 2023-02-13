#ifndef HTTPROUTEUR_H
#define HTTPROUTEUR_H

#include <Arduino.h>
#include "maxpv.h"


// URL appel Dimmer
#define DIMMER_URL "/?POWER="
#define DIMMER_URL_STATE "/state"

#define DIMMER_POW_HYSTERESIS 1 // Marge en % pour laquelle la puissance du dimmer n'est pas modifiée
#define DIMMER_CHECK 120        // Verification Ping DIMMER toutes les XX secondes 
#define DIMMER_COMPUTE 3        // Maj fonctionnment tous les DIMMER_COMPUTE tours

// Configuration Dimmer
extern short  dimmer_m;
extern char dimmer_ip[16] ;            // nom ou IP du dimmer
extern String dimmer_ip2;              // IP du dimmer copie de dimmer_ip ou obtenu par résolution
extern int dimmer_pow;                 // puissance dimmer
extern short dimmer_act;               // mode dimmer
extern short dimmer_count;             // compteur utilisé pour ping
extern short dimmer_sumpourcent ;      // somme des porcentages de fonctionnement max sur tous les dimmers
extern int dimmer_sumpow ;             // somme des puissances sur tous les dimmers


extern String ecoPVConfig[];
extern WiFiClient tcpClient;
extern String ecoPVStats[];
extern String ecoPVConfig[];

extern short appel_http ( String );

short dimmer_engine( void ) ;
void dimmer_failsafe( void );
void dimmer_init( void );
int pow_to_pourcent( int );

#endif
