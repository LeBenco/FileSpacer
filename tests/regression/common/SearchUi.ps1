function Assert-SearchNames([IntPtr]$Window,[string[]]$Names) {
    [string[]]$orderedNames=@($Names)
    [Array]::Sort($orderedNames,[StringComparer]::OrdinalIgnoreCase)
    $expected=$orderedNames -join ';'
    $observed=@{Names=''}
    try {
        Wait-Ui {
            $observed.Names=(Use-UiView $Window {param($view) $view.Names()}) -join ';'
            return $observed.Names -ceq $expected
        } ('visible search items: '+$expected) 400
    } catch {
        Write-Host ('DIAG: expected=['+$expected+'], observed=['+$observed.Names+']')
        throw
    }
}
