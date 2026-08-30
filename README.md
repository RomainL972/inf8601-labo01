# Laboratoire 1

Pipeline de modification d'images

_Écrit par Gabriel-Andrew Pollo-Guilbert et mis à jour par Félix Gagnon (2026)_

Le pipelinage est une méthode de traitement de données où chaque étape du traitement alimente
la prochaine. Celle-ci est souvent implémenté de sorte que chacune des étapes est exécutée en
parallèle, comme dans un pipeline d'instructions d'un processeur ou dans un pipeline graphique.

Ce laboratoire a pour but de vous familiariser avec la programmation parallèle de base par le
biais de l'implémentation d'un pipeline de traitement d'images. Pour se faire, vous devez convertir
un pipeline sériel en un pipeline parallèle à l'aide des librairies
[POSIX Threads](https://en.wikipedia.org/wiki/POSIX_Threads) (pthreads) et
[Threading Building Blocks](https://en.wikipedia.org/wiki/Threading_Building_Blocks) (TBB) de Intel.

**Chaque noeud d'exécution ne doit qu'exécuter qu'une seule étape du pipeline. Lorsque l'image est
convertit, il l'envoit à la prochaine étape si nécessaire.**

## Code

- `source/main.cpp`
  - Contient le point d'entrée du programme qui traite les arguments en ligne de commande et démarre
    le pipeline de traitement d'images voulu.
- `source/image.cpp` `include/image.h`
  - Contiennent les structures et le code permettant la lecture/écriture d'images de format PNG.
- `source/filter.cpp` `include/filter.h`
  - Contiennent différentes fonctions permettant d'appliquer des filtres à des images.
- `source/queue.cpp` `include/queue.h`
  - Contiennent une implémentation simple d'une file permettant la lecture/écriture par plusieurs
    noeuds d'exécution. Ces structures et fonctions sont **fortement** recommandé lors de
    l'implémentation du pipeline utiliant pthreads.
- `serials/pipeline-serial.cpp`
  - Contient l'implémentation sérielle de référence du pipeline (ne pas modifier).
- `source/pipeline-pthread.cpp` (**À COMPLÉTER**)
  - Contient l'implémentation parallèle demandée du pipeline à l'aide de pthreads.
- `source/pipeline-tbb.cpp` (**À COMPLÉTER**)
  - Contient l'implémentation parallèle demandée du pipeline à l'aide de TBB.
- `data/fetch.py`
  - Contient un script pour télécharger les images de test.
- `tests/check_pipelines.cpp` (ne pas modifier)
  - Programme qui vérifie que les images produites par pthread/TBB sont identiques octet-à-octet aux images sérielles.
- `tests/generate_random.cpp` (ne pas modifier)
  - Programme qui génère une image de test aléatoire.

## Spécifications

Les étages du pipeline à implémenter par votre équipe sont décrits à la fin de ce document.

De plus, les contraintes suivantes sont imposées :

- La compilation ne doit pas lancer d'avertissements.
- Le programme ne doit pas avoir de fuite de mémoire durant son exécution.

### Environnement de développement

Sous Linux, installez simplement les outils nécessaires (Clang, CMake, Ninja) avec le
gestionnaire de paquets de votre distribution et compilez directement sur la machine hôte (voir
Compilation ci-dessous).

Sous Windows et macOS ouvrez le dépôt
dans [VS Code](https://code.visualstudio.com/) avec l'extension
[Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers). Cela construit une image contenant les outils nécessaires et vous donne CMake
Tools déjà configuré pour compiler/lancer/déboguer les cibles directement depuis l'éditeur.

### Compilation

Pour compiler l'application, il est recommandé de créer un dossier `build/` à la racine du projet
afin de bien séparer les fichiers générés.

On configure d'abord le projet avec `cmake` en utilisant le générateur [Ninja](https://ninja-build.org/). La première
configuration télécharge et compile TBB depuis les sources, ce qui peut prendre
quelques minutes ; les invocations suivantes réutilisent ce qui a déjà été compilé.

```
$ cmake -B build -G Ninja
$ cmake --build build
```

Il n'est pas nécessaire de re-exécuter la première commande pour recompiler le binaire, seulement
la seconde. Vous pouvez exécuter `./build/pipeline --help` pour voir les options du programme.

### Données et Résultats

Le programme lit les images dans un dossier ayant les noms `0000.png`, `0001.png`, `0002.png`, etc.
Il va ensuite enregistrer les images résultantes dans le même dossier avec un préfixe comme
`pthread-0000.png`, `pthread-0001.png`, etc.

Deux dossiers sont utilisés à cette fin :

- `data/synthetic/` contient une unique image générée à la volée par `ctest`/`ninja -C build test`
  (voir Commandes ci-dessous) ; elle sert uniquement à valider rapidement que les 3 pipelines
  produisent des sorties identiques, sans dépendre du téléchargement ci-dessous.
- `data/downloaded/` contient les vraies images de test, obtenues via `data/fetch.py` (voir
  ci-dessous) ; c'est ce dossier qu'utilisent `ninja -C build run-serial`/`run-pthread`/`run-tbb`/`run-all`
  pour mesurer des accélérations représentatives.

Le script `data/fetch.py` est fourni afin d'obtenir les images de bases dans `data/downloaded/`.
Celui-ci va télécharger des images PNGs du film à license libre
[Big Buck Bunny](https://fr.wikipedia.org/wiki/Big_Buck_Bunny) (environ 200 MB).

```
$ ./data/fetch.py        # toutes les images (équivalent à ./data/fetch.py tous)
$ ./data/fetch.py 20     # seulement les 20 premières, pour un test plus rapide
```

**Avec les images modifiées, le dossier peut faire plus de 1 GB. Il n'est donc pas recommandé de
travailler sur le laboratoire directement sur votre disque réseau de l'école. Vous pouvez utiliser
`/home/tmp` sur les ordinateurs du laboratoire. Cela dit, ce dossier est supprimé à chaque 24 heures.**

Si vous utilisez `/home/tmp` (ou tout autre dossier hors du projet) pour vos données, téléchargez
les images directement dedans avec l'option `-d`/`--dest` de `data/fetch.py` :

```
$ ./data/fetch.py --dest /home/tmp/.../data/downloaded
```

### Commandes

- `ninja -C build run-serial`
  - Exécute le pipeline sériel avec les données dans le dossier `data/downloaded/` et affiche le
    temps écoulé. Utilise la cible `pipeline-notbb`, qui ne compile pas `pipeline-tbb.cpp` :
    fonctionne même si votre implémentation TBB ne compile pas encore.
- `ninja -C build run-pthread`
  - Exécute le pipeline utilisant pthreads avec les données dans le dossier `data/downloaded/` et
    affiche le temps écoulé. Utilise `pipeline-notbb` pour la même raison que ci-dessus.
- `ninja -C build run-tbb`
  - Exécute le pipeline utilisant TBB avec les données dans le dossier `data/downloaded/` et
    affiche le temps écoulé.
- `ninja -C build run-all`
  - Exécute les 3 pipelines ci-dessus sur `data/downloaded/`, affiche le temps de chacun, puis
    vérifie que leurs sorties sont identiques octet-à-octet. Échoue si ce dossier est vide ou
    absent (voir `data/fetch.py` ci-dessus), ou si l'une des 3 implémentations ne compile pas
    encore. Cette commande valide seulement, et rapidement, que les 3 pipelines produisent le même
    résultat : le petit nombre d'images qu'on lui donne pour ça ne suffit pas pour juger de
    l'accélération apportée par le parallélisme, utilisez `run-serial`/`run-pthread`/`run-tbb` sur
    l'ensemble de `data/downloaded/` pour ça.
- `ninja -C build test` (ou `ctest --test-dir build`, une fois le projet compilé)
  - Génère une image de test dans `data/synthetic/`, exécute les 3 pipelines dessus et vérifie
    que leurs sorties sont identiques octet-à-octet.

### Détection d'erreurs mémoire (AddressSanitizer)

Pour détecter des bugs mémoire  dans votre implémentation, configurez le projet avec `-DENABLE_ASAN=ON` : cela recompile `pipeline`,
`pipeline-notbb`, `generate-random` et `check-pipelines` avec AddressSanitizer. Utilisez un
dossier `build/` séparé pour ne pas mélanger avec une compilation normale :

```
$ cmake -B build-asan -G Ninja -DENABLE_ASAN=ON
$ cmake --build build-asan
$ ctest --test-dir build-asan
```

### Exemple

Dans l'exemple ci-dessous, on télécharge les images de test, on exécute les 3 algorithmes et on
vérifie si toutes les images créées sont identiques.

```
$ ./data/fetch.py
$ cmake -B build -G Ninja
$ cmake --build build
$ ninja -C build run-all
```

## Remise

Une fois votre travail commité, générez directement l'archive de remise avec `git archive`. Seuls
`source/pipeline-pthread.cpp` et `source/pipeline-tbb.cpp` sont à remettre :

```
$ git archive --format=zip --output=matricule1_matricule2.zip HEAD -- source/pipeline-pthread.cpp source/pipeline-tbb.cpp
```
