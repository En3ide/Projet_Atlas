# Découverte ARP native

Ce dossier contient le composant Linux qui construit des requêtes ARP et reçoit
directement les réponses du réseau, pour une IP ou une plage CIDR. Le code source reste à côté de l'application
Nest dans `backend/native/`, car il est compilé et exécuté séparément de TypeScript.

## Fichiers

- `arp-discovery.c` : source du programme.
- `dist/atlas-arp` : exécutable généré par la compilation.
- `tests/arp-discovery.test.c` : échanges simulés et vérifications des trames et des plages.
- `../scripts/build-native.sh` : compilation locale du scanner.
- `../scripts/deploy-native.sh` : compilation, installation et configuration des permissions.
- `../scripts/test-native.sh` : tests natifs sans accès réseau ni permission particulière.

## Compiler

Depuis `backend/`, sur une machine Linux disposant d'un compilateur C (`cc`) :

```bash
npm run build:native
```

La compilation ne lance aucune requête réseau. Le binaire doit être compilé pour
l'architecture et l'environnement Linux où il sera exécuté. Le compilateur est
nécessaire sur la machine de compilation ; l'environnement d'exécution reçoit
ensuite le binaire compatible.

## Déployer le scanner

Depuis `backend/`, avec l'utilisateur qui lance habituellement Nest :

```bash
npm run deploy:native
```

Le script compile d'abord le programme, puis utilise `sudo` uniquement pour
l'installation. Il nécessite un compilateur C pour compiler et les commandes
`setcap` / `getcap` pour configurer et vérifier la permission réseau. Il signale les
commandes manquantes avant l'installation.

Il installe le scanner dans `/usr/local/lib/atlas/atlas-arp`, avec :

- `root` comme propriétaire ;
- le groupe principal de l'utilisateur qui lance le script comme groupe d'exécution ;
- les droits `0750`, pour que seuls root et ce groupe puissent l'exécuter ;
- la permission `cap_net_raw=ep`, nécessaire aux requêtes ARP.

Le nouveau binaire est préparé et vérifié dans un fichier temporaire avant de
remplacer l'ancien. Si la compilation ou l'attribution de la permission échoue,
le scanner précédemment installé reste en place.

Sur un serveur utilisant un compte d'application dédié, indiquer son groupe :

```bash
npm run deploy:native -- --group atlas
```

Ce groupe doit déjà exister et le compte exécutant Nest doit en faire partie.
Pour une installation lancée directement par root, `--group` est obligatoire.

Pour installer un binaire déjà compilé pour le serveur cible :

```bash
npm run deploy:native -- --group atlas --skip-build
```

Le service Nest utilise le chemin `/usr/local/lib/atlas/atlas-arp` par défaut.
Il peut aussi être défini dans `backend/.env` avec `ATLAS_ARP_BINARY`.
Le serveur Nest se lance avec son utilisateur habituel, sans `sudo`.

Une compilation locale avec `npm run build:native` ne met pas à jour le binaire
installé. Relancer `npm run deploy:native` pour publier la nouvelle version et
réappliquer sa permission réseau.

## Explorer une plage d'adresses

Après le déploiement, choisir une interface et une plage IPv4 située entièrement
dans son sous-réseau :

```bash
/usr/local/lib/atlas/atlas-arp eth0 192.168.1.0/24
```

Le scanner accepte les préfixes de `/20` à `/32`, soit au maximum 4 096 adresses.
Une adresse avec un préfixe est normalisée : `192.168.1.42/24` correspond à
`192.168.1.0/24`. La plage ne doit pas dépasser le sous-réseau configuré sur
l'interface. Par exemple, une interface en `/24` ne peut pas scanner un `/20`.

Le programme :

- exclut l'IP de l'interface, ainsi que les adresses réseau et broadcast ;
- conserve les deux adresses d'un `/31` et l'adresse d'un `/32` lorsqu'elles sont
  utilisables dans le sous-réseau de l'interface ;
- espace les envois d'au moins 5 ms, soit au maximum 200 requêtes par seconde ;
- lit les réponses pendant les envois ;
- prévoit deux passages, séparés par 500 ms, en relançant seulement les IP qui
  n'ont pas encore répondu ;
- attend jusqu'à deux secondes après le dernier passage, avec une limite globale
  de 60 secondes. Un dépassement de cette limite constitue une erreur.

La sortie standard contient un objet JSON avec la plage normalisée et les IP
qui ont répondu, triées dans l'ordre numérique :

```json
{
  "network": "192.168.1.0/24",
  "devices": [
    {"ipv4": "192.168.1.1", "mac": "aa:bb:cc:dd:ee:ff"},
    {"ipv4": "192.168.1.42", "mac": "02:11:22:33:44:55"}
  ]
}
```

Les doublons sont supprimés par IP. Si plusieurs MAC répondent pour une même IP,
la première est conservée. Plusieurs IP associées à une même MAC restent dans la
liste. Seules les réponses ARP aux adresses effectivement interrogées et adressées
à notre interface sont retenues.

Si aucune IP ne répond, le résultat reste valide :

```json
{"network":"192.168.1.0/24","devices":[]}
```

## Interroger un appareil

Après le déploiement, avec un utilisateur appartenant au groupe autorisé,
remplacer l'interface et l'IP par celles du réseau local :

```bash
/usr/local/lib/atlas/atlas-arp eth0 192.168.1.42
```

La permission Linux `CAP_NET_RAW` a été attribuée au binaire par le déploiement.

Le mode sans préfixe CIDR reste disponible pour les appels directs du programme.
Il interroge uniquement cette IP et s'arrête
dès qu'il reçoit sa réponse. En l'absence de réponse, il effectue deux tentatives
et attend jusqu'à deux secondes après le dernier passage.

La sortie standard contient un objet JSON lorsqu'une réponse est reçue :

```json
{"ipv4":"192.168.1.42","mac":"aa:bb:cc:dd:ee:ff"}
```

Les messages de diagnostic sont écrits sur la sortie d'erreur. Les codes de sortie
sont :

| Code | Signification |
| --- | --- |
| `0` | Scan CIDR terminé, même vide, ou réponse reçue en mode mono-IP. |
| `1` | Arguments invalides ou erreur système, notamment permission manquante. |
| `2` | Aucune réponse correspondante reçue en mode mono-IP uniquement. |

Une absence de réponse ne prouve pas que l'appareil est éteint.

## Vérifier le scanner

Depuis `backend/` :

```bash
npm run test:native
```

Les tests utilisent des interfaces et des trames simulées. Ils vérifient les
plages, les paquets incomplets, les doublons, les formats JSON, le cadencement,
la relance des IP sans réponse et les erreurs d'envoi. Ils couvrent aussi le cas
maximal `/20`. Les accès mémoire sont vérifiés avec les outils de détection
AddressSanitizer et UndefinedBehaviorSanitizer du compilateur C. Ces tests
n'ouvrent pas de socket réseau et n'ont pas besoin de `sudo`.

## Périmètre actuel

Le scanner utilise l'adresse IPv4 principale de l'interface choisie et son masque.
Il ne lit pas le cache ARP et ne détermine ni le fabricant ni le type d'appareil.
Les réseaux isolés ou le proxy ARP peuvent affecter les résultats.

La route `POST /api/network/scans` utilise `NetworkService.scanNetwork()` avec
les champs `interfaceName` et `cidr`. Nest laisse 65 secondes au programme pour
terminer le scan. Une plage sans réponse renvoie une liste `devices` vide.
L'endpoint existant `GET /api/devices` utilise encore Nmap.
