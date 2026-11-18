# Compile RUINMATES Editor target. Close Unreal Editor / Live Coding first.
$ErrorActionPreference = "Stop"
$build = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$proj = "C:\Users\ASUS\Documents\Unreal Projects\RUINMATES\RUINMATES.uproject"
& $build RUINMATESEditor Win64 Development -Project="$proj" -WaitMutex
exit $LASTEXITCODE