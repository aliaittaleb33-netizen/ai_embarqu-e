# TP2 – Interface Capteur & STM32
**Module :** ETRS606 – IA Embarquée | **Université Savoie Mont Blanc**  
**Carte principale :** NUCLEO-N657X0 (STM32N657, ARM Cortex-M33, 160 MHz, 320 Ko RAM, 512 Ko Flash)  
**Carte capteurs :** X-NUCLEO-IKS01A3

---

## 📋 Objectifs du TP

- Programmer la carte NUCLEO-N657X0 via STM32CubeIDE
- Maîtriser la communication I²C avec des capteurs MEMS
- Intégrer les drivers officiels de STMicroelectronics
- Implémenter une interface réseau Ethernet avec FreeRTOS et LWIP

---

## 🛠️ Matériel

| Composant | Description |
|---|---|
| **NUCLEO-N657X0** | MCU STM32N657, ARM Cortex-M33, 160 MHz |
| **X-NUCLEO-IKS01A3** | Carte fille capteurs MEMS (I²C) |
| **Câble USB-C data** | Pour la programmation via ST-LINK |
| **Câble Ethernet RJ45** | Pour la connectivité réseau |

### Configuration matérielle obligatoire

> ⚠️ **IMPORTANT :** Avant de brancher la carte, vérifier impérativement :

| Élément | Réglage correct |
|---|---|
| **Jumper d'alimentation** | Position **1-2 `5V_STLK`** (NE PAS mettre sur 3-4 ou 5-6) |
| **Port USB** | Brancher sur le port **ST-LINK** uniquement (pas le port USB User/OTG) |
| **Câble** | Utiliser un câble **data + charge** (pas un câble charge seule) |

✅ **Signe de succès :** La LED **COM** passe au **vert fixe** quand la carte est correctement reconnue.

---

## 🔴🟢🔵 Partie 1 – LED Blink sur NUCLEO-STM32N657

### Objectif
Prendre en main STM32CubeIDE en programmant les 3 LEDs de la carte.

### Résultats implémentés

**a) Allumage des 3 LEDs simultanément**
```c
HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_SET);
HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  GPIO_PIN_SET);
```

**b) Chenillard avec temporisation 3 secondes**
```c
// Dans la boucle principale main()
printf("<debut de chenillard>\r\n");

printf("<ROUGE>\r\n");
HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_SET);
HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  GPIO_PIN_RESET);
HAL_Delay(3000);

printf("<VERT>\r\n");
HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  GPIO_PIN_RESET);
HAL_Delay(3000);

printf("<BLEU>\r\n");
HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  GPIO_PIN_SET);
HAL_Delay(3000);
```

**c) Logs console (UART/printf)**
```
<debut d application>
<debut de chenillard>
<ROUGE>
<VERT>
<BLEU>
<debut de chenillard>
...
```

---

## 🌡️ Partie 2 – Interface Capteurs I²C

### Capteurs disponibles sur X-NUCLEO-IKS01A3

| Capteur | Type | Adresse I²C |
|---|---|---|
| **LSM6DSO** | Accéléromètre 3 axes + Gyroscope 3 axes | 0x6A |
| **LIS2MDL** | Magnétomètre 3 axes | 0x1E |
| **LIS2DW12** | Accéléromètre 3 axes | 0x19 |
| **HTS221** | Humidité + Température | 0x5F |
| **LPS22HH** | Pression atmosphérique | 0x5D |
| **STTS751** | Température | 0x39 |

### Étape 1 – Récupération des drivers STMems

```bash
# Cloner le dépôt des drivers officiels ST
git clone https://github.com/STMicroelectronics/STMems_Standard_C_drivers
```

Pour chaque capteur, copier les fichiers suivants dans `Drivers/STMems/` du projet :
- `hts221_reg.c` / `hts221_reg.h`
- `lps22hh_reg.c` / `lps22hh_reg.h`
- `lsm6dso_reg.c` / `lsm6dso_reg.h`

Ajouter `Drivers/STMems/` au **Include Path** du projet dans CubeIDE.

### Étape 2 – Configuration I²C (CubeMX)

Dans le fichier `.ioc` → **Connectivity → I2C2** :
- Mode : `I2C`
- Speed : `Standard Mode (100 kHz)`
- Mapping des pins :
  - `PA12` → `I2C2_SCL`
  - `PA11` → `I2C2_SDA`

> ⚠️ **Adresses 7-bit vs 8-bit :** Les drivers STMems utilisent des adresses 7-bit.  
> La HAL STM32 attend l'adresse **8-bit** → multiplier par 2 : `addr_8bit = addr_7bit << 1`  
> Exemple HTS221 : adresse doc = `0x5F` → HAL = `0x5F << 1 = 0xBE`

### Étape 3 – Fonctions bas-niveau platform_read / platform_write

Les drivers `xxx_reg.c` appellent ces fonctions qui doivent être implémentées manuellement :

```c
/**
 * @brief  Lecture I2C - plateforme bas niveau
 * @param  handle : pointeur vers le handle I2C (ex: &hi2c2)
 * @param  reg    : adresse du registre à lire
 * @param  buf    : buffer de réception
 * @param  len    : nombre d'octets à lire
 * @retval 0 si succès, -1 si erreur
 */
int32_t platform_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *) handle;
    uint16_t dev_addr = HTS221_I2C_ADDRESS << 1;  /* Adresse 8-bit pour HAL */

    if (HAL_I2C_Mem_Read(hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                         buf, len, 1000) == HAL_OK)
        return 0;
    else
        return -1;
}

/**
 * @brief  Écriture I2C - plateforme bas niveau
 */
int32_t platform_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *) handle;
    uint16_t dev_addr = HTS221_I2C_ADDRESS << 1;

    if (HAL_I2C_Mem_Write(hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                          (uint8_t *)buf, len, 1000) == HAL_OK)
        return 0;
    else
        return -1;
}
```

### Étape 4 – Scan I²C (test de présence des capteurs)

```c
/**
 * @brief  Scan le bus I2C et affiche les adresses des périphériques détectés
 */
void I2C_Scan(I2C_HandleTypeDef *hi2c)
{
    printf("=== Scan I2C ===\r\n");
    for (uint8_t addr = 1; addr < 128; addr++)
    {
        if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 2, 10) == HAL_OK)
        {
            printf("  Peripherique detecte : 0x%02X\r\n", addr);
        }
    }
    printf("=== Fin du scan ===\r\n");
}
```

**Résultats attendus du scan :**
```
=== Scan I2C ===
  Peripherique detecte : 0x1E  (LIS2MDL)
  Peripherique detecte : 0x19  (LIS2DW12)
  Peripherique detecte : 0x39  (STTS751)
  Peripherique detecte : 0x5D  (LPS22HH)
  Peripherique detecte : 0x5F  (HTS221)
  Peripherique detecte : 0x6A  (LSM6DSO)
=== Fin du scan ===
```

### Étape 5 – Lecture des capteurs

#### HTS221 – Température et Humidité

```c
#include "hts221_reg.h"

stmdev_ctx_t hts221_ctx;
hts221_ctx.write_reg = platform_write;
hts221_ctx.read_reg  = platform_read;
hts221_ctx.handle    = &hi2c2;

/* Vérification WHO_AM_I */
uint8_t who_am_i;
hts221_device_id_get(&hts221_ctx, &who_am_i);
printf("HTS221 WHO_AM_I: 0x%02X (attendu: 0xBC)\r\n", who_am_i);

/* Initialisation et activation */
hts221_power_on(&hts221_ctx);
hts221_data_rate_set(&hts221_ctx, HTS221_ODR_1Hz);

/* Lecture température */
int16_t raw_temp;
float temperature;
hts221_temperature_raw_get(&hts221_ctx, &raw_temp);
/* Appliquer la calibration (voir datasheet) */
/* temperature = raw_temp * coeff_T */
printf("Temperature : %.2f degC\r\n", temperature);

/* Lecture humidité */
int16_t raw_hum;
float humidity;
hts221_humidity_raw_get(&hts221_ctx, &raw_hum);
printf("Humidite    : %.2f %%RH\r\n", humidity);
```

#### LPS22HH – Pression atmosphérique

```c
#include "lps22hh_reg.h"

stmdev_ctx_t lps22hh_ctx;
lps22hh_ctx.write_reg = platform_write;
lps22hh_ctx.read_reg  = platform_read;
lps22hh_ctx.handle    = &hi2c2;

/* Activation en mode continu */
lps22hh_data_rate_set(&lps22hh_ctx, LPS22HH_10_Hz);

/* Lecture pression */
uint32_t raw_press;
float pressure;
lps22hh_pressure_raw_get(&lps22hh_ctx, &raw_press);
pressure = lps22hh_from_lsb_to_hpa(raw_press);
printf("Pression    : %.2f hPa\r\n", pressure);
```

#### LSM6DSO – Accéléromètre + Gyroscope

```c
#include "lsm6dso_reg.h"

stmdev_ctx_t lsm6dso_ctx;
lsm6dso_ctx.write_reg = platform_write;
lsm6dso_ctx.read_reg  = platform_read;
lsm6dso_ctx.handle    = &hi2c2;

/* Configuration */
lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_12Hz5);
lsm6dso_xl_full_scale_set(&lsm6dso_ctx, LSM6DSO_2g);

/* Lecture accélération */
int16_t raw_acc[3];
float acc_mg[3];
lsm6dso_acceleration_raw_get(&lsm6dso_ctx, raw_acc);
acc_mg[0] = lsm6dso_from_fs2_to_mg(raw_acc[0]);
acc_mg[1] = lsm6dso_from_fs2_to_mg(raw_acc[1]);
acc_mg[2] = lsm6dso_from_fs2_to_mg(raw_acc[2]);
printf("Acceleration: X=%.1f Y=%.1f Z=%.1f mg\r\n", acc_mg[0], acc_mg[1], acc_mg[2]);
```

### Étape 6 – Affichage console formaté + LEDs

```c
/* Dans la boucle principale */
while (1)
{
    /* LED ROUGE : lecture en cours */
    HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    printf("<Lecture Interface Capteurs>\r\n");

    /* ... Lecture des capteurs ... */

    printf("------- Mesures -------\r\n");
    printf("Temperature : %.2f degC\r\n",   temperature);
    printf("Humidite    : %.2f %%RH\r\n",   humidity);
    printf("Pression    : %.2f hPa\r\n",    pressure);
    printf("Acc X/Y/Z   : %.1f / %.1f / %.1f mg\r\n", acc_mg[0], acc_mg[1], acc_mg[2]);
    printf("-----------------------\r\n");

    /* LED VERTE : disponible, en attente */
    HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);

    HAL_Delay(1000);
}
```

### Étape 7 – Activation du support float pour printf

Dans STM32CubeIDE :  
*Project Properties → C/C++ Build → Settings → MCU GCC Linker → Miscellaneous*  
→ Cocher **"Use float with printf from newlib-nano"**

Ou ajouter dans les linker flags : `-u _printf_float`

---

## 🌐 Partie 3 – Réseau Ethernet (FreeRTOS + LWIP)

### Architecture choisie
- **OS Temps Réel :** Azure RTOS / ThreadX (via NetXDuo)
- **Stack IP :** NetXDuo (inclus dans STM32CubeIDE)
- **Protocoles :** DHCP + ARP + ICMP + TCP + UDP

> Note : Le sujet mentionne FreeRTOS + LWIP. Sur la NUCLEO-N657X0, NetXDuo/ThreadX est la stack recommandée par STMicroelectronics pour ce MCU.

### Configuration dans CubeMX (.ioc)

1. **Middlewares → NetXDuo** : Activer
2. **Peripherals → ETH** : Activer l'interface Ethernet
3. Augmenter `NX_APP_MEM_POOL_SIZE` si nécessaire (ex: 153600)

### Résultats obtenus

```
Nx_TCP_Echo_Client application started..
The network cable is connected.
Looking for DHCP server ..
STM32 IpAddress: 192.168.141.128
```

### Vérification par ping

Une fois l'IP obtenue, depuis un PC sur le même réseau :
```bash
ping 192.168.141.128
```
```
Réponse de 192.168.141.128 : octets=32 durée<1ms TTL=128
```

### Correspondance LED / État réseau

| LED | État carte |
|---|---|
| 🔴 ROUGE | Lecture des capteurs I2C |
| 🟢 VERTE | En attente, disponible |
| 🔵 BLEUE | Communication réseau active |

---

## ⚠️ Problèmes rencontrés et solutions

| Problème | Cause | Solution |
|---|---|---|
| Carte non reconnue par PC | Câble charge seule ou jumper mal placé | Jumper sur `1-2 5V_STLK` + câble data |
| Capteur absent au scan I2C | Adresse 7-bit non décalée | Utiliser `addr << 1` pour la HAL |
| `printf` n'affiche pas les float | newlib-nano sans support float | Activer `-u _printf_float` dans le Linker |
| WHO_AM_I retourne 0xFF | Bus I2C non initialisé ou mauvais pull-up | Vérifier MX_I2C2_Init() et câblage |

---

## 📁 Structure du projet

```
TP2_STM32N657/
├── Core/
│   ├── Src/
│   │   └── main.c          ← Code principal (LED + lecture capteurs)
│   └── Inc/
├── Drivers/
│   ├── STM32N6xx_HAL_Driver/
│   └── STMems/             ← Drivers capteurs (à ajouter manuellement)
│       ├── hts221_reg.c / .h
│       ├── lps22hh_reg.c / .h
│       └── lsm6dso_reg.c / .h
└── Middlewares/
    └── NetXDuo/            ← Stack réseau
```

---

## 📚 Références

- [STMems Standard C Drivers – GitHub](https://github.com/STMicroelectronics/STMems_Standard_C_drivers)
- [STM32CubeIDE Wiki – LED Blink Tutorial](https://wiki.st.com)
- [NetXDuo Documentation – ST](https://www.st.com)
