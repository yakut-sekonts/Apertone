param([Parameter(Mandatory)][string]$Cli)

$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted') {
    throw 'The VHD qualification runs on an ephemeral GitHub-hosted Windows runner.'
}
$runner = (Resolve-Path -LiteralPath $env:RUNNER_TEMP).Path
$scratch = New-Item -ItemType Directory -Path (Join-Path $runner ('apertone-exfat-' + [Guid]::NewGuid().ToString('N')))
if ([IO.Path]::GetDirectoryName($scratch.FullName) -ine $runner) { throw 'Unexpected VHD directory.' }
$image = Join-Path $scratch.FullName 'verification.vhd'
$plan = Join-Path $scratch.FullName 'create.txt'
@("create vdisk file=`"$image`" maximum=512 type=expandable", "select vdisk file=`"$image`"", 'attach vdisk') |
    Set-Content -LiteralPath $plan -Encoding ascii
try {
    & diskpart.exe /s $plan
    if ($LASTEXITCODE) { throw 'Cannot create and attach the temporary VHD.' }
    $source = Get-DiskImage -ImagePath $image
    $disks = @($source | Get-Disk)
    if (!$source.Attached -or $source.ImagePath -ine $image -or $disks.Count -ne 1 -or
        $disks[0].IsBoot -or $disks[0].IsSystem -or $disks[0].Size -ne 512MB -or
        $disks[0].PartitionStyle -cne 'RAW' -or $disks[0].BusType -notin @('File Backed Virtual', 'Virtual')) {
        throw 'The attached disk is not the new blank Apertone test VHD.'
    }
    $disk = Initialize-Disk -InputObject $disks[0] -PartitionStyle MBR -PassThru
    $partition = New-Partition -DiskNumber $disk.Number -AssignDriveLetter -UseMaximumSize
    if ($partition.DiskNumber -ne $disk.Number -or !$partition.DriveLetter) {
        throw 'The test VHD partition was not assigned a drive letter.'
    }
    $volume = Format-Volume -Partition $partition -FileSystem exFAT -NewFileSystemLabel 'ApertoneQA' -Confirm:$false
    if ($volume.FileSystem -ine 'exFAT') { throw 'The temporary partition is not exFAT.' }
    $cliPath = (Resolve-Path -LiteralPath $Cli).Path
    $env:PATH = "$(Split-Path (Split-Path $cliPath -Parent) -Parent);$env:PATH"
    $env:QT_QPA_PLATFORM = 'offscreen'
    & python (Join-Path $PSScriptRoot '../tests/integration/exfat.py') $cliPath ($partition.DriveLetter + ':\') $scratch.FullName
    if ($LASTEXITCODE) { throw 'Apertone exFAT acceptance failed.' }
} finally {
    if ((Test-Path -LiteralPath $image) -and (Get-DiskImage -ImagePath $image -ErrorAction SilentlyContinue).Attached) {
        Dismount-DiskImage -ImagePath $image | Out-Null
    }
}
