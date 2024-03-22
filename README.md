# Fork de MaxPV (température, mDNS, heure été/hiver, gestion de trois routeurs)
Fork du Routeur Solaire MaxPV, basé sur la version 3.53 

Les modifications apportées sont les suivantes :

* Ajout Routeur HTTP qui permet de gérer une chaine de deux Dimmers. Cette version combine donc 2 routeurs qui permettent de brancher 3 charges résistives:
  * Le routeur principal (EcoPv) et le routeur HTTP peuvent fonctionner ensemble.
  * Le routeur HTTP route vers des Dimmers HTTP en cascade (Idée originale provenant de ce prototype https://github.com/xlyric/pv-router-esp32).
  * Utiliser des Dimmers dont le code est ici : https://github.com/sebsalva/PV-discharge-Dimmer-AC-Dimmer-KIT-Robotdyn. Testé avec SSR Jotta et Robotdyn
  * Le routeur principal (Ecopv) est prioritaire.

* Ajout de limites de routage journalières

* Gestion de la température
  * Utilisation Capteur de température DALLAS DS18b20, lecture et calibration. Affichage sur l'interface Web et sous MQTT
  * Lecture à partir d'un flux MQTT "maxpv/settemperature" (flux pour l'instant en dur dans le code, voir mqtt.h). Permet de déporter la sonde.
  * Seuil de température à ne pas dépasser dans page Configuration Web : Stopper mode Boost si température >= seuil + hyteresis 

* Optimisation Mode Boost.
  * Mode Boost non demarré si le CE est chargé. Il y a 3 conditions (température de consigne atteinte, limite de routage atteinte et Energie exporté importante)
  * (en cours) calcul automatique du temps de boost journalié suivant le routage effectué (ex: on veut charger le CE de 5kw et le routeur a fournit 2kw au CE, alors le temps de boost sera pour charger le CE avec 3kw)  
* Modification du Client NTP. Le nouveau client gère l'heure d'été / d'hiver

* Sauvegarde des états du relais, du SSR et du routeur HTTP: si Maxpv redémarre, le relais et le SSR ne sont plus en mode automatique mais utiliserons le dernier mode sauvegardé avant extinction.

* Modification de la gestion MQTT auto-discovery pour fonctionner avec le plugin MQTT auto-discovery de Domoticz. Les modifications apportées ne devraient rien changer pour les autres serveurs de Domotique mais je n'ai pas testé.

* Lecture de la production sur un flux MQTT

* Optimisation de la partie Web

(Ces modifications sont maintenant disponibles dans Maxpv (non forké). Ces modifications ont été codées originalement et sont peut être différentes :)
* Utilisation d'un Relais HTTP (appel du relais via HTTP pour ON et OFF) en place du relais physique (l'utilisation du relais physique est toujours possible). 
* Ajout Multicast DNS (mDNS). Permet d'appeler le routeur avec http://maxpv.local/
  * Plus besoin de mettre une adresse IP fixe. A la place, le mot any (pour adresse IP, Passerelle, DNS) peut être utilisé dans la page Administration


# MaxPV
Voir les détails de MaxPv ici : https://github.com/Jetblack31/MaxPV

MaxPV! est une nouvelle interface pour EcoPV, compatible avec les montages EcoPV basés sur l'Arduino Nano et sur le Wemos ESP8266 pour la liaison Wifi. MaxPV! apporte une interface Web de configuration et de visualisation du fonctionnement, ainsi qu'une nouvelle API.

![MaxPV! main page](images/mainpage.png)  
MaxPV! hérite d'EcoPV et de son algorithme de routage. Il sera utile de se référer au dépôt EcoPV : https://github.com/Jetblack31/EcoPV

La lecture de ces fils de discussion est plus que recommandée pour la mise en oeuvre :  
* Forum photovoltaïque, discussion sur MaxPV : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=55244 
* Forum photovoltaïque, discussion sur EcoPV : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42721  
* Forum photovoltaïque, réalisation d'un PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42874  
* Forum photovoltaïque, montage du PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=43197  
