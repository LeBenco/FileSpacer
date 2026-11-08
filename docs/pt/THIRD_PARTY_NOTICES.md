<a id="third-party-notices"></a>

# Avisos relativos a componentes de terceiros

O FileSpacer baseia-se no [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, distribuído ao abrigo da [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

O instalador NSIS utiliza o módulo LockedList, Copyright (c) 2013 Afrow Soft Ltd, ao abrigo da licença zlib. O aviso completo encontra-se em [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) e é incluído como `LockedList-LICENSE.txt` na pasta de instalação. Consulte <https://nsis.sourceforge.io/LockedList_plug-in>.

O AppUserModelID dos atalhos é definido pelo `ShortcutAppID.nsh` do FileSpacer através das macros Windows COM do NSIS. O antigo submódulo `nsis-shortcut-properties` não é utilizado. Não são utilizados o Material Design Icons nem o subconjunto do tipo de letra Segoe anteriormente incluído.

<a id="sqlite"></a>

## SQLite

O SQLite 3.53.4 está ligado estaticamente à aplicação e pertence ao domínio público. O código-fonte não modificado e a sua proveniência encontram-se em `third_party/sqlite`. Consulte <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Ícones

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

Na barra de ferramentas, a **seta para cima** abre a **pasta superior** e a **seta circular** **atualiza o conteúdo da pasta**. Estes dois ícones são adaptados do **Lucide** (`arrow-up` e `rotate-cw`). O ícone da seta para cima tem origem no **Feather**.

Consulte [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) para os avisos ISC e MIT completos no código-fonte, ou `Lucide-LICENSE.txt` na pasta de instalação. Consulte <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Os ícones de cadeado fechado e aberto (`src/res/window-state-lock.ico` e `src/res/window-state-unlock.ico`) provêm do ícone **lock** da coleção Small & Flat de paomedia, dedicado ao domínio público ao abrigo de [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/). O cadeado aberto é uma versão derivada.

Coleção original e licença: https://github.com/paomedia/small-n-flat

Distribuição ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
