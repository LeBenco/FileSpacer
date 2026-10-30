# Utilizar o FileSpacer

## Navegação entre janelas

Cada pasta tem uma janela que memoriza a posição, o tamanho e as definições de apresentação. Abrir novamente uma pasta já aberta ativa a respetiva janela. Mover ou mudar o nome de uma pasta conserva a disposição guardada.

- Abrir uma pasta ou a pasta superior fecha por predefinição a janela de origem. Mantenha Ctrl premido para inverter o comportamento uma vez, ou altere a predefinição em **Definições > Geral**. Abrir várias pastas selecionadas mantém a janela de origem aberta.
- O menu junto ao título da pasta permite alternar entre as pastas abertas. **Fechar janelas de pastas superiores** fecha as janelas abertas das pastas que a contêm, mantendo as restantes abertas.
- **Ctrl+Shift+T**, ou **Reabrir a última janela fechada** no menu da barra de título ou na Lista de Atalhos da barra de tarefas, reabre uma pasta fechada sem fechar a atual. A disposição e a seleção são restauradas; os modos Detalhes e Lista também recuperam a primeira linha visível. O comando da barra de tarefas permite recuperar sucessivamente fechos anteriores.
- A Lista de Atalhos da barra de tarefas também abre as pastas afixadas no Acesso rápido do Explorador de Ficheiros.

Subir à pasta superior não seleciona a pasta de onde veio. Uma pasta superior já aberta conserva a sua seleção. O Painel de Controlo abre numa janela própria do Windows.

![O menu do título da pasta com navegação entre janelas e bloqueio do estado guardado.](images/window-menu.png)

## Apresentação e disposição

### Disposição dos ícones

Num modo de ícones, pode posicionar livremente ficheiros e subpastas quando **Dispor ícones automaticamente** está desativado. As posições são guardadas independentemente do tamanho e da posição da janela, mesmo quando o estado guardado está bloqueado.

### Opções de apresentação

**Mostrar o nome completo do ficheiro selecionado**, em **Definições > Apresentação**, torna totalmente legível um único nome selecionado na vista de ícones. Está ativo por predefinição e não altera seleções múltiplas nem outras vistas.

![Um nome de ficheiro comprido, selecionado e apresentado na íntegra na vista de ícones.](images/selected-name.png)

**Pré-visualizar modos de apresentação no menu Ver**, também ativo por predefinição, mostra cada modo ao passar o ponteiro. Um clique confirma-o; Esc ou sair da pré-visualização repõe a vista inicial. A pré-visualização nunca substitui a disposição guardada.

**Misturar pastas e ficheiros na ordenação** ordena-os em conjunto, como no [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) e no [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Está desativado por predefinição em **Definições > Apresentação > Ficheiros**. Aplica-se a Detalhes, Lista e ícones dispostos automaticamente; as disposições livres são preservadas. Pastas virtuais e ficheiros comprimidos mantêm a ordenação do Windows.

![Pastas e ficheiros na mesma ordem alfabética.](images/mixed-sorting.png)

A visibilidade das barras de ferramentas e de estado também se altera nas definições.

### Posicionamento das janelas

**Definições > Geral > Colocar novas janelas em espaço livre** está ativado por predefinição. As pastas sem posição guardada abrem no espaço disponível; quando deixa de haver espaço livre, a sobreposição é reduzida. As posições guardadas têm prioridade. Desativar a opção utiliza o posicionamento predefinido do Windows sem mover as janelas existentes.

### Seleção e ativação

Com **Definições > Geral > Manter a seleção ao clicar numa janela inativa** ativado, clicar no fundo de uma pasta inativa ativa a respetiva janela sem limpar a seleção. Prima uma segunda vez para iniciar um retângulo de seleção. Os cliques nos elementos, os cliques com o botão direito e os gestos de janela mantêm o comportamento habitual.

## Gestão das janelas

### Movimento e redimensionamento

É possível mover ou redimensionar uma janela arrastando a partir da área de ficheiros, incluindo sobre um elemento. Os atalhos predefinidos são:

| Ação | Atalho |
| --- | --- |
| Mover a janela | Alt + arrastar com o botão esquerdo |
| Redimensionar a partir do canto inferior direito | Alt + arrastar com o botão direito |

![Mover uma janela com Alt e o botão esquerdo.](images/move-window.gif)

![Redimensionar uma janela com Alt e o botão direito.](images/resize-window.gif)

A seleção é preservada. Esc cancela o gesto. Campos de texto, cabeçalhos, barras de deslocamento, barras de ferramentas e menus mantêm o funcionamento normal; Alt + duplo clique abre as propriedades de um elemento.

Cada ação tem o seu atalho configurável em **Definições > Atalhos**. Os botões esquerdo e direito exigem Alt; o botão do meio também funciona sem Alt. Os modificadores têm de corresponder exatamente; combinações reservadas ou repetidas são recusadas. As alterações aplicam-se às janelas abertas. Repor regressa aos últimos atalhos aplicados e Cancelar descarta alterações pendentes.

### Bloqueio do estado guardado

O bloqueio guarda a posição, o tamanho e a vista de uma pasta para a próxima abertura. Está desativado por predefinição em **Definições > Geral > Ativar bloqueio do estado da janela**. Depois de ativado, **Bloquear o estado da janela** fica disponível junto ao título da pasta. Um cadeado sobre o ícone da pasta assinala o bloqueio.

<img src="images/locked-title.png" alt="Pormenor do título da pasta com o indicador de cadeado." width="280">

Continuam a ser possíveis alterações temporárias; ao reabrir a pasta, regressa o estado bloqueado. As posições individuais dos ícones continuam a ser guardadas. **Desbloquear o estado da janela** mantém a disposição atual. Ocultar o cadeado não remove o bloqueio.

Desativar a função pede confirmação e remove todos os bloqueios, incluindo os das pastas fechadas. Voltar a ativá-la não os repõe.

## Pesquisa e histórico

Introduza todo ou parte do nome de um ficheiro ou de uma pasta e prima Enter ou clique na lupa para filtrar a pasta atual. **Definições > Geral > Filtrar enquanto escreve** aplica o filtro durante a escrita. Este filtro local nunca inicia o Everything automaticamente.

Ativar o campo mostra as pesquisas recentes e, quando a integração está ativada, **Pesquisar no Everything**. Utilize as setas para cima/baixo e Enter, ou clique, para escolher uma entrada. Enter sem uma sugestão selecionada aplica o filtro local. Esc fecha a lista sem apagar o texto; Ctrl+F volta a ativar o campo.

O histórico conserva por predefinição cinco pesquisas distintas confirmadas por pasta. Pode alterar o limite de 0 a 20 nas definições gerais; 0 limpa e desativa o histórico. Escrever sem confirmar não adiciona entradas. Consulte [Everything](#everything) para pesquisar em subpastas ou em todos os ficheiros indexados.

![Sugestões de pesquisa com o Everything e consultas recentes sobre ficheiros de projeto fictícios.](images/search.png)

## Integração

### Integração com o Windows

O FileSpacer pode ser definido como gestor de ficheiros predefinido durante a instalação ou mais tarde, em **Definições > Integração**, com **Definir como gestor de ficheiros predefinido**. **Repor o Explorador de Ficheiros predefinido** devolve a abertura de pastas ao Explorador de Ficheiros.

O menu de contexto das pastas oferece **Abrir no FileSpacer**, ou **Abrir no Explorador de Ficheiros do Windows** quando o FileSpacer é o predefinido. Também funciona em unidades, pastas ZIP, localizações de rede e fundos de pastas, sem obrigar a mudar a aplicação predefinida.

Os ficheiros abrem na aplicação habitual do Windows. Pastas de rede, pré-visualizações e comandos acrescentados ao menu de contexto também seguem o funcionamento normal do Windows. **Opções do Explorador de Ficheiros** abre preferências de pastas partilhadas, algumas das quais também se aplicam ao FileSpacer.

### Everything

Uma instalação existente do [Everything](https://www.voidtools.com/) pode ser integrada para iniciar pesquisas avançadas a partir do campo do FileSpacer. Os resultados abrem no Everything.

A opção ativa-se em **Definições > Integração**. O Everything é encontrado automaticamente ou com **Detetar**; uma cópia portátil pode ser escolhida manualmente. A pesquisa abrange a pasta atual e as subpastas, ou **Em todo o lado**. As pastas virtuais exigem Em todo o lado.

**Pesquisar no Everything** envia a consulta introduzida, com a sintaxe do Everything. Se o arranque for bem-sucedido, o campo e o filtro local do FileSpacer são limpos; em caso de erro, são mantidos para nova tentativa.

![Uma pesquisa no FileSpacer e os resultados correspondentes no Everything, ao lado.](images/everything-search.png)

### Nilesoft Shell

O [Nilesoft Shell](https://nilesoft.org/) pode definir o aspeto dos menus do FileSpacer e personalizar os seus comandos. Recomenda-se adaptar a configuração para uma integração visual coerente, sobretudo com cantos retos nos menus da barra de título e da barra de ferramentas.

A [referência de configuração](nilesoft-shell.md) explica como permitir o FileSpacer, manter o tema e limitar ao Explorador as regras que lhe são específicas. Após uma alteração, anular e repetir o registo do Nilesoft Shell aplica-a aos menus abertos a partir daí.

O tema Modern abaixo compara os cantos predefinidos com os cantos retos recomendados.

**Menu de navegação**

| Cantos predefinidos | Cantos retos recomendados |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu de navegação da barra de título com os cantos arredondados predefinidos do tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="O mesmo menu de navegação com os cantos retos recomendados." width="320"> |

**Menu principal (…)**

| Cantos predefinidos | Cantos retos recomendados |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principal com os cantos arredondados predefinidos do tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="O mesmo menu principal com os cantos retos recomendados." width="320"> |

## Definições e atualizações

Ctrl+, abre as definições mesmo com a barra de ferramentas oculta. A interface segue o idioma de apresentação do Windows, recorrendo ao inglês quando necessário; mudar de idioma exige reiniciar. F1 abre o guia instalado, também disponível offline.

As atualizações procuram-se em **Definições > Acerca de**. A transferência e a instalação exigem confirmação; a instalação nunca é automática. No fim, as pastas anteriormente abertas regressam com as disposições guardadas. As verificações semanais são opcionais e estão desativadas por predefinição; ativam-se em Acerca de.

## Dados locais

Disposições de pastas, bloqueios, pesquisas e histórico de pastas fechadas são guardados localmente em `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. As preferências gerais estão no Registo do Windows, em `HKEY_CURRENT_USER\Software\FileSpacer`. O estado guardado pode conter caminhos de pastas, incluindo caminhos de rede. Os ficheiros temporários de atualização e a lista de pastas a reabrir permanecem em `%TEMP%\FileSpacerUpdate-{GUID}`.

**Definições > Acerca de > Manutenção > Repor o estado de todas as pastas** fecha as janelas das pastas e elimina as disposições, históricos e bloqueios guardados. Os ficheiros e as preferências gerais são preservados.

Para resolver problemas, `FileSpacer.exe /reset-options` repõe as preferências gerais; `/reset-all` elimina também o estado guardado das pastas. Nenhum dos comandos elimina ficheiros, altera associações do Windows ou remove relatórios de diagnóstico existentes.

## Privacidade

O FileSpacer não inclui telemetria, monitorização de utilização ou publicidade. Não envia automaticamente relatórios de falhas. Os eventuais relatórios de diagnóstico permanecem em `%LOCALAPPDATA%\FileSpacer`; a partilha é opcional e pode revelar caminhos de ficheiros ou partes da memória do processo.

As verificações de atualizações e as transferências contactam o GitHub por HTTPS. O GitHub recebe as informações habituais da ligação, incluindo o endereço IP e a versão da aplicação, mas nenhum estado de pasta, caminho de ficheiro ou relatório de diagnóstico.

As credenciais de acesso a unidades de rede são introduzidas e geridas pelo Windows, tal como no Explorador de Ficheiros. O FileSpacer não as lê nem guarda. Uma pesquisa no Everything transmite a consulta e, se limitada à pasta, o respetivo caminho à aplicação Everything local.
