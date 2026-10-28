# Histórico de versões

## Por publicar

## 1.2.2 — 2026-10-28

- Corrige a utilização do fornecedor de pastas personalizado na ordenação mista com ícones dispostos livremente. Usa o fornecedor nativo no posicionamento livre e muda de fornecedor ao confirmar uma disposição ordenada, conservando as posições guardadas dos ícones, a seleção e o foco.

## 1.2.1 — 2026-10-28

- Reduz a apresentação transitória de ícones genéricos ao abrir pastas com ordenação mista. Prepara os ícones visíveis na cache do Windows antes de mostrar a vista nativa, com um tempo de espera limitado para os manipuladores de ícones lentos.

## 1.2.0 — 2026-10-28

- Adiciona a ordenação mista opcional de pastas e ficheiros do sistema de ficheiros em Apresentação → Ficheiros, conservando a vista nativa do Windows e o estado guardado das pastas. A opção está desativada por predefinição.
- Adiciona traduções da interface para alemão, italiano, espanhol, português (Portugal) e grego, além de inglês e francês.
- Apresenta o título e o subtítulo do instalador como texto do Windows maior, adaptado ao DPI e com suavização ClearType.
- Amplia o guia online com capturas inglesas e francesas, demonstrações dos gestos de janela e comparações dos menus do Nilesoft Shell.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Pede confirmação do Windows apenas ao instalar ou desinstalar para todos os utilizadores e aplica as escolhas pessoais de integração ao utilizador que iniciou a instalação.
- Mostra o histórico completo de versões num painel Novidades com deslocamento, ao lado das opções de instalação no primeiro ecrã.
- Corrige a reposição das escolhas dos componentes durante as atualizações; restaura as seleções guardadas e conserva o gestor de ficheiros predefinido nas atualizações silenciosas.
- Usa a mesma barra lateral do FileSpacer, o logótipo grande e a paleta cinzento-claro e branca em todos os ecrãs de instalação e desinstalação. Dá ao instalador um ícone próprio, composto pelos mapas de bits nativos da aplicação e por uma seta de transferência otimizada separadamente para cada tamanho, conservando um rebordo amarelo fino à direita e em baixo da pasta.
- Corrige falhas ao encerrar causadas pela inicialização antecipada de dados de pastas locais ao thread em threads de trabalho tardias da Shell.
- Oferece Abrir no Explorador de Ficheiros do Windows nos menus de contexto das pastas quando o FileSpacer é predefinido, e Abrir no FileSpacer no caso contrário; atualiza os menus ao mudar a escolha, após a instalação e no arranque.
- Corrige a abertura de atalhos de servidores e partilhas de rede no Explorador de Ficheiros quando o FileSpacer é predefinido. Atualize o componente do menu de contexto das pastas e volte a selecionar o FileSpacer como predefinido para ativar a integração de rede.
- Aumenta o ícone da aplicação no separador Acerca de para 48 píxeis à escala de 100 %, com o texto ao lado.
- Mantém os títulos das janelas maximizadas totalmente visíveis e estende a barra de ferramentas por toda a largura disponível.
- Permite executar os testes de regressão num cliente Hyper-V configurado, compilando a aplicação e as ferramentas de teste no anfitrião.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Mostra uma pré-visualização dos modos ao passar o ponteiro pelo menu Ver, com reposição progressiva em caso de cancelamento e uma opção ativada por predefinição em Apresentação.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Substitui o ícone da aplicação e o logótipo do README por uma projeção de um tesserato, com uma proporção de 11/20 entre as arestas roxas e verdes do cubo, mantendo otimizados os tamanhos pequenos.
- Clarifica os rótulos ingleses e franceses das definições e agrupa as opções de bloqueio das janelas.
- Adiciona uma opção para ocultar o indicador de cadeado das janelas bloqueadas sem alterar o bloqueio do estado.
- Usa os fundos nativos do tema do Windows nas páginas de definições e nos controlos dos atalhos.
- Explica o posicionamento das janelas e o agrupamento na barra de tarefas com descrições normais do Windows junto às opções.
- Associa as definições à janela de pasta que as abriu e descarta as alterações pendentes quando esta é fechada. As operações de manutenção confirmadas fecham as definições sem aplicar alterações pendentes.
- Reorganiza o README em torno da navegação espacial e das funcionalidades, com referências de investigação, uma ligação à versão mais recente, guias de compilação e do Nilesoft Shell e documentação de utilização ampliada.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Minimiza a sobreposição de janelas novas com geometria exata, procurando primeiro espaço livre. Muda o nome da opção geral para «Minimizar a sobreposição de janelas».
- Oferece a escolha opcional do gestor de ficheiros predefinido nos instaladores inglês e francês, com instruções para a ativar ou reverter posteriormente nas definições de integração do FileSpacer.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Corrige as janelas de pastas que permaneciam abertas quando uma pasta superior era eliminada ou movida para a Reciclagem.
- Impede que as Listas de Atalhos proponham reabrir pastas que deixaram de estar disponíveis.
- Reduz a sobreposição ao posicionar janelas sem posição guardada, ignorando a janela de origem substituída durante a navegação espacial. Ativa este comportamento por predefinição com uma opção em Geral > Navegação de pastas.
- Move a manutenção do estado das pastas de Geral para Acerca de.
- Abre novas janelas com uma proporção equilibrada de 4:3 (580 × 435 a 96 DPI), conservando a altura existente e respeitando as dimensões configuradas e o tamanho guardado de cada pasta.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Corrige a criação involuntária de um atalho após Alt + duplo clique seguido de movimento do rato. Conserva as propriedades nativas, a seleção e o foco sem reproduzir uma pressão mantida de um botão.
- Corrige os testes dos tempos nativos do duplo clique e dos alvos físicos do rato; compara propriedades, seleção, foco, âncora da seleção, geometria e conteúdo das pastas com e sem gestos de janela, incluindo combinações Ctrl/Shift.
- Isola o executável real de teste do Everything e o respetivo INI de arranque para que a definição de administrador instalada pelo utilizador não possa redirecionar pesquisas de teste nem pedir elevação de privilégios. Verifica que as definições instaladas ficam inalteradas.
- Mantém o ambiente de trabalho ativo e suspende temporariamente a proteção de ecrã durante os testes de interface, repondo o estado inicial na limpeza, mesmo após falhas nos testes.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

Esta versão baseia-se na 1.1.3, que já oferece gestos configuráveis de teclado e rato para mover e redimensionar janelas.

- Adiciona a pesquisa externa opcional com o Everything: deteção automática, caminho manual do executável, âmbito limitado à pasta e subpastas ou em todo o lado e erros de arranque explícitos.
- Mostra automaticamente o Everything e as pesquisas recentes ao ativar o campo existente, sem um botão adicional. Conserva a pesquisa nativa, a filtragem imediata e a reutilização de consultas locais.
- Memoriza por predefinição cinco pesquisas recentes por pasta entre sessões, com um limite de zero a vinte independente do Everything. Conserva os dados existentes e limpa o histórico segundo a política prudente do estado das pastas.
- Mostra o ícone do executável do Everything no tamanho adequado, com extração em segundo plano e uma cache em memória atualizada quando o executável muda.
- Adiciona testes de regressão de armazenamento isolado, migração e integração real Shell/Everything; torna o leitor de testes da Shell robusto perante alterações da vista por filtros sucessivos.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Adiciona o separador Atalhos às definições para configurar ou desativar os gestos de movimento e redimensionamento com teclas modificadoras e os botões esquerdo, central ou direito, com captura direta e validação de conflitos.
- Deixa a gestão de Alt + duplo clique nas propriedades à vista nativa do Explorador.
- Restaura os nomes completos sobre fundo opaco para um único elemento selecionado no modo Ícones. As seleções múltiplas conservam a apresentação nativa do Windows; o texto da preferência esclarece agora o seu âmbito.
- Adiciona testes exaustivos dos atalhos e dos gestos reais do rato, com recursos de definições franceses e ingleses.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Adiciona o bloqueio opcional da geometria e da apresentação guardadas de uma janela através do menu do título. Permite alterações temporárias e continua a guardar normalmente as posições individuais dos ícones.
- Mostra um indicador de cadeado Small & Flat nos ícones de pastas bloqueadas, com ICO nativos de várias resoluções, adaptação ao DPI e cores de alto contraste.
- Confirma a desativação global do bloqueio, remove os bloqueios guardados de pastas abertas e fechadas e retoma o armazenamento normal entre processos.
- Amplia os testes de regressão às transições do bloqueio, falhas de persistência, restauro da Shell e apresentação nativa do indicador.
- Reabrir uma pasta fechada com Ctrl+Shift+T ou as Listas de Atalhos volta a colocar no topo dos modos Detalhes e Lista a linha que aí se encontrava ao fechar.
- O diagnóstico de falhas continua a escrever um minidump e um relatório nos dados locais da aplicação; remove a atribuição à antiga apresentação de nomes completos, a alteração automática da respetiva preferência e o reinício.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Restaura a ativação que conserva a seleção: o primeiro clique no fundo vazio de uma pasta inativa ativa a janela sem limpar a seleção. A preferência volta a estar ativada.
- Corrige uma falha ao fechar uma janela de pasta enquanto a vista da Shell é atualizada, com Atualizar, filtro por nome ou comandos da Shell Atualizar/Novo.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Reorganiza as vistas das pastas para utilizar as implementações da Windows Shell através das API COM públicas, em vez de adaptar os controlos internos da Shell.
- Mantém o estado espacial das janelas, a seleção e o foco, as posições dos ícones e a filtragem associados ao armazenamento SQLite de produção e aos eventos públicos da Shell.
- Conserva os comandos nativos dos ficheiros e os gestos Alt do FileSpacer para mover, redimensionar e abrir propriedades.
- Abre o Painel de Controlo numa janela própria do Windows, conservando a pasta de origem e a seleção.
- Remove a seleção automática da pasta de origem ao subir a uma pasta superior por motivos ergonómicos: seleções posteriores com Ctrl não devem abrir ambas as pastas por engano.
- Aceita três diferenças nesta versão: não estão disponíveis a apresentação personalizada de nomes selecionados completos sobre fundo opaco, a conservação da seleção na ativação nem a reposição exata do deslocamento. Desativa os dois controlos de definições inativos, mantendo as preferências guardadas.
- Simplifica os testes de regressão em torno da configuração nativa de produção da Shell; retira comparações exploratórias de perfis, sondas de incorporação e relatórios reservados à migração.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Move janelas com Alt + arrastar com o botão esquerdo e redimensiona-as com Alt + arrastar com o botão direito a partir da área principal de ficheiros, conservando a seleção.
- Mantém ativa a vista clássica da Shell independentemente da preferência de apresentação dos nomes completos dos elementos selecionados.
- Aplica o tema do Nilesoft Shell ao menu dos modos de apresentação.
- Evita demoras prolongadas ao encerrar causadas pelas caches passivas de ícones da Shell, conservando as referências de tempo de vida adquiridas pelos comandos da Shell invocados explicitamente.
- Isola o alojamento da vista da Shell e amplia os testes dos gestos Alt, reentrada dos menus e tempo de vida Shell/STA.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Seleciona a pasta subordinada de origem ao abrir uma pasta superior, conservando a seleção das janelas já abertas.
- Corrige a filtragem dos nomes ao submeter, alterar ou limpar uma consulta.
- Restaura o ícone nativo do Acesso rápido do Windows e centra os botões Acesso rápido e Configuração.
- Remove os diagnósticos temporários e usa Benco nos agradecimentos do projeto.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Restaura automaticamente as janelas de pastas abertas após uma atualização a partir da aplicação, conservando as posições, os tamanhos e as definições de apresentação guardados.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Procura atualizações de forma assíncrona na página Acerca de, com um botão de instalação e texto de estado adequados.
- Transfere as atualizações numa janela de progresso separada e cancelável, mantendo utilizáveis as janelas de pastas.
- Verifica o tamanho e o SHA-256 do instalador antes de fechar ordenadamente o FileSpacer e iniciar a instalação.
- Publica os metadados de transferência do instalador juntamente com cada versão.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Adiciona um comando ao menu do título para fechar as janelas das pastas superiores.
- Reabre pastas fechadas através do menu do título, das Listas de Atalhos ou de Ctrl+Shift+T, restaurando a apresentação, a seleção e a posição de deslocamento.
- Adiciona às Listas de Atalhos da barra de tarefas as pastas afixadas no Acesso rápido do Explorador.
- Mostra o comando de reabertura apenas se a última pasta fechada não estiver já aberta, com o nome e o ícone nativo.
- Exclui a janela atual da lista de janelas do menu do título.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Restaura as verificações manuais e as verificações semanais opcionais de atualizações através das versões GitHub do FileSpacer, desativadas por predefinição.

- Publica o manifesto de atualização juntamente com cada instalador.

- Conserva a gestão nativa da Shell de Alt + duplo clique e Alt + Enter nas propriedades das pastas.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Aplica os temas do Nilesoft Shell ao menu de pastas da barra de título independentemente da seleção dos ficheiros, mantendo os comandos nativos da Shell.
- Permite personalizações Nilesoft separadas para os menus da barra de título e da barra de ferramentas.
- Centra a seta do menu pendente da barra de título.
- Documenta a configuração do Nilesoft Shell, a aplicação de temas e a filtragem dos menus.
- Identifica Benco nos avisos de direitos de autor.
- Permite ao workflow de publicação substituir o instalador de uma versão existente.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Ativa os controlos nativos Minimizar e Maximizar e corrige os botões do título nas janelas maximizadas.
- Guarda e restaura separadamente o estado maximizado e a geometria normal da janela, com migração automática da base de dados.
- Usa as animações nativas da barra de tarefas e as propriedades de reinício específicas de cada pasta.
- Aplica as preferências de agrupamento da barra de tarefas às janelas novas e esclarece este comportamento nas definições.
- Permite eliminar pastas abertas e limpa o estado guardado correspondente.
- Impede que a aplicação e o assistente de recuperação bloqueiem pastas através da sua pasta de trabalho.
- Conserva os caminhos absolutos das pastas e o modo de testes de depuração durante a recuperação de falhas.
- Restaura imediatamente os nomes normais depois de retirar a seleção, quando a apresentação do nome completo está ativada.
- Liberta as interfaces da Shell em cache ao fechar as janelas de pastas e corrige o tratamento das notificações da Shell.
- Permite publicar versões manualmente e inclui o número da versão nos nomes dos instaladores.

[Todas as alterações](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Primeira versão do FileSpacer para Windows 10 ou posterior, x64, baseada no ChromaFiler.
- Oferece navegação espacial com uma janela por pasta e estado persistente de janelas, vistas, ícones e filtros guardado em SQLite.
- Acompanha as mudanças de localização e de nome das pastas e ativa a janela existente ao reabrir uma pasta.
- Fecha por predefinição a janela de origem ao abrir uma pasta ou a pasta superior, com comportamento configurável e Ctrl para o inverter.
- Adiciona um trilho de navegação, uma barra de caminho editável e um filtro de nomes com filtragem opcional enquanto escreve.
- Oferece elementos configuráveis da barra de ferramentas, uma barra de estado inferior e preferências de agrupamento na barra de tarefas.
- Adiciona a apresentação experimental opcional dos nomes completos dos elementos selecionados.
- Oferece interfaces inglesa e francesa, com o inglês como alternativa.
- Adiciona a recuperação de falhas e os comandos de manutenção para repor preferências ou o estado guardado das pastas.
- Remove a navegação em colunas Miller, o editor de texto integrado, as janelas de pré-visualização e o painel flutuante.
- Fornece scripts de compilação MSVC/NMAKE, um instalador NSIS, publicações GitHub automatizadas e documentação do projeto.

[Histórico da versão](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
