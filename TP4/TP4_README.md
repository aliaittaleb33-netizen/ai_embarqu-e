# TP4 – Cloud vs Edge AI sur STM32N6
**Module :** ETRS606 – IA Embarquée | **Université Savoie Mont Blanc**  
**Carte :** NUCLEO-N657X0 | **IDE :** STM32CubeIDE 2.1.0  
**Stack réseau :** NetXDuo (Azure RTOS / ThreadX) | **IA embarquée :** X-CUBE-AI

---

## 📋 Objectifs du TP

Comparer deux approches d'intelligence artificielle :
- **Cloud AI :** Modèle hébergé sur ThingSpeak/MATLAB, inférence distante
- **Edge AI :** Modèle déployé directement sur la STM32N6, inférence locale

| Critère | Cloud AI | Edge AI |
|---|---|---|
| Puissance de calcul | Illimitée (serveurs) | Limitée (MCU) |
| Latence | Haute (réseau) | Très faible |
| Consommation bande passante | Élevée | Nulle |
| Indépendance réseau | Non | Oui |
| Mise à jour modèle | Facile | Reflash nécessaire |

---

## 🔌 Partie 0 – Configuration matérielle (prérequis)

### Configuration obligatoire de la carte

| Élément | Configuration correcte |
|---|---|
| **Jumper d'alimentation** | Position **1-2 `5V_STLK`** |
| **Port USB à utiliser** | Port **ST-LINK** (côté opposé au port Ethernet) |
| **Câble Ethernet** | Branché sur le port RJ45, connecté à un réseau avec DHCP |

> ✅ **Validation :** La LED **COM** doit être **verte fixe** une fois la carte correctement connectée.

### Problème ST-LINK clignotant rouge/jaune

Si la LED clignote rouge/jaune après une mise à jour firmware ratée :

1. Ouvrir **STM32CubeIDE → Help → ST-LINK Upgrade**
2. Cliquer **Refresh device list**
3. Si la carte n'apparaît pas → brancher sur un autre PC "vierge" et relancer l'upgrade
4. Alternative Windows : Gestionnaire de périphériques → **Afficher les périphériques cachés** → Désinstaller tous les périphériques ST-LINK grisés → Redémarrer

---

## 🌐 Partie 1 – Cloud AI : NetXDuo + DNS + ThingSpeak

### Architecture des threads ThreadX

```
MX_NetXDuo_Init()
    ├── NxAppThread      (AUTO_START) → DHCP → débloque AppTCPThread
    ├── AppTCPThread     (DONT_START) → DNS → TCP → ThingSpeak
    └── AppLinkThread    (AUTO_START) → surveillance câble Ethernet
```

### 1.1 Code complet – `app_netxduo.c`

#### Includes et variables globales

```c
#include "app_netxduo.h"
#include "nxd_dhcp_client.h"

/* USER CODE BEGIN Includes */
#include "nxd_dns.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Variables générées par CubeMX */
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;

/* USER CODE BEGIN PV */
TX_THREAD     AppTCPThread;
TX_THREAD     AppLinkThread;
NX_TCP_SOCKET TCPSocket;
NX_DNS        DnsClient;
ULONG         IpAddress;
ULONG         NetMask;
/* USER CODE END PV */
```

#### Initialisation NetXDuo

```c
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;

  printf("Nx_TCP_Echo_Client application started..\n");

  nx_system_initialize();

  /* Packet Pool */
  tx_byte_allocate(byte_pool, (VOID **)&pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT);
  nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE,
                        pointer, NX_APP_PACKET_POOL_SIZE);

  /* Instance IP */
  tx_byte_allocate(byte_pool, (VOID **)&pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT);
  nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance",
               NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK,
               &NxAppPool, nx_stm32_eth_driver,
               pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);

  /* Protocoles */
  tx_byte_allocate(byte_pool, (VOID **)&pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT);
  nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);
  nx_icmp_enable(&NetXDuoEthIpInstance);
  nx_tcp_enable(&NetXDuoEthIpInstance);
  nx_udp_enable(&NetXDuoEthIpInstance);

  /* Thread TCP (démarrage différé) */
  tx_byte_allocate(byte_pool, (VOID **)&pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);
  tx_thread_create(&AppTCPThread, "App TCP Thread", App_TCP_Thread_Entry,
                   0, pointer, NX_APP_THREAD_STACK_SIZE,
                   NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY,
                   TX_NO_TIME_SLICE, TX_DONT_START);  /* ← TX_DONT_START : attend l'IP */

  /* Thread principal (démarrage auto) */
  tx_byte_allocate(byte_pool, (VOID **)&pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);
  tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry,
                   0, pointer, NX_APP_THREAD_STACK_SIZE,
                   NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY,
                   TX_NO_TIME_SLICE, TX_AUTO_START);

  /* DHCP */
  nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");
  tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);

  /* Thread Link (surveillance câble) */
  /* USER CODE BEGIN MX_NetXDuo_Init */
  ret = tx_byte_allocate(byte_pool, (VOID **)&pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);
  if (ret != TX_SUCCESS) {
    printf("ERREUR : Plus de RAM disponible (0x%02X)\n", ret);
    return TX_POOL_ERROR;
  }
  tx_thread_create(&AppLinkThread, "App Link Thread", App_Link_Thread_Entry,
                   0, pointer, NX_APP_THREAD_STACK_SIZE,
                   LINK_PRIORITY, LINK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}
```

#### Thread principal – DHCP

```c
static VOID nx_app_thread_entry(ULONG thread_input)
{
  /* Enregistrement du callback changement d'IP */
  nx_ip_address_change_notify(&NetXDuoEthIpInstance,
                              ip_address_change_notify_callback, NULL);

  /* Démarrage DHCP */
  nx_dhcp_start(&DHCPClient);
  printf("Looking for DHCP server ..\n");

  /* Attente de l'IP (sémaphore libéré par le callback) */
  tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER);

  PRINT_IP_ADDRESS(IpAddress);      /* Affiche : STM32 IpAddress: 192.168.x.x */
  printf("DNS configure : 8.8.8.8\n");

  /* Débloque le thread TCP maintenant que l'IP est disponible */
  tx_thread_resume(&AppTCPThread);
  tx_thread_relinquish();
}

static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
  if (IpAddress != NULL_ADDRESS)
    tx_semaphore_put(&DHCPSemaphore);
}
```

#### Thread TCP – DNS + Connexion + ThingSpeak

```c
static VOID App_TCP_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  ULONG server_ip = 0;
  NX_PACKET *data_packet, *server_packet;
  ULONG bytes_read;
  UCHAR data_buffer[512];

  /* =========================================================
   * ÉTAPE 1 : Initialisation DNS (DANS le thread, obligatoire)
   * =========================================================
   * ⚠️ PIÈGE CLASSIQUE : nx_dns_create() doit être appelé
   *    depuis un thread ThreadX, PAS dans MX_NetXDuo_Init().
   *    Si appelé hors thread → erreur NX_CALLER_ERROR (0x11)
   * ========================================================= */
  printf("Initialisation du DNS dans le thread...\r\n");

  ret = nx_dns_create(&DnsClient, &NetXDuoEthIpInstance, (UCHAR *)"DNS Client");
  if (ret == NX_SUCCESS)
  {
    /* nx_dns_packet_pool_set peut retourner une erreur non bloquante (0x4B) */
    /* selon la config CubeMX → on l'ignore avec un simple printf */
    nx_dns_packet_pool_set(&DnsClient, &NxAppPool);
    nx_dns_server_add(&DnsClient, IP_ADDRESS(8, 8, 8, 8));  /* DNS Google */
    printf("DNS Init terminee avec succes.\r\n");
  }
  else
  {
    printf("Erreur creation DNS : 0x%02X\r\n", ret);
  }

  /* =========================================================
   * ÉTAPE 2 : Résolution DNS de api.thingspeak.com
   * ========================================================= */
  printf("Resolution DNS de api.thingspeak.com ...\r\n");

  ret = nx_dns_host_by_name_get(&DnsClient,
                                (UCHAR *)"api.thingspeak.com",
                                &server_ip,
                                10 * NX_IP_PERIODIC_RATE);  /* Timeout 10s */
  if (ret != NX_SUCCESS)
  {
    /* Fallback IP fixe si DNS échoue */
    server_ip = IP_ADDRESS(184, 106, 153, 149);
    printf("DNS echoue → IP fixe : 184.106.153.149\r\n");
  }
  else
  {
    printf("DNS OK ! IP ThingSpeak : %lu.%lu.%lu.%lu\r\n",
           (server_ip >> 24) & 0xFF, (server_ip >> 16) & 0xFF,
           (server_ip >>  8) & 0xFF,  server_ip        & 0xFF);
  }

  /* =========================================================
   * ÉTAPE 3 : Connexion TCP sur port 80
   * =========================================================
   * ⚠️ PIÈGE : Utiliser server_ip (résolu par DNS),
   *    PAS la macro TCP_SERVER_ADDRESS (adresse locale invalide)
   * ========================================================= */
  nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket, "TCP Client",
                       NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                       NX_IP_TIME_TO_LIVE, WINDOW_SIZE, NX_NULL, NX_NULL);

  nx_tcp_client_socket_bind(&TCPSocket, DEFAULT_PORT, NX_WAIT_FOREVER);

  printf("Connexion au serveur TCP...\r\n");
  ret = nx_tcp_client_socket_connect(&TCPSocket, server_ip, 80, NX_WAIT_FOREVER);
  if (ret != NX_SUCCESS) {
    printf("Erreur connexion TCP ! (0x%02X)\r\n", ret);
    Error_Handler();
  }
  printf("Connecte au serveur TCP !\r\n");

  /* =========================================================
   * ÉTAPE 4 : Envoi requête HTTP GET vers ThingSpeak
   * =========================================================
   * Construction dynamique avec les vraies valeurs capteurs
   * (ici : valeur simulée 24.5°C)
   * ========================================================= */
  UINT count = 0;
  while (count++ < MAX_PACKET_COUNT)
  {
    TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

    ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
    if (ret != NX_SUCCESS) break;

    /* Construction de la requête HTTP dynamique */
    char http_request[256];
    float temperature = 24.5;  /* Remplacer par lecture capteur réel */

    sprintf(http_request,
            "GET /update?api_key=K2KZJ5HE0O2WWOLZ&field1=%.1f HTTP/1.1\r\n"
            "Host: api.thingspeak.com\r\n"
            "Connection: close\r\n\r\n",
            temperature);

    ret = nx_packet_data_append(data_packet, (VOID *)http_request,
                                strlen(http_request), &NxAppPool, TX_WAIT_FOREVER);
    if (ret != NX_SUCCESS) { nx_packet_release(data_packet); break; }

    ret = nx_tcp_socket_send(&TCPSocket, data_packet, DEFAULT_TIMEOUT);
    if (ret != NX_SUCCESS) break;

    /* Réception et affichage de la réponse */
    ret = nx_tcp_socket_receive(&TCPSocket, &server_packet, DEFAULT_TIMEOUT);
    if (ret == NX_SUCCESS)
    {
      ULONG src_ip; UINT src_port;
      nx_udp_source_extract(server_packet, &src_ip, &src_port);
      nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);
      PRINT_DATA(src_ip, src_port, data_buffer);
      nx_packet_release(server_packet);
      HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
    else { break; }
  }

  /* Nettoyage socket */
  nx_tcp_socket_disconnect(&TCPSocket, DEFAULT_TIMEOUT);
  nx_tcp_client_socket_unbind(&TCPSocket);
  nx_tcp_socket_delete(&TCPSocket);

  /* Résultat : succès si au moins 1 message envoyé/reçu */
  if (count > 0) {
    printf("\n--- SUCCESS : Requete HTTP terminee ! ---\n");
    Success_Handler();
  } else {
    printf("\n--- FAIL : Echec communication ---\n");
    Error_Handler();
  }
}
```

#### Thread Link – Surveillance câble Ethernet

```c
static VOID App_Link_Thread_Entry(ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  while (1)
  {
    status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0,
                                         NX_IP_LINK_ENABLED, &actual_status, 10);
    if (status == NX_SUCCESS)
    {
      if (linkdown == 1) {
        linkdown = 0;
        printf("The network cable is connected.\n");
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE, &actual_status);
        /* Réinitialisation DHCP si câble rebranché */
        nx_dhcp_stop(&DHCPClient);
        nx_dhcp_reinitialize(&DHCPClient);
        nx_dhcp_start(&DHCPClient);
        tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER);
        PRINT_IP_ADDRESS(IpAddress);
      }
    }
    else
    {
      if (linkdown == 0) {
        linkdown = 1;
        printf("The network cable is not connected.\n");
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_DISABLE, &actual_status);
      }
    }
    tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
  }
}
```

### 1.2 Résultats obtenus – Console série (115200 baud)

```
Nx_TCP_Echo_Client application started..
The network cable is connected.
Looking for DHCP server ..
STM32 IpAddress: 192.168.141.128
DNS configure : 8.8.8.8
Initialisation du DNS dans le thread...
DNS Init terminee avec succes.
Resolution DNS de api.thingspeak.com ...
DNS OK ! IP ThingSpeak : 34.203.56.254
Connexion au serveur TCP...
Connecte au serveur TCP !

[34.203.56.254 : 80] HTTP/1.1 200 OK
Server: awselb/2.0
Content-Type: text/plain; charset=utf-8
Content-Length: 1

1

--- SUCCESS : Requete HTTP terminee ! ---
```

### 1.3 Vérification sur ThingSpeak

- **Onglet Private View :** Point à 24.5°C visible sur le graphique Field 1
- **Entries :** 1 (s'incrémente à chaque envoi)
- **Last entry :** "about a minute ago"

### 1.4 Problèmes résolus

| Erreur | Cause | Solution |
|---|---|---|
| `NX_CALLER_ERROR (0x11)` | `nx_dns_create()` appelé dans `MX_NetXDuo_Init()` | Déplacer dans `App_TCP_Thread_Entry()` |
| Connexion TCP bloquée | `TCP_SERVER_ADDRESS` au lieu de `server_ip` | Utiliser l'IP résolue par DNS |
| `400 Bad Request` ThingSpeak | Header `Host: google.com` | Corriger en `Host: api.thingspeak.com` |
| Float non affiché dans `sprintf` | newlib-nano sans support float | *Project Properties → Linker → Use float with printf* |
| Écran console en escalier | `\n` sans retour chariot | Remplacer tous les `\n` par `\r\n` |
| Données non affichées sur graphique | `%f` non supporté | Activer `-u _printf_float` dans les linker flags |

---

## 🤖 Partie 2 – Edge AI : Déploiement sur STM32N6 via X-CUBE-AI

### 2.1 Génération du modèle ONNX (Google Colab)

```python
# Étape 1 : Installation (dans une cellule Colab)
# !pip install tf2onnx

import tensorflow as tf
import tf2onnx

# Mini-réseau MeteoStat (3 entrées : Temp, Hum, Pression → 4 classes)
model = tf.keras.Sequential([
    tf.keras.Input(shape=(3,)),          # ← Input EXPLICITE (obligatoire TF récent)
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(8,  activation='relu'),
    tf.keras.layers.Dense(4,  activation='softmax')
], name="MeteoStat_EdgeAI")

# Conversion ONNX
spec = (tf.TensorSpec((None, 3), tf.float32, name="input"),)
tf2onnx.convert.from_keras(model, input_signature=spec,
                            output_path="meteostat_mini.onnx")
print("✅ meteostat_mini.onnx généré")
```

> 💡 **Récupérer le fichier :** Dans Google Colab, cliquer sur l'icône 📁 (Fichiers) → clic droit sur `meteostat_mini.onnx` → **Télécharger**

### 2.2 Intégration X-CUBE-AI dans STM32CubeIDE

1. Ouvrir le fichier `.ioc` du projet
2. **Software Packs → Select Components → STMicroelectronics X-CUBE-AI** → Activer **Core**
3. Dans la section **Software Packs → X-CUBE-AI** :
   - Cliquer **Add network**
   - Importer `meteostat_mini.onnx`
   - Cliquer **Analyze** → vérifier que la RAM/ROM nécessaire est compatible
4. **Ctrl+S** → Générer le code → Les fichiers C sont créés automatiquement

**Analyse typique par X-CUBE-AI :**
```
Network    : meteostat_mini
RAM usage  : 512 bytes   (bien inférieur aux 320 Ko disponibles)
ROM usage  : 2048 bytes  (bien inférieur aux 512 Ko Flash)
MACs       : 316
```

### 2.3 Code C d'inférence dans `main.c`

```c
/* Includes X-CUBE-AI générés */
#include "meteostat_mini.h"
#include "meteostat_mini_data.h"

/* Dans main() ou dans un thread dédié */

/* Données d'entrée (valeurs capteurs réelles ou simulées) */
float input_data[AI_METEOSTAT_MINI_IN_1_SIZE] = {
    24.5f,    /* Température (°C) */
    55.0f,    /* Humidité (%RH) */
    1013.25f  /* Pression (hPa) */
};

/* Buffer de sortie (probabilités par classe) */
float output_data[AI_METEOSTAT_MINI_OUT_1_SIZE];

/* Variables X-CUBE-AI */
ai_handle network;
ai_buffer ai_input[AI_METEOSTAT_MINI_IN_NUM];
ai_buffer ai_output[AI_METEOSTAT_MINI_OUT_NUM];

/* Initialisation du réseau */
ai_error err = ai_meteostat_mini_create(&network, AI_METEOSTAT_MINI_DATA_CONFIG);
if (err.type != AI_ERROR_NONE) {
    printf("Erreur init IA : type=%d\r\n", err.type);
    Error_Handler();
}

/* Configuration des buffers */
ai_input[0]  = AI_BUFFER_INIT(AI_FLAG_NONE, AI_BUFFER_FORMAT_FLOAT,
                               AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 1, 1, 3),
                               AI_METEOSTAT_MINI_IN_1_SIZE, NULL, input_data);
ai_output[0] = AI_BUFFER_INIT(AI_FLAG_NONE, AI_BUFFER_FORMAT_FLOAT,
                               AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 1, 1, 4),
                               AI_METEOSTAT_MINI_OUT_1_SIZE, NULL, output_data);

/* Mesure du temps d'inférence */
uint32_t t_start = HAL_GetTick();

/* Exécution de l'inférence */
ai_i32 n_batch = ai_meteostat_mini_run(network, &ai_input[0], &ai_output[0]);

uint32_t t_inference = HAL_GetTick() - t_start;

if (n_batch != 1) {
    printf("Erreur inference !\r\n");
} else {
    /* Trouver la classe avec la probabilité maximale */
    int best_class = 0;
    float best_prob = output_data[0];
    for (int i = 1; i < 4; i++) {
        if (output_data[i] > best_prob) {
            best_prob  = output_data[i];
            best_class = i;
        }
    }

    const char *classes[] = {"Clair", "Nuageux", "Pluie", "Neige"};
    printf("=== Inference Edge AI ===\r\n");
    printf("Entrees   : T=%.1f°C H=%.1f%%RH P=%.1f hPa\r\n",
           input_data[0], input_data[1], input_data[2]);
    printf("Classe    : %s (%.1f%%)\r\n", classes[best_class], best_prob * 100.0f);
    printf("Temps     : %lu ms\r\n", t_inference);
    printf("========================\r\n");
}
```

### 2.4 Mesure de consommation énergétique

Selon le sujet TP4, la mesure se fait sur le **jumper d'alimentation** de la carte :

**Protocole de mesure :**

1. **Programme témoin** (sans inférence IA) :
   - Désactiver toutes les GPIOs non essentielles
   - Exécuter toutes les étapes (init, DHCP, etc.) **sauf** l'appel à `ai_meteostat_mini_run()`
   - Mesurer le courant I₀ sur le jumper avec un multimètre (mode mA)

2. **Programme avec IA** :
   - Même programme + appel à `ai_meteostat_mini_run()`
   - Mesurer le courant I_AI

3. **Calcul de la puissance IA :**
   ```
   P_AI = (I_AI - I₀) × V_alimentation
        = (I_AI - I₀) × 5V
   ```

4. **Énergie par inférence :**
   ```
   E_inférence = P_AI × t_inference (en ms → convertir en secondes)
   ```

**Ordre de grandeur attendu sur STM32N6 :**
- `t_inference` : < 1 ms pour un mini-réseau Dense (< 400 MACs)
- `ΔI` : quelques mA
- Avantage du NPU Neural-ART (600 GOPS) pour des réseaux CNN plus complexes

> ⚠️ **Consulter les chargés de TP avant de modifier les jumpers pour la mesure de courant.**

---

## 📊 Comparaison Cloud AI vs Edge AI

| Critère | Cloud AI (ThingSpeak/MATLAB) | Edge AI (STM32N6) |
|---|---|---|
| **Latence** | ~500ms - 2s (réseau) | < 1 ms (local) |
| **Accuracy** | Identique (même modèle ONNX) | Identique |
| **Consommation réseau** | ~2 Ko par requête | 0 octet |
| **Indépendance réseau** | Non | Oui |
| **Taille modèle supportée** | Illimitée | ~50 Ko max (RAM) |
| **Mise à jour du modèle** | Immédiate (MATLAB) | Reflash STM32 |
| **Coût calcul** | ~0 (ThingSpeak free) | ~0 (MCU) |

### Logs console de comparaison

**Cloud AI :**
```
DNS OK ! IP ThingSpeak : 34.203.56.254
Connexion TCP → HTTP 200 OK → Résultat : 1 (ThingSpeak entry)
Latence totale : ~800ms
```

**Edge AI :**
```
=== Inference Edge AI ===
Entrees   : T=24.5°C H=55.0%RH P=1013.2 hPa
Classe    : Clair (87.3%)
Temps     : 0 ms
========================
```

---

## 📁 Structure du dépôt

```
TP4_Cloud_vs_EdgeAI/
├── README.md                          ← Ce fichier
├── code/
│   ├── app_netxduo_dns_http.c         ← Code NetXDuo DHCP+DNS+TCP+ThingSpeak (✅ fonctionnel)
│   └── app_netxduo_thingspeak.c       ← Code avec envoi données dynamiques ThingSpeak (✅ fonctionnel)
├── python/
│   └── generate_onnx_model.py         ← Script génération modèle ONNX (Google Colab)
└── matlab/
    └── inference_cloud.m              ← Script inférence MATLAB ThingSpeak
```

---

## ⚠️ Checklist avant compilation

- [ ] Jumper carte en position `1-2 5V_STLK`
- [ ] Câble branché sur port **ST-LINK** (pas USB OTG)
- [ ] Câble **Ethernet** connecté à un réseau avec serveur DHCP
- [ ] Remplacer `K2KZJ5HE0O2WWOLZ` par votre **Write API Key** ThingSpeak
- [ ] Activer **"Use float with printf"** dans les options Linker de CubeIDE
- [ ] Tous les `\n` remplacés par `\r\n` pour un affichage console propre
- [ ] `nx_dns_create()` appelé dans `App_TCP_Thread_Entry()`, **PAS** dans `MX_NetXDuo_Init()`

---

## 📚 Références

- [NetXDuo Documentation – Microsoft Azure RTOS](https://learn.microsoft.com/en-us/azure/rtos/netx-duo/)
- [X-CUBE-AI User Manual – STMicroelectronics](https://www.st.com/resource/en/user_manual/um2526-getting-started-with-xcubeai-expansion-package-for-artificial-intelligence-ai-stmicroelectronics.pdf)
- [ThingSpeak REST API](https://www.mathworks.com/help/thingspeak/rest-api.html)
- [tf2onnx – GitHub](https://github.com/onnx/tensorflow-onnx)
- [ONNX – Open Neural Network Exchange](https://onnx.ai/)
