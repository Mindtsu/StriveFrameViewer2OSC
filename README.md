# StriveOSC

Mod UE4SS pour **Guilty Gear -Strive-** qui envoie en **OSC**, en temps réel, ce qui se passe pendant un combat : les coups tels que le jeu les a interprétés, la vie, la tension, le burst, la R.I.S.C. et les rounds. Il est pensé pour piloter un dispositif sonore (Max/MSP, Pure Data, etc.).

Le mod n'affiche rien à l'écran. Les messages ne quittent jamais l'ordinateur : seules les adresses locales (`127.x.x.x`) sont acceptées.

Dérivé de [StriveFrameViewer](https://github.com/Procdox/StriveFrameViewer) (pbozai, Sevoi, Procdox). Les offsets de la vie, de la tension et du burst proviennent de [StriveLabs](https://github.com/ENVYFGC/StriveLabs).

## Messages OSC

Préfixe par défaut : `/ggst`, port `9000`. `pN` vaut `p1` ou `p2`.

| Adresse | Argument | Envoyé quand |
|---|---|---|
| `/ggst/pN/move` | symbole, ex. `NmlAtk5A` | un coup est déclenché par une commande |
| `/ggst/pN/state` | symbole, ex. `CmnActFDash` | l'état du personnage change (si `send_states = 1`) |
| `/ggst/pN/hp` | entier | la vie change |
| `/ggst/pN/hp_norm` | flottant 0 à 1 | la vie change (1 = vie pleine au début du round) |
| `/ggst/pN/damage` | entier | le joueur perd de la vie (quantité perdue) |
| `/ggst/pN/tension` | flottant 0 à 100 | la tension change |
| `/ggst/pN/burst` | flottant 0 à 100 | le burst change |
| `/ggst/pN/risc` | entier (brut) | la R.I.S.C. change (une fois l'offset configuré) |
| `/ggst/pN/rounds` | entier | rounds gagnés |
| `/ggst/pN/lives` | entier | rounds que l'adversaire doit encore gagner |
| `/ggst/pN/round_win` | aucun | le joueur gagne un round |
| `/ggst/match/start` | aucun | début de match (ou rematch) |
| `/ggst/round/start` | entier (n° de round) | début de round |
| `/ggst/round/end` | entier | fin de round |
| `/ggst/mode` | entier | entrée dans un mode de jeu actif |
| `/ggst/frame` | entier | chaque frame (si `send_frame = 1`) |

Les jauges sont envoyées uniquement quand elles changent, et toutes renvoyées au début de chaque round.

Test rapide dans Max : `[udpreceive 9000]` → `[print]`.

### `move` ou `state` ?

- **`move`** est le nom du coup reconnu par l'interpréteur de commandes du jeu (le nom interne du « skill »). C'est la phraséologie du joueur : un coup = un message.
- **`state`** est l'état de script dans lequel se trouve le personnage. Il change aussi pour la marche, la garde, les dégâts subis, et pour chaque étape d'un coup en plusieurs parties.

Les noms sont **internes au jeu** (par exemple `NmlAtk5A` pour 5P), pas la notation 236H. Pour construire votre table de correspondance coup → son, relevez-les en mode entraînement : mettez `log_moves = 1`, jouez chaque coup du personnage, puis lisez le fichier `UE4SS.log` (dans le dossier `Win64` du jeu).

## Installation sous CrossOver

1. Téléchargez le paquet `StriveOSC` produit par la compilation (voir plus bas) et copiez son contenu dans :
   `~/Library/Application Support/CrossOver/Bottles/<bouteille>/drive_c/Program Files (x86)/Steam/steamapps/common/GUILTY GEAR STRIVE/RED/Binaries/Win64/`
2. Si un autre mod UE4SS était installé (StriveFrameViewer, StriveLabs), retirez son dossier dans `Mods/` et supprimez le dossier `cache`.
3. Dans CrossOver, ouvrez la **Configuration de Wine** de la bouteille, onglet **Bibliothèques**, ajoutez `dwmapi` et réglez-la sur **native, puis intégrée**. Sans ce réglage, le mod ne se charge pas.
4. Lancez un Versus local ou l'entraînement. Le fichier `UE4SS.log` doit contenir des lignes `[StriveOSC]`, dont `game mode 6: OSC active`.

## Configuration

Le fichier `Mods/StriveOSC/osc_config.ini` est commenté. Les réglages principaux :

- `port` et `prefix` : destination et préfixe OSC ;
- `modes` : modes de jeu actifs. Par défaut `6,7,8` (Versus local et entraînement). Le numéro du mode courant est écrit dans `UE4SS.log` ;
- `send_states`, `send_frame`, `log_moves`, `rounds_to_win`.

Relancez le jeu après chaque modification.

## Trouver la R.I.S.C.

Aucun projet public ne documente l'emplacement de la jauge R.I.S.C. en mémoire. Le mod inclut un outil pour la trouver, à utiliser une fois :

1. Mettez `risc_finder = 1` dans `osc_config.ini`, relancez le jeu, allez en **entraînement**. Réglez le mannequin pour qu'il **bloque tout**.
2. Faites un **reset** de l'entraînement (R.I.S.C. du mannequin vide), puis appuyez sur **F6**.
3. Frappez le mannequin pour qu'il bloque plusieurs coups (sa R.I.S.C. monte), puis appuyez sur **F7**.
4. Attendez que sa jauge redescende un peu, puis appuyez sur **F8**.
5. Répétez F7 (après avoir fait monter la jauge) et F8 (après l'avoir laissée descendre) jusqu'à ce qu'il reste quelques candidats. Ils s'affichent dans `UE4SS.log`, par exemple `candidate player:0x1234 = 3200`.
6. Reportez le résultat dans la configuration :
   - `player:0x...` : la même valeur vaut pour les deux joueurs, `risc_p1 = player:0x...` et `risc_p2 = player:0x...` ;
   - `engine:0x...` : c'est la valeur du joueur 2. Celle du joueur 1 est probablement à `0x198` de moins, ou `4` de moins, à vérifier en observant `/ggst/p1/risc`.
7. Remettez `risc_finder = 0`.

Si plusieurs candidats résistent, testez-les un par un en les mettant dans `risc_p2` et en observant `/ggst/p2/risc` dans Max.

## Compilation avec GitHub Actions (depuis un Mac)

La compilation se fait sur une machine Windows de GitHub, à chaque `push` sur `master`, ou à la main depuis l'onglet **Actions**.

UE4SS dépend d'un dépôt privé (`Re-UE4SS/UEPseudo`) que seuls les comptes GitHub liés à un compte Epic Games peuvent lire. Il faut donc donner au workflow une clé SSH de votre compte :

1. Liez votre compte GitHub à Epic Games et acceptez l'invitation de l'organisation EpicGames reçue par e-mail.
2. Dans le Terminal du Mac : `ssh-keygen -t ed25519 -f ~/.ssh/ggst_ci -N ""`
3. Sur GitHub, **Settings → SSH and GPG keys → New SSH key** : collez le contenu de `~/.ssh/ggst_ci.pub` (`cat ~/.ssh/ggst_ci.pub`).
4. Dans le dépôt, **Settings → Secrets and variables → Actions → New repository secret** : nom `UEPSEUDO_SSH_KEY`, valeur = contenu de `~/.ssh/ggst_ci` (la clé **privée**, `cat ~/.ssh/ggst_ci`).
5. Onglet **Actions** : activez les workflows si GitHub le demande, puis lancez **Build StriveOSC**. Le paquet se télécharge en bas de la page du run, section **Artifacts**.

Ne publiez jamais la clé privée ailleurs que dans ce secret.

## Limites connues

- Le mod n'a pas pu être testé en jeu au moment de l'écriture. Les offsets viennent de mods à jour en novembre 2026 ; un patch du jeu peut les casser.
- La détection de `move` repose sur le skill demandé par le jeu. Si un coup n'apparaît pas, son `state` reste disponible.
- Le nom du personnage n'est pas envoyé.
- En ligne, le rollback rejoue des frames : le mod ignore ces frames rejouées, mais l'usage en ligne n'est pas recommandé.

## Désactiver

Supprimez ou renommez `dwmapi.dll` dans le dossier `Win64` du jeu.

## Avertissement

Ce mod repose sur de l'injection de code. Il peut contrevenir aux conditions d'utilisation d'Arc System Works. Utilisez-le en local (Versus sur une seule machine) et désactivez-le avant de jouer en ligne.
