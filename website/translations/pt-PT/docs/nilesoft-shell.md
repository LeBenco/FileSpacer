# Referência de configuração do Nilesoft Shell

Para uma apresentação geral, consulte [Nilesoft Shell no guia do utilizador](user-guide.md#nilesoft-shell).

Se utiliza o [Nilesoft Shell](https://nilesoft.org/), pode aplicar os seus temas e personalizações de menus também ao FileSpacer através de `shell.nss` e dos ficheiros de configuração importados. Para esta integração opcional, recomenda-se adaptar a configuração para que os menus se integrem visualmente no FileSpacer. Os exemplos seguintes mostram como adicionar, modificar ou remover entradas, definir condições para o FileSpacer e utilizar os cantos retos recomendados.

## Limitar as regras dos menus do sistema

Para as regras destinadas ao Explorador de Ficheiros, ao ambiente de trabalho ou à barra de tarefas, acrescente `process.is_explorer &&` à condição `where` existente. Para manter o seu efeito nas outras aplicações e excluir o FileSpacer, utilize antes `process.name != 'FileSpacer' &&`. Aplique o filtro às regras genéricas `menu`, `item`, `modify` e `remove`, incluindo as dos ficheiros importados.

## Aplicar temas ao FileSpacer

Permita o FileSpacer em `settings.exclude.where` e mantenha a importação do tema. Para a condição de exclusão habitual:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

As propriedades do tema também podem utilizar expressões específicas do processo. Por exemplo, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dentro de `theme { ... }` escolhe o modo escuro para o FileSpacer e a preferência do sistema nas restantes aplicações.

Com o FileSpacer, as alterações do tema aplicam-se aos novos menus depois de anular o registo do Nilesoft Shell e voltar a registá-lo, sem reiniciar o Explorador.

## Identificar um menu específico

Declare estas variáveis antes das importações que as utilizam:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Utilize as propriedades `where` e `type` correspondentes nas regras:

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu pendente da barra de título | `filespacer_title` | `'*'` |
| Barra de ferramentas **…** | `filespacer_toolbar` | `'*'` |
| Ficheiros ou pastas selecionados | `filespacer_context` | `'file\|dir'` |
| Fundo da pasta | `filespacer_context` | `'back'` |

Utilize `mode='multiple'` para regras que devam permanecer disponíveis independentemente do estado da seleção. `process.name` não inclui `.exe`; `window.parent.name` identifica uma classe de janela.

## Identificar uma entrada de menu existente

O mecanismo geral `modify(find=...)` do Nilesoft identifica uma entrada pelo respetivo texto. Combine-o com `where` para limitar a alteração a um menu; `|` separa textos alternativos e `*` é um caráter universal:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copiar caminho' title='Copiar caminho da pasta')
```

Consulte a [referência de configuração do Nilesoft](https://nilesoft.org/docs/configuration/).

## Aspeto recomendado dos menus

Recomendam-se cantos retos nos menus da barra de título e da barra de ferramentas, a condizer com os botões do FileSpacer. Acrescente esta definição ao bloco `theme { ... }` existente:

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
