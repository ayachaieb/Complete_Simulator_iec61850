# README

## Description
Ce projet est une application permettant de simuler des valeurs brutes de courants et tensions pour des tests conformes à la norme IEC61850. L'application utilise des scénarios de test pour configurer les paramètres de simulation.

---

## Instructions

### Compilation
Pour compiler le projet, exécutez la commande suivante :

```bash
cd emis/UC_STM32/TEST_SAMPLE_VALUES_IEC61850 && make all
```

### Exécution
Pour exécuter le binaire généré, utilisez la commande suivante :

```bash
sudo BIN/simuStpmMonoChBrutVal lo ./scenario_DPA.txt 0x5000 01:0C:CD:04:00:01 ./goose_config.conf
```

- **lo** : Interface réseau à utiliser.
- **./scenario_DPA.txt** : Fichier de scénario définissant les paramètres de test.
- **0x5000** : appID SV
- **01:0C:CD:04:00:01** : Adresse MAC destination SV
- **./goose_config.conf** : Fichier de configuration des goose suscribers.
---

## Format du fichier de scénario
Le fichier de scénario doit être structuré comme suit :

```plaintext
# Phase 0
duration_ms=10000
channel1_voltage1=8550.74
channel1_voltage2=8550.74
channel1_voltage3=8550.74
channel1_current1=400.0
channel1_current2=400.0
channel1_current3=400.0
channel1_voltage1_phi=0.0
channel1_voltage2_phi=240.0
channel1_voltage3_phi=120.0
channel1_current1_phi=0.0
channel1_current2_phi=240.0
channel1_current3_phi=120.0

# Phase 1
duration_ms=1200
channel1_voltage1=8550.74
channel1_voltage2=8550.74
channel1_voltage3=8850.74
channel1_current1=560.0
channel1_current2=560.0
channel1_current3=560.0
channel1_voltage1_phi=0.0
channel1_voltage2_phi=240.0
channel1_voltage3_phi=120.0
channel1_current1_phi=0.0
channel1_current2_phi=240.0
channel1_current3_phi=120.0

# Phase 2
duration_ms=10000
channel1_voltage1=8550.74
channel1_voltage2=8550.74
channel1_voltage3=8550.74
channel1_current1=400.0
channel1_current2=400.0
channel1_current3=400.0
channel1_voltage1_phi=0.0
channel1_voltage2_phi=240.0
channel1_voltage3_phi=120.0
channel1_current1_phi=0.0
channel1_current2_phi=240.0
channel1_current3_phi=120.0
```

### Définition des éléments
- **Phase** : Numéro du scénario. Chaque phase correspond à un ensemble de paramètres pour la simulation.
- **duration_ms** : Durée en millisecondes de la phase.
- **channel1_voltageX** : Tension pour le canal 1 sur les phases X (1, 2, 3).
- **channel1_currentX** : Courant pour le canal 1 sur les phases X (1, 2, 3).
- **channel1_voltageX_phi** : Dephasage Tension pour le canal 1 sur les phases X (1, 2, 3).
- **channel1_currentX_phi** : Dephasage Courant pour le canal 1 sur les phases X (1, 2, 3).
---

## Remarques
- Les fichiers de scénario doivent être correctement formatés pour éviter des erreurs lors de l'exécution.
- Assurez-vous que l'interface réseau spécifiée (e.g., **lo**) est active.

---

