# Symbolize module-relative crash addresses (the "SE_CRASH ... dll+0xRVA" lines that
# engine/sys_dll.cpp writes into se_probe.txt) without a debugger.
#
#   .\symbolize.ps1 -Module D:\cstrike\bin\panoramauiclient.dll -Rva 0x48DB34,0xDF714
#
# dbghelp is loaded into this process, the module is mapped at a fake base address and the
# symbols/line numbers are read straight out of the PDB that sits next to the DLL.
param(
    [Parameter(Mandatory = $true)][string]$Module,
    [Parameter(Mandatory = $true)][string[]]$Rva,
    [uint64]$FakeBase = 0x10000000
)

Add-Type -AssemblyName System.Runtime.InteropServices

$sig = @"
using System;
using System.Runtime.InteropServices;

public static class DbgHelpSym {
    [StructLayout(LayoutKind.Sequential)]
    public struct SYMBOL_INFO {
        public uint SizeOfStruct;
        public uint TypeIndex;
        public ulong Reserved0;
        public ulong Reserved1;
        public uint Index;
        public uint Size;
        public ulong ModBase;
        public uint Flags;
        public ulong Value;
        public ulong Address;
        public uint Register;
        public uint Scope;
        public uint Tag;
        public uint NameLen;
        public uint MaxNameLen;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IMAGEHLP_LINE64 {
        public uint SizeOfStruct;
        public IntPtr Key;
        public uint LineNumber;
        public IntPtr FileName;
        public ulong Address;
    }

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymInitialize(IntPtr hProcess, string UserSearchPath, bool fInvadeProcess);
    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern ulong SymLoadModuleEx(IntPtr hProcess, IntPtr hFile, string ImageName, string ModuleName,
                                               ulong BaseOfDll, uint DllSize, IntPtr Data, uint Flags);
    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymFromAddr(IntPtr hProcess, ulong Address, out ulong Displacement, IntPtr SymbolInfo);
    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymGetLineFromAddr64(IntPtr hProcess, ulong Address, out uint Displacement, IntPtr Line);
    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern uint SymSetOptions(uint SymOptions);
}
"@
Add-Type -TypeDefinition $sig

$SYMOPT_LOAD_LINES = 0x10
$SYMOPT_UNDNAME = 0x02
[DbgHelpSym]::SymSetOptions($SYMOPT_LOAD_LINES -bor $SYMOPT_UNDNAME) | Out-Null

$hProcess = [System.Diagnostics.Process]::GetCurrentProcess().Handle
# SymInitialize with fInvadeProcess=false: we only need the one module, and this keeps the call cheap.
if (-not [DbgHelpSym]::SymInitialize($hProcess, $null, $false)) {
    Write-Output ("SymInitialize failed, err=" + [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    exit 1
}

$loaded = [DbgHelpSym]::SymLoadModuleEx($hProcess, [IntPtr]::Zero, $Module, $null, $FakeBase, 0, [IntPtr]::Zero, 0)
Write-Output ("module {0} loaded at 0x{1:X} (0 = not loaded/failed, err={2})" -f $Module, $loaded, [Runtime.InteropServices.Marshal]::GetLastWin32Error())

$symSize = 1024
$symBuf = [Runtime.InteropServices.Marshal]::AllocHGlobal($symSize)
$lineBuf = [Runtime.InteropServices.Marshal]::AllocHGlobal([Runtime.InteropServices.Marshal]::SizeOf([type][DbgHelpSym+IMAGEHLP_LINE64]))

foreach ($r in $Rva) {
    $addr = $FakeBase + [uint64]$r
    [Runtime.InteropServices.Marshal]::WriteInt32($symBuf, 0, [Runtime.InteropServices.Marshal]::SizeOf([type][DbgHelpSym+SYMBOL_INFO]))
    [Runtime.InteropServices.Marshal]::WriteInt32($symBuf, 4 + 8 + 8 + 4 + 4 + 8 + 4 + 8 + 8 + 4 * 4, 900)

    $disp = [uint64]0
    $name = '<no symbol>'
    if ([DbgHelpSym]::SymFromAddr($hProcess, $addr, [ref]$disp, $symBuf)) {
        $nameOff = 4 + 8 + 8 + 4 + 4 + 8 + 4 + 8 + 8 + 4 * 4   # offset of SYMBOL_INFO.Name
        $name = [Runtime.InteropServices.Marshal]::PtrToStringAnsi($symBuf + $nameOff)
    }

    $lineText = ''
    [Runtime.InteropServices.Marshal]::WriteInt32($lineBuf, 0, [Runtime.InteropServices.Marshal]::SizeOf([type][DbgHelpSym+IMAGEHLP_LINE64]))
    $ldisp = [uint32]0
    if ([DbgHelpSym]::SymGetLineFromAddr64($hProcess, $addr, [ref]$ldisp, $lineBuf)) {
        $filePtr = [Runtime.InteropServices.Marshal]::ReadIntPtr($lineBuf, 4 + 4 + 4 + 4)
        $lineNo = [Runtime.InteropServices.Marshal]::ReadInt32($lineBuf, 12)
        $file = [Runtime.InteropServices.Marshal]::PtrToStringAnsi($filePtr)
        $lineText = "  [$file:$lineNo]"
    }

    Write-Output ("  +0x{0:X}  {1}+0x{2:X}{3}" -f $r, $name, $disp, $lineText)
}

[Runtime.InteropServices.Marshal]::FreeHGlobal($symBuf)
[Runtime.InteropServices.Marshal]::FreeHGlobal($lineBuf)
