# Fork de MaxPV
Fork du Routeur Solaire MaxPV, basé sur la version 3.35.

**Attention, version testée sur prototype. Le code necessite d'avantage de tests.**

Les modifications apportées sont les suivantes :

* Utilisation d'un Relais HTTP (appel du relais via HTTP pour ON et OFF) en place du relais physique (l'utilisation du relais physique est toujours possible)
* Sauvegarde des états du relais et du SSR: si Le routeur redémarre, le relais et le SSR ne sont plus en mode automatique mais utiliserons le dernier mode sauvegardé avant extinction.
* Modification de la gestion MQTT auto-discovery pour fonctionner avec le plugin MQTT auto-discovery de domoticz
* Capteur de température DALLAS DS18b20, lecture et calibration. Affichage sur l'interface Web et sous MQTT 
* Utilisation du Capteur de température pour :
  * Donner un Seuil de température à ne pas dépasser dans page Administration
  * Stopper mode Boost si température >= seuil + hyteresis
  * Stopper Relais SSR si température >= seuil + hyteresis
  * Authoriser Allumage Relais SSR si température <= seuil + hyteresis
* Modification du Client NTP. Le nouveau client gere l'heure d'été / d'hiver
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
