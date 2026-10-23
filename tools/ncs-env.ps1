param([string]$Sdk = 'C:\ncs\v3.4.1', [string]$Toolchain = 'C:\ncs\toolchains\4f5b6ad6dd')
$envSpec = Get-Content -LiteralPath "$Toolchain\environment.json" -Raw | ConvertFrom-Json
foreach ($entry in $envSpec.env_vars) {
    $value = if ($entry.type -eq 'relative_paths') {
        ($entry.values | ForEach-Object { Join-Path $Toolchain $_ }) -join ';'
    } else { $entry.value }
    if ($entry.existing_value_treatment -eq 'prepend_to') {
        $value += ';' + [Environment]::GetEnvironmentVariable($entry.key, 'Process')
    }
    [Environment]::SetEnvironmentVariable($entry.key, $value, 'Process')
}
$env:ZEPHYR_BASE = "$Sdk\zephyr"
# This checkout can be owned by the Administrators group. Limit the exception to
# this process and this exact repository, including CMake's child git commands.
$env:GIT_CONFIG_COUNT = '2'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = (Resolve-Path "$PSScriptRoot\..").Path.Replace('\', '/')
$env:GIT_CONFIG_KEY_1 = 'safe.directory'
$env:GIT_CONFIG_VALUE_1 = $Sdk.Replace('\', '/') + '/*'
