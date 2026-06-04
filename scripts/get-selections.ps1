param (
    [string][Alias('c')]$DefaultCpu = "false",
    [Int][Alias('w')][ValidateRange(1, 8)]$CswWidth = 4,
    [switch][Alias('v')]$ShowValues,
    [Parameter(Position = 0)]$BaseDirectory = "."
)

$AllParams = @("csw", "dir", "cpu", "desc")

function Select-ColumnMajor {
    param ( [Parameter(Position=0,Mandatory=$true)]$Data )

    $half = [int][Math]::Round($Data.Count/2, [MidpointRounding]::AwayFromZero)
    $Data[((0..$half) | Foreach-Object {$_, ($_+$half)})[0..($Data.Count-1)]]
}

$Selections = Get-Item $BaseDirectory/*/pidp_info | ForEach-Object {
    $directory = $_.Directory
    $attributes = Invoke-Expression "@{$(gc $_ | % {$_ -replace '#.*','' -replace '([^=]+)=(.*)','$1=""$2"";' })}"
    if ($directory.LinkType -ne $null) {
        # special case the "default" symlink
        # it inherits the attributes of its reference
        # but the console switch setting is "0000"
        $attributes.csw = "0000"
        $attributes.dir = $directory.Target
        ## $attributes.dir = Split-Path -Leaf $_.Directory.Target
    } else {
        # ordinary directory
        $attributes.dir = $directory.Name
    }

    # parse the "csw" attribute (which could be octal,
    # decimal, or hexadecimal) and reformat as $CswWidth octal digits
    # .NET makes this hard because it doesn't like octal
    try {
        $csw = [Convert]::ToInt32($attributes.csw,
            $(if ($attributes.csw -match "^0x") { 16 }
            elseif ($attributes.csw -match "^0") { 8 }
            else { 10 }))
        $attributes.csw = [Convert]::ToString($csw, 8).PadLeft($CswWidth, '0')
    }
    catch {
        # if we can't parse the csw, skip this directory entirely
        continue
    }

    # if the "cpu" attribute is missing or empty
    # set it to $DefaultCpu
    if ("$($attributes.cpu)" -eq "") {
        $attributes.cpu = $DefaultCpu
    }

    $attributes.pretty = "$($attributes.csw)`t$(if (""$($attributes.desc)"" -ne '') {$attributes.desc} else { $attributes.dir })"

    New-Object -TypeName PSObject -Property $attributes
} | Sort-Object -Property csw

if ($ShowValues) {
    $Selections
} else {
    Select-ColumnMajor $Selections | Format-Wide -Column 2 -Property pretty
}
