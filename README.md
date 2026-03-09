# 🧠 TP1 — Introduction aux Réseaux de Neurones avec MNIST

Ce TP explore les fondements des réseaux de neurones denses (fully connected) à travers le dataset **MNIST** (reconnaissance de chiffres manuscrits), en comparant différents choix d'architecture et d'hyperparamètres.

---

## 📋 Contenu du TP

### 1. 🔀 Fonctions d'Activation
Comparaison de 4 architectures sur 10 epochs :

| Modèle | Activation | Couches cachées | Synapses | Précision (Val) |
|--------|-----------|-----------------|----------|-----------------|
| A | Softmax | 0 | 7 850 | ~92.9% |
| B | ReLU | 2 (128 + 64) | 109 386 | ~97.5% |
| C | Tanh | 1 (128) | 101 770 | ~97.4% |
| D | Sigmoid | 1 (128) | 101 770 | ~97.3% |

> ✅ **Meilleur compromis** : Modèle C (Tanh) — bonne précision, loss stable, architecture légère.

---

### 2. ⚙️ Algorithmes d'Optimisation
Même architecture de base (128 neurones, ReLU) testée avec 4 optimiseurs :

| Optimiseur | Précision (Val) | Perte (Val) |
|------------|-----------------|-------------|
| SGD | 94.86% | 0.1866 |
| Adam | 97.53% | 0.0990 |
| RMSprop | 97.67% | 0.0979 |
| Adagrad | 91.15% | 0.3339 |

> ✅ **Meilleur résultat** : RMSprop et Adam, très proches. Adam reste le choix standard.

---

### 3. 📉 Fonctions de Coût
3 fonctions de perte testées selon le type de classification :

| Fonction de coût | Précision (Val) | Perte (Val) | Usage |
|-----------------|-----------------|-------------|-------|
| `binary_crossentropy` | 99.12% | 0.0324 | Classification binaire (ex: "5 ou non 5") |
| `categorical_crossentropy` | 97.20% | 0.0924 | Multi-classes, labels one-hot |
| `sparse_categorical_crossentropy` | 97.54% | 0.0853 | Multi-classes, labels entiers |

---

## 🚀 Lancer le notebook

### Option 1 — Google Colab *(recommandé)*
[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/)

### Option 2 — En local

```bash
# Cloner le repo
git clone https://github.com/<votre-username>/<votre-repo>.git
cd <votre-repo>

# Installer les dépendances
pip install tensorflow matplotlib numpy

# Lancer Jupyter
jupyter notebook TP1_AI.ipynb
```

---

## 🛠️ Technologies utilisées

- **Python 3**
- **TensorFlow / Keras** — construction et entraînement des modèles
- **NumPy** — manipulation des données
- **Matplotlib** — visualisation *(si applicable)*
- **Dataset** : [MNIST](http://yann.lecun.com/exdb/mnist/) — 60 000 images d'entraînement, 10 000 de test

---

## 📁 Structure du projet

```
📦 TP1-AI
 ┗ 📓 TP1_AI.ipynb     # Notebook principal
 ┗ 📄 README.md        # Ce fichier
```

---

## 📌 Conclusions clés

- Une architecture **sans couche cachée** est insuffisante pour MNIST (~93%).
- **ReLU** offre la meilleure convergence rapide avec plusieurs couches cachées.
- **Adam** et **RMSprop** dominent largement SGD et Adagrad.
- Le choix de la **fonction de coût** dépend du type de problème (binaire vs multi-classes).
