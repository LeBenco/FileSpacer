<p align="center">
    <img height="200" alt="FileSpacer" src="../../src/res/FileSpacer.svg" />
</p>


# FileSpacer

[![Transferir versão mais recente](https://img.shields.io/badge/Transferir-vers%C3%A3o%20mais%20recente-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![telemetria: nenhuma](https://img.shields.io/badge/telemetria-nenhuma-2ea44f)](#privacy)
[![Licença: GPL v3](https://img.shields.io/badge/Licen%C3%A7a-GPLv3-blue.svg)](#licença-e-agradecimentos)

O FileSpacer é um gestor de ficheiros para Windows baseado no [paradigma espacial](https://en.wikipedia.org/wiki/Spatial_file_manager): cada pasta tem uma única janela que memoriza a posição, o tamanho e as definições de apresentação. Isto cria [referências visuais estáveis](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), permitindo que a [memória espacial](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) ajude a reconhecer as pastas familiares de relance. Num gestor de ficheiros baseado na navegação, a mesma janela mostra sucessivamente pastas diferentes: a posição e as dimensões, por si só, não identificam a pasta aberta, pelo que é preciso olhar para o conteúdo para saber onde se está. Ao conservar referências distintas para cada pasta, a abordagem espacial pode reduzir o esforço necessário para se orientar entre os seus ficheiros. Os separadores são incompatíveis com o modelo espacial e nunca serão adicionados.

<img src="images/spatial-windows-pt.png" alt="Três janelas de pastas sem sobreposição, com tamanhos, modos de apresentação e níveis de zoom diferentes, incluindo uma disposição livre de ícones." width="560">

O FileSpacer privilegia uma experiência leve e descomplicada. Para limitar a acumulação de janelas, uma desvantagem conhecida do paradigma espacial, o FileSpacer fecha por predefinição a janela de origem ao abrir uma pasta. Mantenha Ctrl premido durante a abertura para a conservar quando quiser trabalhar com várias pastas ao mesmo tempo. O FileSpacer mantém uma interface deliberadamente sóbria, ao contrário de certas interfaces sobrecarregadas que mal deixam espaço entre os botões para se vislumbrar um ficheiro.

<a id="system--languages"></a>**Sistema e idiomas:** Windows 10 ou posterior, x64. Disponível em dez idiomas: inglês, francês, alemão, italiano, espanhol, português (Portugal), grego, polaco, neerlandês e ucraniano.

<a id="privacy"></a>**Privacidade:** **Sem telemetria, monitorização de utilização ou publicidade.** As verificações de atualizações contactam o GitHub. As semanais são opcionais e estão desativadas por predefinição. A transferência e a instalação exigem confirmação. [Privacidade](user-guide.md#privacidade).

**Instalação:**

- **Instalação manual:** transferir e executar <a href="https://github.com/LeBenco/FileSpacer/releases/download/v1.3.2/FileSpacer-1.3.2-install.exe" id="last-release-url">o instalador da versão mais recente</a>.
- **WinGet:** `winget install --id Benco.FileSpacer --exact`
- **Chocolatey** (terminal como administrador): `choco install filespacer -y`

Os pacotes WinGet e Chocolatey aguardam aprovação nos respetivos catálogos.

**Documentação:**

[![Docs](https://img.shields.io/badge/Docs-Guia%20de%20utiliza%C3%A7%C3%A3o-blue)](user-guide.md)
[![Docs](https://img.shields.io/badge/Docs-Hist%C3%B3rico%20de%20vers%C3%B5es-orange)](CHANGELOG.md)

_O FileSpacer é uma derivação do [ChromaFiler](https://github.com/vanjac/chromafiler), criado originalmente por J. van’t Hoog. Consulte os [agradecimentos](#licença-e-agradecimentos) abaixo para mais informações._

## Funcionalidades

- **Uma janela por pasta**, reativada se a pasta já estiver aberta. A posição, o tamanho, as definições de apresentação e a disposição dos ícones são mantidos entre aberturas. [Navegação espacial](user-guide.md#navegação-entre-janelas).
- **Disponha os ícones livremente**, com posições guardadas e restauradas para cada pasta. [Disposição dos ícones](user-guide.md#disposição-dos-ícones).
- **Mova e redimensione as janelas** a partir de qualquer ponto da área de ficheiros, mantendo a seleção. Os gestos do rato são configuráveis, com Alt + arrastar com o botão esquerdo e Alt + arrastar com o botão direito por predefinição. [Gestos de janela](user-guide.md#movimento-e-redimensionamento).
- **Alterne entre pastas abertas** através do menu da barra de título, feche as janelas das pastas superiores ou reabra a última janela fechada com Ctrl+Shift+T. [Navegação entre janelas](user-guide.md#navegação-entre-janelas).
- **Bloqueie o estado guardado de uma janela**, mantendo a possibilidade de alterar temporariamente a posição, o tamanho e a apresentação. [Bloqueio do estado das janelas](user-guide.md#bloqueio-do-estado-guardado).
- **Filtre a pasta atual** e reutilize as pesquisas recentes. [Pesquisa e histórico](user-guide.md#pesquisa-e-histórico).
- **Conserve a seleção ao ativar uma janela** clicando no fundo. [Seleção e ativação](user-guide.md#seleção-e-ativação).
- **Reduza a sobreposição das janelas** ao abrir pastas sem posição guardada. [Posicionamento das janelas](user-guide.md#posicionamento-das-janelas).
- **Personalizar o menu da barra de título** com ações para janelas e disposições e guardar grupos de pastas abertas. [Personalização do menu](user-guide.md#personalizar-o-menu-da-barra-de-título) · [Grupos de pastas](user-guide.md#grupos-de-pastas).

## Integração

O FileSpacer pode ser definido como gestor de ficheiros predefinido na instalação ou mais tarde nas definições. Os ficheiros abrem na aplicação habitual do Windows. As pastas de rede seguem o funcionamento normal do Windows. [Integração com o Windows](user-guide.md#integração-com-o-windows).

Com o **Everything**, é possível iniciar pesquisas avançadas a partir do campo do FileSpacer, na pasta atual e nas subpastas ou em todos os ficheiros indexados. Os resultados abrem no Everything. [Integração e utilização](user-guide.md#everything).

Com o **Nilesoft Shell**, os menus do FileSpacer podem usar os seus temas e comandos personalizados. Recomenda-se adaptar a configuração para uma integração visual coerente. [Integração e configuração](user-guide.md#nilesoft-shell).

## Compilação

[![Publish FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

A compilação utiliza MSVC/NMAKE, Visual Studio 2022 Build Tools com ATL e um Windows SDK, Inkscape e ImageMagick. O instalador requer o NSIS. O SQLite está incluído. Consulte o [guia de compilação](../en/build.md).

Os testes de regressão combinam cenários PowerShell com ferramentas C++ e C# para verificações internas e testes reais da interface do Windows e da Shell. O [workflow de regressão do GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) executa esta suite da aplicação a pedido e automaticamente antes de uma publicação. Os mesmos casos podem ser executados com `tests.cmd` num ambiente de trabalho Windows local ou numa VM Hyper-V configurada. O [guia dos testes](../en/tests/regression/README.md) explica a seleção de casos, os pré-requisitos e os resultados.

## Licença e agradecimentos

### Direitos de autor e licença
O FileSpacer é distribuído ao abrigo da licença <img align="absbottom" src="https://img.shields.io/badge/Licen%C3%A7a-GPLv3-blue.svg" alt="Licença: GPL v3">.<br>
Copyright © 2026 Benco pelas alterações do FileSpacer. A licença completa encontra-se em [LICENSE](../../LICENSE) no código-fonte, ou em `LICENSE.txt` na pasta de instalação. O FileSpacer é fornecido sem garantia. A redistribuição e a modificação são permitidas nos termos dessa licença.

### Origem do projeto e agradecimentos
O FileSpacer é uma derivação do [ChromaFiler](https://github.com/vanjac/chromafiler), criado por J. van’t Hoog (vanjac). Muito obrigado ao vanjac por esta base! Código original do ChromaFiler © 2023 J. van’t Hoog.

### Avisos relativos a componentes de terceiros
Consulte os [avisos relativos a componentes de terceiros](THIRD_PARTY_NOTICES.md) e a [política de privacidade](privacy-policy.md).
