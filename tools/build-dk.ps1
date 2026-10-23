param(
    [ValidateSet('diagnostic', 'tracker')][string]$App = 'tracker',
    [ValidateSet('hardware', 'hardware-p1', 'software')][string]$Wiring = 'hardware',
    [switch]$ButtonSleep,
    [switch]$CurrentMeasurement
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\ncs-env.ps1"
$repo = (Resolve-Path "$PSScriptRoot\..").Path.Replace('\', '/')
$config = "$repo/config/nrf54l15dk_lsm6dsv"
$overlay = if ($App -eq 'tracker') { "$config/tracker.overlay" } else { "$config/wiring.overlay" }
if ($Wiring -eq 'hardware') { $overlay += ";$config/hardware.overlay" }
if ($Wiring -eq 'hardware-p1') { $overlay += ";$config/hardware-p1.overlay" }
$source = if ($App -eq 'tracker') { $repo } else { "$repo/validation/lsm6dsv" }
$build = "$repo/build-$App-$Wiring"
if ($ButtonSleep) {
    if ($App -ne 'tracker') { throw 'ButtonSleep requires the tracker application.' }
    $build += '-button-sleep'
}
if ($CurrentMeasurement) {
    if ($App -ne 'tracker') { throw 'CurrentMeasurement requires the tracker application.' }
    $build += '-current'
}
$options = @("-DDTC_OVERLAY_FILE=$overlay", "-DUSER_CACHE_DIR=$repo/build-cache")
if ($App -eq 'tracker') {
    $extraConf = "$config/tracker.conf"
    if ($ButtonSleep) { $extraConf += ";$config/button-sleep.conf" }
    if ($CurrentMeasurement) { $extraConf += ";$config/current-measurement.conf" }
    $options += "-DEXTRA_CONF_FILE=$extraConf"
}
# NCS 3.4.1 can generate malformed escaped crypto options during CMake
# regeneration on Windows. A pristine configure avoids those stale options.
$ErrorActionPreference = 'Continue' # Native tools write normal progress to stderr in Windows PowerShell.
& west build -p always --no-sysbuild -b nrf54l15dk/nrf54l15/cpuapp -d $build $source -- @options
exit $LASTEXITCODE
