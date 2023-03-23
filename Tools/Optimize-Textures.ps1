[CmdletBinding()]
param (
    [Parameter()]
    [string]
    $DirectoryToCompress,
    
    [Parameter()]
    [string]
    $CompressionFormat
)

Get-ChildItem $DirectoryToCompress -Filter *.png |
ForEach-Object {
    $OriginalFilename = $_.FullName
    $KtxFilename = $OriginalFilename.Replace("png", "ktx2")

    Write-Host "Converting file $OriginalFilename to $KtxFilename"

    if($CompressionFormat -eq '') {
        toktx --genmipmap --t2 --target_type RGBA $KtxFilename $OriginalFilename
    } else {
        toktx --genmipmap --t2 --target_type RGBA --encode $CompressionFormat $KtxFilename $OriginalFilename
    }
}

Get-ChildItem $DirectoryToCompress -Filter *.jpg |
ForEach-Object {
    $OriginalFilename = $_.FullName
    $KtxFilename = $OriginalFilename.Replace("jpg", "ktx2")

    Write-Host "Converting file $OriginalFilename to $KtxFilename"

    if($CompressionFormat -eq '') {
        toktx --genmipmap --t2 --target_type RGBA $KtxFilename $OriginalFilename
    } else {
        toktx --genmipmap --t2 --target_type RGBA --encode $CompressionFormat $KtxFilename $OriginalFilename
    }
}
