# Tests sur un poste client Hyper-V

## Point d'entrée courant

```cmd
tests.cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal -Pattern "26-*"
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

L'absence de `-VM` conserve le mode local. Le nom désigne une VM déjà configurée,
jamais une création implicite. Les filtres sélectionnent les mêmes cas que le banc
local, avant la préparation. L'application, les tests natifs et les helpers C#
sont compilés sur l'hôte ; seules leurs dépendances et les scripts sélectionnés
sont envoyés dans l'invité. Les assemblies et exécutables sont vérifiés par SHA256.
Les assertions ne changent pas. Un helper absent ou modifié provoque un échec,
sans compilation invitée et sans repli vers le bureau hôte.

Le poste client doit être allumé, avec une session graphique ouverte du compte
de test. Fermer VMConnect, sans fermer la session ni arrêter la VM. Les tests
s'exécutent sous un jeton limité dans cette session. L'utilisateur peut continuer
à travailler sur l'hôte. Le contrôleur transfère si nécessaire une session invitée
déconnectée vers sa console ; il ne déverrouille pas Windows.

La tâche hôte protégée et les identifiants DPAPI existants sont réutilisés. Pour
configurer **une nouvelle VM dédiée**, une seule fois et au moment choisi :

```cmd
prepare-client-hyperv.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "NomVM"
```

Le compte invité doit avoir un mot de passe. La configuration demande UAC et
les identifiants Windows invités, puis les conserve chiffrés hors du dépôt.
Ne pas exécuter cette procédure à chaque test. Une mise à jour du contrôleur
utilise le même script avec `-UpdateController -VMName "NomVM"`, en conservant
les identifiants. La version de protocole 2 permet les paquets du banc principal.
Le contrôleur élevé exécute uniquement ses scripts protégés sur l'hôte ; les
scénarios du checkout sont exécutés uniquement dans la VM.

Les rapports sont récupérés automatiquement sous
`build/regression/vm-<id>/output/regression/`, accompagnés des contrôles de
session, de déploiement, de restauration et des journaux hôte. `-RunDirectory`
demande une copie à un autre emplacement libre. Les codes de sortie des cas,
du runner, de la tâche invitée et de la tâche hôte sont vérifiés. Les délais
maximaux tiennent compte du nombre de cas sélectionnés et de `-TimeoutSeconds`.
Les scénarios Everything nécessitent Everything dans l'invité ; l'installation
de l'hôte n'est pas une installation du client. Les préconditions propres à chaque
cas (écrans, providers Shell, bureau déverrouillé…) restent applicables.

La préparation des cas a été vérifiée sur l'hôte. La validation effective
de ce transport porte sur des passages ciblés ; elle ne constitue pas un PASS
du catalogue complet dans la VM.

## Diagnostic minimal historique

Le prototype Windows Sandbox a été retiré après les mesures d'interférence avec
le pointeur hôte et les limitations d'entrée lorsque sa fenêtre est minimisée.
L'historique des essais et leurs limites sont conservés dans
[development-history.md](../../docs/development-history.md).

`prepare-client-hyperv.cmd` et `test-agentwin10.cmd` conservent un diagnostic
minimal (vrai dossier Shell, redimensionnement Alt-droit, fermeture normale),
antérieur à l'intégration du banc. Il ne remplace pas `tests.cmd -VM` et son PASS
ne constitue pas une validation des autres scénarios. `connect-agentwin10.cmd`
sert uniquement au diagnostic manuel avec authentification Windows.

La tâche hôte utilise un exécutable .NET sans console, `NoConsoleRunner.exe`,
pour lancer uniquement son contrôleur PowerShell protégé. Une ancienne fenêtre
PowerShell masquée pouvait apparaître au premier plan ; le lanceur actuel crée
le processus avec `CreateNoWindow=true` et capture stdout/stderr et le code de
sortie. Les sources du dépôt ne sont jamais exécutées élevées sur l'hôte.

Références Microsoft : [PowerShell Direct](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/powershell-direct),
[tâches interactives](https://learn.microsoft.com/en-us/windows/win32/taskschd/security-contexts-for-running-tasks),
[identifiants chiffrés DPAPI](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/export-clixml?view=powershell-5.1).
