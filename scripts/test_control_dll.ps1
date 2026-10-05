param(
  [Parameter(Mandatory = $true)]
  [string]$Path
)

$ErrorActionPreference = "Stop"
$resolved = (Resolve-Path -LiteralPath $Path).Path

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class SdrTownControlProbe
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryEx(string fileName, IntPtr file, uint flags);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr module, string name);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool FreeLibrary(IntPtr module);
}
"@

# LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS.
$flags = 0x00000100 -bor 0x00001000
$module = [SdrTownControlProbe]::LoadLibraryEx($resolved, [IntPtr]::Zero, $flags)
if ($module -eq [IntPtr]::Zero) {
  $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  $message = [ComponentModel.Win32Exception]::new($code).Message
  throw "LoadLibraryEx failed for '$resolved' ($code): $message"
}

try {
  foreach ($export in @(
      'SdrTownControl_Health',
      'SdrTownControl_Status',
      'SdrTownControl_Tune',
      'SdrTownControl_SetRfGain',
      'SdrTownControl_StartP25Control')) {
    if ([SdrTownControlProbe]::GetProcAddress($module, $export) -eq [IntPtr]::Zero) {
      throw "The loaded bridge is missing required export '$export'."
    }
  }
} finally {
  [void][SdrTownControlProbe]::FreeLibrary($module)
}

Write-Host "SdrTownControl.dll load probe passed: $resolved"
