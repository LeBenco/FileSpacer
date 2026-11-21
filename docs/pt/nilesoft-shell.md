<a id="nilesoft-shell-configuration-reference"></a>

# Referência de configuração do Nilesoft Shell

Para uma apresentação geral, consulte [Nilesoft Shell no guia do utilizador](user-guide.md#nilesoft-shell).

Se utiliza o [Nilesoft Shell](https://nilesoft.org/), pode aplicar os seus temas e personalizações de menus também ao FileSpacer através de `shell.nss` e dos ficheiros de configuração importados. Para esta integração opcional, recomenda-se adaptar a configuração para que os menus se integrem visualmente no FileSpacer. Os exemplos seguintes mostram como adicionar, modificar ou remover entradas, definir condições para o FileSpacer e utilizar os cantos retos recomendados.

<a id="scope-system-menu-rules"></a>

## Limitar as regras dos menus do sistema

Para as regras destinadas ao Explorador de Ficheiros, ao ambiente de trabalho ou à barra de tarefas, acrescente `process.is_explorer &&` à condição `where` existente. Para manter o seu efeito nas outras aplicações e excluir o FileSpacer, utilize antes `process.name != 'FileSpacer' &&`. Aplique o filtro às regras genéricas `menu`, `item`, `modify` e `remove`, incluindo as dos ficheiros importados.

<a id="apply-themes-to-filespacer"></a>

## Aplicar temas ao FileSpacer

Permita o FileSpacer em `settings.exclude.where` e mantenha a importação do tema. Para a condição de exclusão habitual:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

As propriedades do tema também podem utilizar expressões específicas do processo. Por exemplo, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dentro de `theme { ... }` escolhe o modo escuro para o FileSpacer e a preferência do sistema nas restantes aplicações.

Depois de guardar um ficheiro NSS, **Ctrl + clique direito** no ambiente de trabalho ou na barra de tarefas força o Nilesoft a recarregar a configuração no Explorador, sem o reiniciar nem anular o registo da extensão. O Nilesoft também recarrega quando deteta o botão esquerdo premido ao abrir um menu; um botão do FileSpacer pode assim aplicar imediatamente as alterações. Isto depende do gesto de abertura, não de um mecanismo de recarregamento mais rápido no FileSpacer.

<a id="menu-opening-delay"></a>

## Atraso na abertura dos menus

Para eliminar o [atraso na abertura dos menus](https://nilesoft.org/docs/configuration/settings) do Nilesoft Shell, definir `showdelay = 0` no bloco `settings` existente de `shell.nss`. Para aplicar a alteração apenas ao FileSpacer, usar `showdelay = if(process.name == 'FileSpacer', 0, 200)`, substituindo `200` pelo atraso a manter nas outras aplicações, em milissegundos.

<a id="target-a-specific-menu"></a>

## Identificar um menu específico

Declare estas variáveis antes das importações que as utilizam:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Utilize as propriedades `where` e `type` correspondentes nas regras:

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu pendente da barra de título | `filespacer_title` | `'*'` |
| Barra de ferramentas **…** e **Ver** | `filespacer_toolbar` | `'*'` |
| Apenas **Ver** | `filespacer_view` | `'*'` |
| Apenas **…** da barra de ferramentas | `filespacer_more` | `'*'` |
| Ficheiros ou pastas selecionados | `filespacer_context` | `'file\|dir'` |
| Fundo da pasta | `filespacer_context` | `'back'` |

Utilize `mode='multiple'` para regras que devam permanecer disponíveis independentemente do estado da seleção. `process.name` não inclui `.exe`. `window.parent.name` identifica uma classe de janela.

`filespacer_toolbar` continua a abranger ambos os menus da barra de ferramentas. Utilize `filespacer_view` apenas para Ver ou `filespacer_more` apenas para **…**. O marcador interno `window.parent.title` é estável e não é traduzido. Para manter Terminal fora de Ver, acrescente `!filespacer_view &&` à condição `where` existente e coloque a condição original entre parênteses:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Identificar uma entrada de menu existente

O mecanismo geral `modify(find=...)` do Nilesoft identifica uma entrada pelo respetivo texto. Combine-o com `where` para limitar a alteração a um menu. `|` separa textos alternativos e `*` é um caráter universal:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copiar caminho' title='Copiar caminho da pasta')
```

Consulte a [referência de configuração do Nilesoft](https://nilesoft.org/docs/configuration/).

<a id="recommended-menu-appearance"></a>

## Aspeto recomendado dos menus

Recomendam-se cantos retos nos três menus: o menu pendente da barra de título, **…** e **Ver**, a condizer com os botões do FileSpacer. Acrescente esta definição ao bloco `theme { ... }` existente:

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

Se já utiliza um raio personalizado, substitua `default` pelo valor anterior.

<a id="use-filespacer-actions"></a>

## Usar as ações do FileSpacer

Usar [`window.command`](https://nilesoft.org/docs/functions/window) com um dos ID abaixo para adicionar uma ação do FileSpacer, mesmo que não conste da composição do seu menu. Limitar a regra ao menu da barra de título com a condição `filespacer_title` definida acima. Mantêm-se os requisitos habituais da ação: por exemplo, repor uma disposição bloqueada exige um bloqueio existente.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Repor a disposição bloqueada' cmd=window.command(1123))
```

| Ação | ID |
| --- | ---: |
| Repor a disposição bloqueada | 1123 |
| Substituir a disposição de referência | 1124 |
| Fechar janelas de subpastas | 1126 |
| Fechar as outras janelas | 1127 |
| Ajustar a janela ao conteúdo | 1128 |
| Guardar / abrir um grupo de pastas | 1129 |
