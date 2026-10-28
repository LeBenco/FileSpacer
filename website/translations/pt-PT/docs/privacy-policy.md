# Política de privacidade

O FileSpacer não inclui telemetria, análise de utilização nem publicidade. Não envia automaticamente ao programador informações de utilização ou relatórios de falhas.

## Verificação de atualizações

As verificações manuais e as verificações semanais automáticas, quando ativadas, pedem ao GitHub o manifesto da versão mais recente do FileSpacer através de HTTPS. As verificações automáticas estão desativadas por predefinição. O GitHub e a sua infraestrutura de transferência recebem as informações habituais de ligação, incluindo o seu endereço IP e um agente de utilizador com a versão do FileSpacer. O pedido não contém caminhos de ficheiros, estados de pastas ou relatórios de diagnóstico.

Quando existe uma atualização instalável, o FileSpacer pede confirmação antes de transferir o instalador do GitHub através de HTTPS. Verifica o tamanho e o SHA-256 da transferência face ao manifesto da versão antes de iniciar a instalação. O instalador requer a sua intervenção; as atualizações nunca são instaladas automaticamente. As versões sem metadados de instalação abrem a respetiva página no navegador predefinido.

## Dados locais

As preferências globais são guardadas no Registo do Windows do utilizador atual, em `HKEY_CURRENT_USER\Software\FileSpacer`. As posições e os tamanhos das janelas de pastas, as definições de apresentação, as posições dos ícones, os bloqueios de estado, as pesquisas recentes e o histórico de pastas fechadas são guardados numa base de dados SQLite em `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. O SQLite pode criar ficheiros de diário associados na mesma pasta. O estado guardado pode incluir identidades e caminhos de pastas, incluindo caminhos de rede.

Uma atualização guarda temporariamente o instalador, uma cópia do iniciador da atualização e uma lista SQLite dos caminhos de pastas abertas em `%TEMP%\FileSpacerUpdate-{GUID}`. Esta lista local é utilizada para reabrir as pastas depois da instalação ou do cancelamento; não é enviada.

A proteção contra falhas pode guardar relatórios de diagnóstico e minidumps em `%LOCALAPPDATA%\FileSpacer`. Permanecem no computador, salvo se o utilizador decidir partilhá-los. O diagnóstico pode conter caminhos de ficheiros, informações sobre processos e partes da memória dos processos. A mesma pasta contém um ficheiro de bloqueio das definições.

As compilações de depuração podem usar preferências e dados locais de teste separados com `/test`.

## Windows e aplicações externas

Abrir pastas de rede, resolver elementos da Shell, apresentar pré-visualizações ou miniaturas fornecidas pelo Windows e utilizar extensões de menus de contexto recorre ao Windows e aos componentes da Shell instalados. Estes componentes e os servidores remotos podem aceder à rede segundo o seu próprio funcionamento e as respetivas políticas.

Os ficheiros abrem com a aplicação associada ou com o seletor de aplicações do Windows. O tratamento dos ficheiros por essas aplicações está fora do controlo do FileSpacer.

Se a integração com o Everything estiver ativada, escolher a sua ação de pesquisa transmite a consulta visível à aplicação Everything instalada separadamente. As pesquisas limitadas à pasta também transmitem o caminho atual. O filtro local não inicia o Everything; o índice e as definições da aplicação externa determinam os resultados.

## Reposição das preferências

`FileSpacer.exe /reset-options` repõe as preferências globais. `FileSpacer.exe /reset-all` limpa também o estado guardado das pastas. Nenhum dos comandos elimina os ficheiros do utilizador nem altera as associações de ficheiros do Windows. Não removem os relatórios de diagnóstico guardados anteriormente.
