# UpdateFilters.ps1
# 物理フォルダ構成に基づいて vcxproj および vcxproj.filters を自動同期するPowerShellスクリプト

$ErrorActionPreference = "SilentlyContinue"

$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
if (-not $PSScriptRoot) {
    $PSScriptRoot = (Get-Item .).FullName
}

Write-Host "Project Root: $PSScriptRoot" -ForegroundColor Cyan

$VcxprojPath = Join-Path $PSScriptRoot "KentoCompo.vcxproj"
$FiltersPath = Join-Path $PSScriptRoot "KentoCompo.vcxproj.filters"

# 対象フォルダ
$TargetDirs = @("engine", "application")

# 対象拡張子とMSBuildタグのマッピング
$ExtMap = @{
    ".cpp" = "ClCompile"
    ".h"   = "ClInclude"
    ".hpp" = "ClInclude"
    ".hlsl" = "FxCompile"
    ".hlsli" = "None"
}

Write-Host "Scanning physical files..." -ForegroundColor Cyan

# 物理ファイルをスキャン
$PhysicalFiles = @{}
foreach ($dir in $TargetDirs) {
    $dirPath = Join-Path $PSScriptRoot $dir
    if (Test-Path $dirPath) {
        $files = Get-ChildItem -Path $dirPath -Recurse -File
        foreach ($file in $files) {
            $ext = $file.Extension.ToLower()
            if ($ExtMap.ContainsKey($ext)) {
                # 確実な相対パス生成
                $relPath = $file.FullName.Replace($PSScriptRoot + "\", "")
                $relPath = $relPath -replace '/', '\'
                $PhysicalFiles[$relPath] = $ExtMap[$ext]
            }
        }
    }
}
Write-Host "Found $($PhysicalFiles.Count) source/header files physically." -ForegroundColor Green

if ($PhysicalFiles.Count -eq 0) {
    Write-Error "No physical files found! Check directories."
    exit
}

# --- 1. vcxprojの更新 ---
if (Test-Path $VcxprojPath) {
    Write-Host "Updating KentoCompo.vcxproj..." -ForegroundColor Cyan
    $xml = New-Object System.Xml.XmlDocument
    $xml.Load($VcxprojPath)

    # 既存のすべての登録ファイル情報を収集 (Include -> Tag, Metadata)
    $ExistingFiles = @{}
    $ItemGroups = $xml.SelectNodes("//*[local-name()='ItemGroup']")
    foreach ($group in $ItemGroups) {
        foreach ($tag in @("ClCompile", "ClInclude", "FxCompile", "None")) {
            $nodes = $group.SelectNodes("*[local-name()='$tag']")
            foreach ($node in $nodes) {
                $inc = $node.GetAttribute("Include")
                if ($inc) {
                    # 子要素（ExcludedFromBuildなど）をディクショナリに格納
                    $meta = @{}
                    foreach ($child in $node.ChildNodes) {
                        $meta[$child.Name] = $child.InnerText
                    }
                    $ExistingFiles[$inc] = @{ Tag = $tag; Meta = $meta }
                }
            }
        }
    }

    # マージ処理
    $MergedFiles = @{}
    
    # 1. スキャン対象外の既存登録ファイルを維持 (externals\ や main.cpp など)
    foreach ($inc in $ExistingFiles.Keys) {
        $isInTarget = $false
        foreach ($target in $TargetDirs) {
            if ($inc.StartsWith("$target\")) {
                $isInTarget = $true
                break
            }
        }
        if (-not $isInTarget) {
            $MergedFiles[$inc] = $ExistingFiles[$inc]
        }
    }

    # 2. スキャンで発見した最新のファイルを登録
    foreach ($inc in $PhysicalFiles.Keys) {
        $meta = @{}
        if ($ExistingFiles.ContainsKey($inc)) {
            $meta = $ExistingFiles[$inc].Meta
        }
        $MergedFiles[$inc] = @{ Tag = $PhysicalFiles[$inc]; Meta = $meta }
    }

    # 既存のファイル登録ItemGroupを削除
    $groupsToRemove = @()
    foreach ($group in $ItemGroups) {
        $hasFile = $false
        foreach ($tag in @("ClCompile", "ClInclude", "FxCompile", "None")) {
            if ($group.SelectNodes("*[local-name()='$tag']").Count -gt 0) {
                $hasFile = $true
                break
            }
        }
        if ($hasFile) {
            $groupsToRemove += $group
        }
    }
    foreach ($group in $groupsToRemove) {
        if ($group.ParentNode) {
            $group.ParentNode.RemoveChild($group) | Out-Null
        }
    }

    # 新しいItemGroupを追加
    $SortedKeys = $MergedFiles.Keys | Sort-Object
    $CompileGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $HeaderGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $ShaderGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $OtherGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")

    foreach ($key in $SortedKeys) {
        $tag = $MergedFiles[$key].Tag
        $meta = $MergedFiles[$key].Meta

        $elem = $xml.CreateElement($tag, "http://schemas.microsoft.com/developer/msbuild/2003")
        $elem.SetAttribute("Include", $key)

        if ($meta -ne $null) {
            foreach ($metaKey in $meta.Keys) {
                $metaChild = $xml.CreateElement($metaKey, "http://schemas.microsoft.com/developer/msbuild/2003")
                $metaChild.InnerText = $meta[$metaKey]
                $elem.AppendChild($metaChild) | Out-Null
            }
        }

        switch ($tag) {
            "ClCompile" { $CompileGroup.AppendChild($elem) | Out-Null }
            "ClInclude" { $HeaderGroup.AppendChild($elem) | Out-Null }
            "FxCompile" { $ShaderGroup.AppendChild($elem) | Out-Null }
            default { $OtherGroup.AppendChild($elem) | Out-Null }
        }
    }

    # インポートの前に挿入するため、最後のImportの前に挿入する
    $lastImport = $xml.SelectSingleNode("//*[local-name()='Import'][contains(@Project, 'targets')]")
    if ($lastImport -and $lastImport.ParentNode) {
        $lastImport.ParentNode.InsertBefore($CompileGroup, $lastImport) | Out-Null
        $lastImport.ParentNode.InsertBefore($HeaderGroup, $lastImport) | Out-Null
        $lastImport.ParentNode.InsertBefore($ShaderGroup, $lastImport) | Out-Null
        $lastImport.ParentNode.InsertBefore($OtherGroup, $lastImport) | Out-Null
    } else {
        $xml.DocumentElement.AppendChild($CompileGroup) | Out-Null
        $xml.DocumentElement.AppendChild($HeaderGroup) | Out-Null
        $xml.DocumentElement.AppendChild($ShaderGroup) | Out-Null
        $xml.DocumentElement.AppendChild($OtherGroup) | Out-Null
    }

    # 保存
    $xml.Save($VcxprojPath)
    Write-Host "KentoCompo.vcxproj updated successfully!" -ForegroundColor Green
} else {
    Write-Error "Error: KentoCompo.vcxproj not found at $VcxprojPath"
}

# --- 2. vcxproj.filtersの更新 ---
if (Test-Path $FiltersPath) {
    Write-Host "Updating KentoCompo.vcxproj.filters..." -ForegroundColor Cyan
    $xml = New-Object System.Xml.XmlDocument
    $xml.Load($FiltersPath)

    # 既存のUUIDマップを収集
    $UuidMap = @{}
    $ExistingFilters = $xml.SelectNodes("//*[local-name()='Filter']")
    foreach ($f in $ExistingFilters) {
        $inc = $f.GetAttribute("Include")
        $ui = $f.SelectSingleNode("*[local-name()='UniqueIdentifier']")
        if ($inc -and $ui -and $ui.InnerText) {
            $UuidMap[$inc] = $ui.InnerText
        }
    }

    # 既存のすべてのItemGroupをクリアして再構築
    $ItemGroups = $xml.SelectNodes("//*[local-name()='ItemGroup']")
    $groupsToRemove = @()
    foreach ($group in $ItemGroups) {
        $groupsToRemove += $group
    }
    foreach ($group in $groupsToRemove) {
        if ($group.ParentNode) {
            $group.ParentNode.RemoveChild($group) | Out-Null
        }
    }

    # 2.1 フィルター定義のItemGroupを作成
    # 登録されている全ファイルパスからフィルター階層を抽出
    $FilterPaths = @()
    foreach ($key in $MergedFiles.Keys) {
        $dirname = Split-Path $key
        if ($dirname) {
            $parts = $dirname.Split('\')
            for ($i = 1; $i -le $parts.Length; $i++) {
                $fp = [string]::Join('\', $parts[0..($i-1)])
                if ($FilterPaths -notcontains $fp) {
                    $FilterPaths += $fp
                }
            }
        }
    }
    $FilterPaths = $FilterPaths | Sort-Object

    $FilterGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    foreach ($fp in $FilterPaths) {
        $fNode = $xml.CreateElement("Filter", "http://schemas.microsoft.com/developer/msbuild/2003")
        $fNode.SetAttribute("Include", $fp)
        
        $uiNode = $xml.CreateElement("UniqueIdentifier", "http://schemas.microsoft.com/developer/msbuild/2003")
        if ($UuidMap.ContainsKey($fp)) {
            $uiNode.InnerText = $UuidMap[$fp]
        } else {
            # 新しいUUIDを生成
            $uiNode.InnerText = [guid]::NewGuid().ToString("B").ToUpper()
        }
        $fNode.AppendChild($uiNode) | Out-Null
        $FilterGroup.AppendChild($fNode) | Out-Null
    }
    $xml.DocumentElement.AppendChild($FilterGroup) | Out-Null

    # 2.2 ファイルアイテムのItemGroupを作成
    $CompileGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $HeaderGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $ShaderGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")
    $OtherGroup = $xml.CreateElement("ItemGroup", "http://schemas.microsoft.com/developer/msbuild/2003")

    foreach ($key in $SortedKeys) {
        $tag = $MergedFiles[$key].Tag
        $dirname = Split-Path $key

        $elem = $xml.CreateElement($tag, "http://schemas.microsoft.com/developer/msbuild/2003")
        $elem.SetAttribute("Include", $key)

        if ($dirname) {
            $fChild = $xml.CreateElement("Filter", "http://schemas.microsoft.com/developer/msbuild/2003")
            $fChild.InnerText = $dirname
            $elem.AppendChild($fChild) | Out-Null
        }

        switch ($tag) {
            "ClCompile" { $CompileGroup.AppendChild($elem) | Out-Null }
            "ClInclude" { $HeaderGroup.AppendChild($elem) | Out-Null }
            "FxCompile" { $ShaderGroup.AppendChild($elem) | Out-Null }
            default { $OtherGroup.AppendChild($elem) | Out-Null }
        }
    }

    $xml.DocumentElement.AppendChild($CompileGroup) | Out-Null
    $xml.DocumentElement.AppendChild($HeaderGroup) | Out-Null
    $xml.DocumentElement.AppendChild($ShaderGroup) | Out-Null
    $xml.DocumentElement.AppendChild($OtherGroup) | Out-Null

    # 保存
    $xml.Save($FiltersPath)
    Write-Host "KentoCompo.vcxproj.filters updated successfully!" -ForegroundColor Green
} else {
    Write-Error "Error: KentoCompo.vcxproj.filters not found at $FiltersPath"
}

Write-Host "Visual Studio projects synchronized successfully!" -ForegroundColor Green
