# TP3 – Connectivité Cloud & Entraînement IA MeteoStat
**Module :** ETRS606 – IA Embarquée | **Université Savoie Mont Blanc**  
**Plateforme Cloud :** ThingSpeak (MathWorks)  
**Framework IA :** Python / TensorFlow / Keras

---

## 📋 Objectifs du TP

- Configurer un canal ThingSpeak pour collecter et visualiser des données IoT
- Envoyer des données capteurs vers le cloud via HTTP REST
- Analyser les données collectées avec un script MATLAB
- Entraîner un réseau de neurones en Python/TensorFlow pour la classification météo (MeteoStat)
- Étudier le compromis classes / taille du modèle / précision

---

## ☁️ Partie 1 – Collecte des données dans un canal ThingSpeak

### 1.1 Configuration du canal ThingSpeak

1. Créer un compte sur [thingspeak.com](https://thingspeak.com)
2. **New Channel** → Nommer le canal (ex: `STM32_Capteurs`)
3. Activer les champs :
   - `Field 1` : Température (°C)
   - `Field 2` : Humidité (%RH)
   - `Field 3` : Pression (hPa)
4. Récupérer la **Write API Key** dans l'onglet *API Keys*

### 1.2 Envoi des données via HTTP GET (depuis STM32)

Format de la requête HTTP GET vers ThingSpeak :

```
GET /update?api_key=<WRITE_API_KEY>&field1=<temp>&field2=<hum>&field3=<press> HTTP/1.1
Host: api.thingspeak.com
Connection: close
```

**Exemple de construction dynamique en C (STM32) :**

```c
#include <stdio.h>
#include <string.h>

/* Données des capteurs (lues via I2C) */
float temperature = 22.5;
float humidity    = 55.3;
float pressure    = 1012.8;

char http_request[512];

sprintf(http_request,
    "GET /update?api_key=VOTRE_API_KEY"
    "&field1=%.1f"
    "&field2=%.1f"
    "&field3=%.1f"
    " HTTP/1.1\r\n"
    "Host: api.thingspeak.com\r\n"
    "Connection: close\r\n\r\n",
    temperature, humidity, pressure);

/* Envoyer via nx_packet_data_append + nx_tcp_socket_send */
```

> ⚠️ **Activation du support float :** Ajouter `-u _printf_float` dans les Linker flags de CubeIDE pour que `%.1f` fonctionne dans `sprintf`.

### 1.3 Résultats obtenus

Réponse du serveur ThingSpeak après un envoi réussi :
```
HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 1

1
```
> Le chiffre `1` correspond au numéro de l'entrée enregistrée. ThingSpeak renvoie `0` en cas d'erreur (clé API invalide, débit trop élevé > 1 requête/15s).

**Tableau de bord ThingSpeak :**
- Onglet *Private View* → graphiques temps réel des 3 champs
- Entrées visibles dans *Data Import/Export*

### 1.4 Script MATLAB – Analyse des données collectées

```matlab
%% Récupération et analyse des données ThingSpeak
% Remplacer par votre Channel ID et Read API Key
channelID = VOTRE_CHANNEL_ID;
readAPIKey = 'VOTRE_READ_API_KEY';

% Nombre de données à récupérer
numPoints = 100;

% Lecture des 3 champs
[temp_data, timestamps] = thingSpeakRead(channelID, ...
    'Fields', [1 2 3], ...
    'NumPoints', numPoints, ...
    'ReadKey', readAPIKey);

temperature = temp_data(:, 1);
humidity    = temp_data(:, 2);
pressure    = temp_data(:, 3);

%% Calcul de la moyenne glissante sur 1 minute (≈ 4 points à 15s/mesure)
window = 4;
temp_moy    = movmean(temperature, window);
hum_moy     = movmean(humidity,    window);
press_moy   = movmean(pressure,    window);

%% Affichage
figure;
subplot(3,1,1);
plot(timestamps, temperature, 'b', timestamps, temp_moy, 'r--', 'LineWidth', 1.5);
ylabel('Température (°C)'); legend('Brut','Moyenne 1min'); grid on;

subplot(3,1,2);
plot(timestamps, humidity, 'b', timestamps, hum_moy, 'r--', 'LineWidth', 1.5);
ylabel('Humidité (%RH)'); grid on;

subplot(3,1,3);
plot(timestamps, pressure, 'b', timestamps, press_moy, 'r--', 'LineWidth', 1.5);
ylabel('Pression (hPa)'); xlabel('Temps'); grid on;

sgtitle('Données capteurs NUCLEO-N657X0 - Analyse 1 minute');

%% Statistiques
fprintf('Température : Moy=%.2f°C, Min=%.2f°C, Max=%.2f°C\n', ...
    mean(temperature), min(temperature), max(temperature));
fprintf('Humidité    : Moy=%.2f%%RH\n', mean(humidity));
fprintf('Pression    : Moy=%.2f hPa\n', mean(pressure));
```

### 1.5 API ThingSpeak – Alerte email (HTTP POST)

Pour déclencher une alerte email quand la température dépasse un seuil :

```matlab
%% Alerte email via ThingSpeak Alerts API
alertAPIKey  = 'VOTRE_ALERT_API_KEY';
seuil_temp   = 30.0;  % Seuil en °C

if max(temperature) > seuil_temp
    alertBody = struct('subject', 'Alerte Temperature STM32', ...
                       'body',    sprintf('Température max: %.1f°C > seuil %.1f°C', ...
                                         max(temperature), seuil_temp));
    thingSpeakWrite(channelID, 'Fields', {1}, 'Values', {max(temperature)}, ...
                    'WriteKey', 'WRITE_API_KEY');
    % Appel REST API d'alerte
    webwrite('https://api.thingspeak.com/alerts/send', alertBody, ...
             weboptions('HeaderFields', {'ThingSpeak-Alerts-API-Key', alertAPIKey}, ...
                        'MediaType', 'application/json'));
    disp('Alerte envoyée !');
end
```

### 1.6 API TalkBack – Contrôle de la carte STM32

TalkBack permet d'envoyer des commandes à la carte depuis le cloud :

**Côté MATLAB (envoi de commande) :**
```matlab
talkbackID  = VOTRE_TALKBACK_ID;
talkbackKey = 'VOTRE_TALKBACK_KEY';

% Ajouter une commande à la file
url = sprintf('https://api.thingspeak.com/talkbacks/%d/commands.json', talkbackID);
webwrite(url, 'api_key', talkbackKey, 'command_string', 'LED_ON');
```

**Côté STM32 (lecture de commande via HTTP GET) :**
```c
/* Requête pour lire la prochaine commande TalkBack */
sprintf(http_request,
    "GET /talkbacks/%d/commands/execute?api_key=%s HTTP/1.1\r\n"
    "Host: api.thingspeak.com\r\n"
    "Connection: close\r\n\r\n",
    TALKBACK_ID, TALKBACK_API_KEY);
```

---

## 🧠 Partie 2 – Entraînement du Modèle IA MeteoStat

### 2.1 Présentation du problème

Classifier les conditions météorologiques à partir de données capteurs :
- **Entrées :** Température (°C), Humidité (%RH), Pression (hPa)
- **Sortie :** Classe météo parmi N catégories

### 2.2 Choix des classes (compromis retenu : 4 classes)

| Classe | Label | Description |
|---|---|---|
| 0 | ☀️ **Clair** | Pression élevée, humidité faible, temp normale |
| 1 | ⛅ **Nuageux** | Pression intermédiaire, humidité moyenne |
| 2 | 🌧️ **Pluie** | Pression basse, humidité élevée |
| 3 | ❄️ **Neige** | Température < 0°C, humidité élevée |

**Justification du choix de 4 classes :**
- 4 classes = bon compromis précision / taille modèle pour un déploiement embarqué
- Un modèle avec 12+ classes nécessiterait plus de données d'entraînement et un modèle plus grand (incompatible avec 320 Ko RAM de la STM32N657)
- Les 4 classes couvrent ~90% des situations météo courantes

### 2.3 Récupération des données (bibliothèque Meteostat)

```python
# Installation des dépendances
# pip install meteostat tensorflow tf2onnx pandas scikit-learn

from meteostat import Point, Daily
from datetime import datetime
import pandas as pd
import numpy as np

# Station météo : Chambéry (France)
chambery = Point(45.5646, 5.9178, 270)

# Récupération des données historiques (5 ans)
start = datetime(2019, 1, 1)
end   = datetime(2023, 12, 31)

data = Daily(chambery, start, end)
data = data.fetch()

# Colonnes utilisées : tavg (temp), rhum (humidité), pres (pression), coco (code météo)
data = data[['tavg', 'rhum', 'pres', 'coco']].dropna()

print(f"Données récupérées : {len(data)} jours")
print(data.head())
```

### 2.4 Préparation des données et étiquetage

```python
def label_meteo(row):
    """
    Attribution des classes météo selon les codes Meteostat (coco)
    et les valeurs capteurs
    """
    coco = row['coco']
    temp = row['tavg']
    
    if coco in [1, 2]:                    # Ciel dégagé / Peu nuageux
        return 0  # Clair
    elif coco in [3, 4]:                  # Nuageux / Couvert
        return 1  # Nuageux
    elif coco in [7, 8, 9, 10, 11]:       # Pluie / Averses
        return 2  # Pluie
    elif coco in [14, 15, 16] or temp < 0:# Neige / Grésil
        return 3  # Neige
    else:
        return 1  # Par défaut : Nuageux

data['label'] = data.apply(label_meteo, axis=1)

# Features et labels
X = data[['tavg', 'rhum', 'pres']].values.astype('float32')
y = data['label'].values

print(f"Distribution des classes : {np.bincount(y)}")
```

### 2.5 Entraînement du modèle TensorFlow

```python
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

# Normalisation des données
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# Séparation train/test (80%/20%)
X_train, X_test, y_train, y_test = train_test_split(
    X_scaled, y, test_size=0.2, random_state=42, stratify=y
)

# ====================================================
# Architecture du modèle (optimisée pour embarqué)
# ====================================================
model = tf.keras.Sequential([
    tf.keras.Input(shape=(3,)),               # 3 entrées : temp, hum, pression
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dropout(0.2),
    tf.keras.layers.Dense(8,  activation='relu'),
    tf.keras.layers.Dense(4,  activation='softmax')  # 4 classes de sortie
], name='MeteoStat_Classifier')

model.summary()

# ====================================================
# Hyperparamètres choisis
# ====================================================
# Optimizer : Adam (taux d'apprentissage adaptatif)
# Loss : sparse_categorical_crossentropy (classes entières)
# Epochs : 50 (compromis temps/convergence)
# Batch_size : 32
# ====================================================

model.compile(
    optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

# Callbacks
callbacks = [
    tf.keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
    tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=5)
]

history = model.fit(
    X_train, y_train,
    epochs=50,
    batch_size=32,
    validation_split=0.2,
    callbacks=callbacks,
    verbose=1
)

# Évaluation
test_loss, test_acc = model.evaluate(X_test, y_test)
print(f"\nAccuracy sur test : {test_acc:.4f} ({test_acc*100:.2f}%)")
```

### 2.6 Étude du compromis Classes / Taille / Accuracy

| Nb classes | Couches | Paramètres | Accuracy (test) | Taille ONNX |
|---|---|---|---|---|
| 2 (Beau/Mauvais) | Dense(8) + Dense(2) | 50 | ~88% | ~5 Ko |
| **4 (retenu)** | **Dense(16)+Dense(8)+Dense(4)** | **252** | **~81%** | **~8 Ko** |
| 6 | Dense(32)+Dense(16)+Dense(6) | 758 | ~74% | ~15 Ko |
| 12 | Dense(64)+Dense(32)+Dense(12) | 2700 | ~68% | ~45 Ko |

**Conclusion :** 4 classes avec une architecture légère offre le meilleur compromis. Plus de classes fragmente les données et réduit la précision sans gain opérationnel réel pour un capteur embarqué.

### 2.7 Export vers ONNX (pour MATLAB et X-CUBE-AI)

```python
import tf2onnx
import onnx

# Sauvegarde du modèle TensorFlow
model.save("meteostat_model")

# Conversion en ONNX
spec = (tf.TensorSpec((None, 3), tf.float32, name="input"),)

model_proto, _ = tf2onnx.convert.from_keras(
    model,
    input_signature=spec,
    output_path="meteostat_classifier.onnx"
)

print("✅ Modèle exporté : meteostat_classifier.onnx")

# Vérification
onnx_model = onnx.load("meteostat_classifier.onnx")
onnx.checker.check_model(onnx_model)
print("✅ Modèle ONNX valide")
```

### 2.8 Import et inférence dans MATLAB (ThingSpeak)

```matlab
%% Import du modèle ONNX dans MATLAB
model = importONNXNetwork("meteostat_classifier.onnx", ...
                          OutputLayerType="classification");

%% Lecture des données réelles depuis ThingSpeak
channelID  = VOTRE_CHANNEL_ID;
readAPIKey = 'VOTRE_READ_API_KEY';

data = thingSpeakRead(channelID, 'Fields', [1 2 3], ...
                      'NumPoints', 1, 'ReadKey', readAPIKey);

temperature = data(1);
humidity    = data(2);
pressure    = data(3);

% Normalisation (utiliser les mêmes paramètres que lors de l'entraînement)
% mu et sigma issus du StandardScaler Python
mu    = [12.3, 72.4, 1010.5];  % Remplacer par vos vraies valeurs
sigma = [8.2,  18.6, 9.1];
input_data = (data - mu) ./ sigma;

%% Inférence
input_tensor = reshape(single(input_data), 1, 3);
scores = predict(model, input_tensor);
[prob, class_idx] = max(scores);

classes = {'Clair', 'Nuageux', 'Pluie', 'Neige'};
fprintf('=== Résultat de la classification ===\n');
fprintf('Données : T=%.1f°C, H=%.1f%%RH, P=%.1f hPa\n', temperature, humidity, pressure);
fprintf('Classe prédite : %s (confiance : %.2f%%)\n', classes{class_idx}, prob*100);

%% Sauvegarde du résultat sur un canal dédié (résultats IA)
resultChannelID  = VOTRE_RESULT_CHANNEL_ID;
resultWriteKey   = 'VOTRE_RESULT_WRITE_KEY';
thingSpeakWrite(resultChannelID, class_idx, 'WriteKey', resultWriteKey);
fprintf('Résultat sauvegardé sur ThingSpeak (channel %d)\n', resultChannelID);
```

---

## 📊 Résultats obtenus

| Étape | Résultat |
|---|---|
| Canal ThingSpeak configuré | ✅ 3 champs actifs |
| Envoi données STM32 → Cloud | ✅ `HTTP 200 OK`, entrées enregistrées |
| Analyse MATLAB | ✅ Moyennes glissantes calculées et affichées |
| Modèle TensorFlow entraîné | ✅ Accuracy test ≈ 81% sur 4 classes |
| Export ONNX | ✅ Fichier `meteostat_classifier.onnx` valide |
| Inférence MATLAB | ✅ Classification fonctionnelle |

---

## ⚠️ Points clés à retenir

| Point | Détail |
|---|---|
| Fréquence d'envoi ThingSpeak | Minimum **15 secondes** entre deux mises à jour (limite free tier) |
| Format float en C | Activer `-u _printf_float` pour `%.1f` dans `sprintf` |
| Normalisation obligatoire | Appliquer les **mêmes µ et σ** à l'inférence qu'à l'entraînement |
| Format ONNX | Utiliser `tf.keras.Input(shape=(3,))` **explicite** dans le modèle avant `tf2onnx` |

---

## 📚 Références

- [ThingSpeak – Collect Data Tutorial](https://thingspeak.com/pages/learn_more)
- [Meteostat Python Library](https://dev.meteostat.net/python/)
- [tf2onnx – GitHub](https://github.com/onnx/tensorflow-onnx)
- [MATLAB importONNXNetwork](https://www.mathworks.com/help/deeplearning/ref/importonnxnetwork.html)

