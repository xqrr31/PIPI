param(
    [Parameter(Mandatory = $true)]
    [string]$Directory,

    [string]$GuiLibrary = ''
)

$resolved = [System.IO.Path]::GetFullPath($Directory).TrimEnd([char[]]@([char]92, [char]47))
$current = [Environment]::GetEnvironmentVariable('Path', 'User')
$parts = @()
if (-not [string]::IsNullOrWhiteSpace($current)) {
    $parts = @($current -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}
$alreadyPresent = $false
foreach ($part in $parts) {
    try {
        if ([System.IO.Path]::GetFullPath($part).TrimEnd([char[]]@([char]92, [char]47)) -ieq $resolved) {
            $alreadyPresent = $true
            break
        }
    } catch { }
}
if (-not $alreadyPresent) {
    $newPath = if ([string]::IsNullOrWhiteSpace($current)) { $resolved } else { $current.TrimEnd(';') + ';' + $resolved }
    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    Write-Output "Added to current-user PATH: $resolved"
} else {
    Write-Output "Already in current-user PATH: $resolved"
}

if (-not [string]::IsNullOrWhiteSpace($GuiLibrary)) {
    $existingGui = [Environment]::GetEnvironmentVariable('PIPI_GUI_LIBRARY', 'User')
    $processGui = [Environment]::GetEnvironmentVariable('PIPI_GUI_LIBRARY', 'Process')
    if ([string]::IsNullOrWhiteSpace($existingGui) -and [string]::IsNullOrWhiteSpace($processGui)) {
        [Environment]::SetEnvironmentVariable('PIPI_GUI_LIBRARY', $GuiLibrary, 'User')
        Write-Output "Set current-user PIPI_GUI_LIBRARY: $GuiLibrary"
    } else {
        $keptGui = if ([string]::IsNullOrWhiteSpace($existingGui)) { $processGui } else { $existingGui }
        Write-Output "Preserved existing PIPI_GUI_LIBRARY: $keptGui"
    }
}
