<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

O FileSpacer é um gestor de ficheiros para Windows baseado no [paradigma espacial](https://en.wikipedia.org/wiki/Spatial_file_manager): cada pasta tem uma única janela que memoriza a posição, o tamanho e as definições de apresentação. Isto cria [referências visuais estáveis](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), permitindo que a [memória espacial](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) ajude a reconhecer as pastas familiares de relance. Num gestor de ficheiros baseado na navegação, a mesma janela mostra sucessivamente pastas diferentes: a posição e as dimensões, por si só, não identificam a pasta aberta, pelo que é preciso olhar para o conteúdo para saber onde se está. Ao conservar referências distintas para cada pasta, a abordagem espacial pode reduzir o esforço necessário para se orientar entre os seus ficheiros.

Para limitar a acumulação de janelas, uma desvantagem conhecida do paradigma espacial, o FileSpacer fecha por predefinição a janela de origem ao abrir uma pasta. Mantenha Ctrl premido durante a abertura para a conservar quando quiser trabalhar com várias pastas ao mesmo tempo.

O FileSpacer mantém uma interface deliberadamente sóbria, ao contrário de certas interfaces sobrecarregadas que mal deixam espaço entre os botões para se vislumbrar um ficheiro.

Os separadores são incompatíveis com o modelo espacial e nunca serão adicionados.

Windows 10 ou posterior, x64. Disponível em inglês, francês, alemão, italiano, espanhol, português (Portugal) e grego.

[Transferir a versão mais recente](https://github.com/LeBenco/FileSpacer/releases/latest)
· [Guia de utilização](docs/user-guide.md)
· [Histórico de versões](CHANGELOG.md)

![Uma janela do FileSpacer com ficheiros e pastas de exemplo dispostos livremente.](docs/images/overview.png)

*Cada pasta conserva a sua janela e a disposição dos ícones.*

## Funcionalidades

- **Disponha os ícones livremente**, com posições guardadas e restauradas para cada pasta. [Disposição dos ícones](docs/user-guide.md#icon-layout).
- **Mova e redimensione as janelas** a partir de qualquer ponto da área de ficheiros, mantendo a seleção. Os gestos do rato são configuráveis; por predefinição, Alt + arrastar com o botão esquerdo e Alt + arrastar com o botão direito. [Gestos de janela](docs/user-guide.md#moving-and-resizing).
- **Alterne entre pastas abertas** através do menu da barra de título, feche as janelas das pastas superiores ou reabra a última janela fechada com Ctrl+Shift+T. [Navegação entre janelas](docs/user-guide.md#window-navigation).
- **Bloqueie o estado guardado de uma janela**, mantendo a possibilidade de alterar temporariamente a posição, o tamanho e a apresentação. [Bloqueio do estado das janelas](docs/user-guide.md#locking-a-saved-layout).
- **Filtre a pasta atual** e reutilize as pesquisas recentes. [Pesquisa e histórico](docs/user-guide.md#search-and-history).
- **Conserve a seleção ao ativar uma janela** clicando no fundo. [Seleção e ativação](docs/user-guide.md#selection-and-activation).
- **Reduza a sobreposição das janelas** ao abrir pastas sem posição guardada. [Posicionamento das janelas](docs/user-guide.md#window-placement).

## Integração

Se utiliza o **Nilesoft Shell**, pode também usá-lo para alterar o aspeto dos menus do FileSpacer e personalizar os seus comandos, com regras específicas para cada menu. Recomenda-se adaptar a configuração para uma boa integração visual. [Integração e configuração](docs/user-guide.md#nilesoft-shell).

O **Everything** pode pesquisar na pasta atual e nas subpastas, ou em todo o lado, usando a consulta introduzida no FileSpacer. Os resultados abrem no Everything, que tem de ser instalado separadamente. [Instalação e utilização](docs/user-guide.md#everything).

Também pode definir o FileSpacer como aplicação predefinida para abrir pastas. [Integração com o Windows](docs/user-guide.md#windows-integration).

## Compilação

A compilação utiliza MSVC/NMAKE, Visual Studio 2022 Build Tools com ATL e um Windows SDK, Inkscape e ImageMagick. O instalador requer o NSIS; o SQLite está incluído. Consulte o [guia de compilação](docs/build.md).

Os testes de regressão combinam cenários PowerShell com ferramentas C++ e C# para verificações internas e testes reais da interface do Windows e da Shell. Execute-os com `tests.cmd`, localmente ou numa VM Hyper-V configurada. Consulte o [guia dos testes](tests/regression/README.md).

---

O FileSpacer é uma derivação do [ChromaFiler](https://github.com/vanjac/chromafiler), criado por J. van’t Hoog. Muito obrigado ao vanjac por esta base!

Distribuído ao abrigo da [GNU GPL v3.0](LICENSE). Copyright © 2026 Benco pelas alterações do FileSpacer; código original do ChromaFiler © 2023 J. van’t Hoog. Consulte os [avisos relativos a componentes de terceiros](THIRD_PARTY_NOTICES.md) e a [política de privacidade](docs/privacy-policy.md).
