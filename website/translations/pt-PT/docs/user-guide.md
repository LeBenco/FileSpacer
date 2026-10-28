# Utilizar o FileSpacer

## Navegação entre janelas

Cada pasta tem uma janela que memoriza a posição, o tamanho e as definições de apresentação. Abrir novamente uma pasta já aberta ativa a respetiva janela. Mover ou mudar o nome de uma pasta conserva a disposição guardada.

- Abrir uma pasta ou a pasta superior fecha por predefinição a janela de origem. Mantenha Ctrl premido para inverter o comportamento uma vez, ou altere a predefinição em **Definições > Geral**. Abrir várias pastas selecionadas mantém a janela de origem aberta.
- O menu junto ao título da pasta permite alternar entre as pastas abertas. **Fechar janelas de pastas superiores** fecha as janelas abertas das pastas que a contêm, mantendo as restantes abertas.
- **Ctrl+Shift+T**, ou **Reabrir a última janela fechada** no menu da barra de título ou na Lista de Atalhos da barra de tarefas, reabre uma pasta fechada sem fechar a atual. A disposição e a seleção são restauradas; os modos Detalhes e Lista também recuperam a primeira linha visível. O comando da barra de tarefas permite recuperar sucessivamente fechos anteriores.
- A Lista de Atalhos da barra de tarefas também abre as pastas afixadas no Acesso rápido do Explorador de Ficheiros.

Subir à pasta superior não seleciona a pasta de onde veio. Uma pasta superior já aberta conserva a sua seleção. O Painel de Controlo abre numa janela própria do Windows.

![O menu do título da pasta com navegação entre janelas e bloqueio do estado guardado.](images/window-menu.png)

*O menu do título, aqui com o estado da janela bloqueado.*

## Apresentação e disposição

### Disposição dos ícones

Desative **Dispor ícones automaticamente** num modo de ícones para posicionar livremente ficheiros e subpastas. As posições são guardadas independentemente do tamanho e da posição da janela, mesmo quando o estado guardado está bloqueado.

### Opções de apresentação

A visibilidade da barra de ferramentas e da barra de estado pode ser alterada em **Definições**.

Em **Definições > Apresentação**, duas opções estão ativadas por predefinição:

- **Mostrar o nome completo do ficheiro selecionado** mantém legível o nome de um único ficheiro selecionado no modo Ícones. Não altera as seleções múltiplas nem os outros modos de apresentação.
- **Pré-visualizar modos de apresentação no menu Ver** mostra uma pré-visualização do modo sob o ponteiro. Clique para o manter; Esc ou sair da pré-visualização restaura a apresentação inicial. A pré-visualização não substitui a disposição guardada.

**Misturar pastas e ficheiros na ordenação**, em **Definições > Apresentação > Ficheiros**, está desativado por predefinição. Ative esta opção para ordenar pastas e ficheiros em conjunto, como no [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) e no [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Aplica-se aos modos Detalhes, Lista e Ícones dispostos automaticamente. As disposições livres são conservadas. As pastas virtuais e os arquivos comprimidos mantêm a ordenação do Windows.

![Uma janela com pastas e ficheiros de exemplo misturados por ordem alfabética.](images/mixed-sorting.png)

*As pastas e os ficheiros partilham a mesma ordem alfabética.*

### Posicionamento das janelas

**Definições > Geral > Colocar novas janelas em espaço livre** está ativado por predefinição. As pastas sem posição guardada abrem no espaço disponível; quando deixa de haver espaço livre, a sobreposição é reduzida. As posições guardadas têm prioridade. Desativar a opção utiliza o posicionamento predefinido do Windows sem mover as janelas existentes.

### Seleção e ativação

Com **Definições > Geral > Manter a seleção ao clicar numa janela inativa** ativado, clicar no fundo de uma pasta inativa ativa a respetiva janela sem limpar a seleção. Prima uma segunda vez para iniciar um retângulo de seleção. Os cliques nos elementos, os cliques com o botão direito e os gestos de janela mantêm o comportamento habitual.

## Gestão das janelas

### Movimento e redimensionamento

Arraste na área de ficheiros, sobre o fundo ou sobre um elemento, com estes gestos predefinidos:

| Ação | Atalho |
| --- | --- |
| Mover a janela | Alt + arrastar com o botão esquerdo |
| Redimensionar a partir do canto inferior direito | Alt + arrastar com o botão direito |

**Mover a janela — Alt + arrastar com o botão esquerdo**

![Movimento de uma janela de pasta mantendo Alt premido e arrastando com o botão esquerdo na área de ficheiros.](images/move-window.gif)

**Redimensionar a janela — Alt + arrastar com o botão direito**

![Redimensionamento de uma janela de pasta mantendo Alt premido e arrastando com o botão direito na área de ficheiros.](images/resize-window.gif)

A seleção é conservada. Esc cancela o gesto e restaura a posição e as dimensões iniciais. Os campos de texto, cabeçalhos, barras de deslocamento, barras de ferramentas e menus mantêm o comportamento normal. Alt + duplo clique num elemento abre as respetivas propriedades.

Em **Definições > Atalhos**, ative ou desative cada ação e expanda-a para alterar a combinação. A área de captura direta regista uma combinação de teclas e botão do rato. Os botões esquerdo e direito requerem Alt; o botão central também suporta combinações sem Alt. As teclas modificadoras têm de corresponder exatamente. Não é possível aplicar combinações reservadas ou duplicadas.

O botão de valor predefinido repõe uma ação. **Repor** recupera os últimos atalhos aplicados; **Cancelar** descarta as alterações pendentes. **Aplicar** ou **OK** disponibiliza as alterações nas janelas abertas do FileSpacer.

### Bloqueio do estado guardado

Ative **Definições > Geral > Ativar bloqueio do estado da janela** (desativado por predefinição) e escolha **Bloquear o estado da janela** no menu junto ao título da pasta. O bloqueio guarda a posição, o tamanho e as definições de apresentação. Um **indicador de cadeado sobre o ícone da pasta na barra de título**, junto ao nome, identifica um estado bloqueado.

<img src="images/locked-title.png" alt="Pormenor da barra de título: um cadeado está sobreposto ao ícone da pasta junto a Studio project." width="420">

*O indicador de cadeado identifica o estado guardado como bloqueado.*

Pode continuar a mover e redimensionar temporariamente a janela ou a mudar a apresentação. Reabrir a pasta restaura o estado bloqueado. As posições individuais dos ícones continuam a ser guardadas normalmente. **Desbloquear o estado da janela** mantém a disposição atual. Pode ocultar o indicador nas definições gerais sem remover o bloqueio.

Desativar a funcionalidade pede confirmação e remove todos os bloqueios guardados, incluindo os de pastas fechadas. Voltar a ativá-la não os restaura.

## Pesquisa e histórico

Escreva no campo de pesquisa e prima Enter ou clique na lupa para filtrar a pasta atual. **Definições > Geral > Filtrar enquanto escreve** aplica o filtro durante a escrita. Este filtro local nunca inicia o Everything automaticamente.

Ativar o campo mostra as pesquisas recentes e, quando a integração está ativada, **Pesquisar no Everything**. Utilize as setas para cima/baixo e Enter, ou clique, para escolher uma entrada. Enter sem uma sugestão selecionada aplica o filtro local. Esc fecha a lista sem apagar o texto; Ctrl+F volta a ativar o campo.

O histórico conserva por predefinição cinco pesquisas distintas confirmadas por pasta. Pode alterar o limite de 0 a 20 nas definições gerais; 0 limpa e desativa o histórico. Escrever sem confirmar não adiciona entradas. Consulte [Everything](#everything) para pesquisar em subpastas ou em todos os ficheiros indexados.

![Sugestões de pesquisa com o Everything e consultas recentes sobre ficheiros de projeto fictícios.](images/search.png)

*O Everything é uma ação separada; as pesquisas recentes filtram a pasta atual.*

## Integração

### Integração com o Windows

O instalador oferece **Definir o FileSpacer como gestor de ficheiros predefinido**, desmarcado por predefinição. Instala também o componente do menu de contexto das pastas. Pode alterar a escolha posteriormente em **Definições > Integração** com **Definir como gestor de ficheiros predefinido**; **Repor o Explorador de Ficheiros predefinido** devolve a abertura das pastas ao Explorador de Ficheiros do Windows.

O componente do menu de contexto oferece **Abrir no Explorador de Ficheiros do Windows** quando o FileSpacer é o gestor predefinido, e **Abrir no FileSpacer** no caso contrário. Aplica-se a pastas, unidades, pastas comprimidas, localizações de rede e fundos de pastas. Instalar apenas este componente não altera a aplicação predefinida.

**Opções do Explorador de Ficheiros**, na mesma página de definições, abre as opções de pastas do Windows; algumas também afetam a apresentação dos ficheiros no FileSpacer.

### Everything

Instale o [Everything](https://www.voidtools.com/) separadamente e ative-o em **Definições > Integração**. Deixe vazio o caminho do executável para deteção automática, utilize **Detetar** ou escolha manualmente o ficheiro `.exe` de uma instalação portátil.

Escolha o âmbito da pesquisa: a pasta atual e as subpastas, ou **Em todo o lado**. As pastas virtuais não têm um âmbito no sistema de ficheiros; utilize Em todo o lado para essas pastas.

Introduza uma consulta no campo de pesquisa do FileSpacer e escolha **Pesquisar no Everything** nas sugestões. Os resultados abrem no Everything, com o respetivo índice e sintaxe de pesquisa. Se o Everything for iniciado com êxito, o FileSpacer limpa o campo e remove o filtro local; se ocorrer um erro, conserva ambos para poder tentar novamente.

### Nilesoft Shell

O [Nilesoft Shell](https://nilesoft.org/) permite alterar o aspeto dos menus do FileSpacer e personalizar as suas entradas. É opcional e instala-se separadamente.

Permita o FileSpacer na regra de exclusão da configuração do Nilesoft e mantenha a importação do tema. Restrinja ao Explorador as regras que se destinam apenas a ele, para que não alterem também os menus do FileSpacer.

Consulte a [referência de configuração e os exemplos](nilesoft-shell.md) para as definições exatas, as regras específicas de cada menu e o aspeto recomendado. Depois de alterar a configuração, anule o registo do Nilesoft Shell e volte a registá-lo para aplicar a alteração aos menus abertos a seguir.

O tema Modern apresentado abaixo usa os cantos arredondados predefinidos à esquerda e os cantos retos recomendados à direita.

**Menu de navegação**

| Cantos predefinidos | Cantos retos recomendados |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu de navegação da barra de título com os cantos arredondados predefinidos do tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="O mesmo menu de navegação com os cantos retos recomendados." width="320"> |

**Menu principal (…)**

| Cantos predefinidos | Cantos retos recomendados |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principal com os cantos arredondados predefinidos do tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="O mesmo menu principal com os cantos retos recomendados." width="320"> |

## Definições e atualizações

**Preferências.** **Ctrl+,** abre as definições, mesmo com a barra de ferramentas oculta. A interface segue o idioma de apresentação do Windows, com o inglês como alternativa; mudar de idioma exige reiniciar a aplicação.

**Atualizações.** Procure uma nova versão em **Definições > Acerca de** e escolha **Instalar o FileSpacer vX.Y.Z** para atualizar. A instalação requer confirmação; as atualizações nunca são instaladas automaticamente. Depois da instalação, o FileSpacer reabre as pastas anteriormente abertas com as disposições guardadas. As verificações semanais opcionais estão desativadas por predefinição e podem ser ativadas em Acerca de.

**Manutenção.** **Definições > Acerca de > Manutenção > Repor o estado de todas as pastas** fecha as janelas de pastas abertas e limpa as disposições guardadas, o histórico e os bloqueios de estado. Os seus ficheiros e as preferências globais são conservados.

**Documentação.** Escolha **Documentação** acima de **Definições do FileSpacer** no menu principal, prima **F1** ou utilize o botão em **Definições > Acerca de**. O guia instalado abre no navegador no idioma de apresentação do Windows, com o inglês como alternativa. As páginas, imagens e pesquisas funcionam sem ligação à Internet; o seletor de idioma permite mudar de tradução.
