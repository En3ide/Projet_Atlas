# Atlas

Atlas est une plateforme de supervision et de gestion d'infrastructure domestique.

L'objectif est de centraliser l'ensemble des services et équipements d'un réseau domestique au sein d'une interface web unique, moderne et sécurisée.

---

# 🚀 Fonctionnalités

Atlas permet de superviser et contrôler :

* Les appareils du réseau local
* Les caméras de surveillance
* Les capteurs environnementaux (température, humidité, etc.)
* Les services auto-hébergés
* L'accès au NAS local
* Les ressources système des serveurs
* Un terminal distant sécurisé
* Un dashboard global de la maison

---

# 🎯 Objectif du projet

Créer un véritable **Home Infrastructure Control Center** permettant de visualiser, surveiller et administrer l'ensemble de l'infrastructure d'une maison depuis une seule interface.

Atlas vise à fournir :

* Une vue centralisée de tous les équipements
* Une supervision en temps réel
* Une gestion simplifiée des services locaux
* Une architecture extensible pour l'ajout futur de nouveaux modules

---

# 🏗️ Architecture

Le projet est organisé autour d'une architecture Frontend / Backend séparée.

## Frontend

Le frontend est développé avec Nuxt 4 et fournit :

* Interface utilisateur
* Dashboard
* Authentification
* Visualisation des données
* Gestion des équipements

### Stack

* Nuxt 4
* Vue 3
* TypeScript
* Pinia
* Tailwind CSS

## Backend

Le backend centralise la logique métier et l'accès aux ressources de l'infrastructure.

### Stack

* NestJS
* TypeScript
* WebSockets
* REST API

## Base de données

* PostgreSQL

---

# 📡 Fonctionnalités temps réel

Atlas utilise les WebSockets pour :

* La supervision en temps réel
* Les notifications
* La mise à jour des capteurs
* Le suivi de l'état des équipements

---

# 🔒 Sécurité

* Authentification utilisateur
* Gestion des rôles et permissions
* Sessions sécurisées
* Communication chiffrée
* Journalisation des événements

---

# 🐳 Déploiement

Le projet est conçu pour être déployé via Docker afin de simplifier l'installation et la maintenance.

---

# ⚙️ Installation

## Prérequis

* Node.js 22.22.3+ (branche 22), 24.15.0+ (branche 24) ou 26+
* PostgreSQL
* Docker (optionnel)

## Installation

```bash
npm ci
```

## Lancement du serveur de développement

```bash
npm run dev
```

Le frontend sera accessible à l'adresse :

```text
http://localhost:3000
```

## Vérification des dépendances

```bash
npm audit
npm audit --omit=dev
npm run build
```

Les overrides de `package.json` ciblent uniquement `glob` dans `@vercel/nft`
et `minimatch` dans `readdir-glob`. Ils utilisent les versions corrigées tant
que ces outils de compilation gardent des plages de versions vulnérables.
Le build vérifie notamment le traçage des fichiers de Nitro avec ces versions.

L'alerte faible [GHSA-gcqx-w48m-w2h4 sur deepmerge](https://github.com/advisories/GHSA-gcqx-w48m-w2h4)
reste présente au 7 novembre 2026 : aucun correctif n'est publié pour `deepmerge`
4.3.1. Elle vient de `@rollup/plugin-node-resolve`, utilisé par Nitro, et remonte
dans les paquets Nuxt qui en dépendent. Elle apparaît aussi avec `--omit=dev`,
car Nuxt est déclaré dans les dépendances du projet. On conserve ce signalement
en attendant un correctif amont, sans rétrograder Nuxt ni remplacer la bibliothèque
par un fork non vérifié.

---

# 📌 Roadmap

* [X] Dashboard principal
* [ ] Authentification
* [ ] Découverte des équipements réseau
* [ ] Gestion des caméras
* [ ] Monitoring des serveurs
* [ ] Gestion du NAS
* [ ] Terminal distant
* [ ] Notifications temps réel
* [ ] Gestion multi-utilisateurs
* [ ] Déploiement Docker
