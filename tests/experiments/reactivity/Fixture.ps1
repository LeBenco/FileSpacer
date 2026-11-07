function New-ReactivityFixture([string]$Root,[int]$Count=5000) {
    $folder=Join-Path ([IO.Path]::GetFullPath($Root)) ('build\reactivity-fixture-'+[Guid]::NewGuid().ToString('N'))
    if(Test-Path -LiteralPath $folder){throw 'The unique fixture directory already exists.'}
    [void][IO.Directory]::CreateDirectory($folder)
    for($i=0;$i-lt$Count;++$i){[IO.File]::WriteAllText((Join-Path $folder ('item-'+(100000+$i)+'.txt')),('x'*(1+$i%127)))}
    return $folder
}
function Remove-ReactivityFixture([string]$Root,[string]$Folder) {
    $allowed=[IO.Path]::GetFullPath((Join-Path $Root 'build')).TrimEnd('\')+'\'
    $target=[IO.Path]::GetFullPath($Folder)
    if(!$target.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)-or ![IO.Path]::GetFileName($target).StartsWith('reactivity-fixture-',[StringComparison]::Ordinal)){
        throw 'Fixture cleanup target is outside the expected project build directory.'
    }
    if(Test-Path -LiteralPath $target){
        $item=Get-Item -LiteralPath $target
        if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Fixture cleanup refuses a reparse point.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
