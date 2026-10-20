function Assert-SearchCount([IntPtr]$Window,[int]$Count) {
    $observed=@{Value=$null}
    try {
        Wait-Ui {
            $observed.Value=Use-UiView $Window {param($view) $view.Count}
            return $observed.Value -eq $Count
        } "search view count = $Count" 400
    } catch {
        $failure=$_
        try { Write-Host "DIAG: HWND=$Window, expected=$Count, observed=$($observed.Value), search='$([FileSpacerRegression.Native]::Text((Get-UiSearchEdit $Window)))', saved LiveNameSearch=$(Get-UiPreference 'LiveNameSearch')" }
        catch { Write-Warning ('Cannot collect search diagnostics: '+$_.Exception.Message) }
        throw $failure
    }
}
