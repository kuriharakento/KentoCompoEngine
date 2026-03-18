$files = @(
    "project\KentoCompo.vcxproj",
    "project\KentoCompo.vcxproj.filters",
    "project\engine\ecs\debug\EcsInspector.cpp",
    "project\application\scene\EcsDebugScene.cpp",
    "project\application\GameObject\obstacle\ObstacleManager.cpp",
    "project\application\GameObject\obstacle\ObstacleManager.h",
    "project\application\ecs\systems\InstancedRenderSystem.cpp",
    "project\application\ecs\systems\InstancedRenderSystem.h",
    "project\application\GameObject\Combatable\character\enemy\EnemyManager.cpp",
    "project\application\GameObject\Combatable\character\enemy\EnemyManager.h",
    "project\application\ecs\components\RenderComponent.h"
)

foreach ($f in $files) {
    if (Test-Path $f) {
        (Get-Content $f) -replace 'RenderComponent', 'InstancedRenderComponent' | Set-Content $f
    }
}

$oldFile = "project\application\ecs\components\RenderComponent.h"
$newFile = "project\application\ecs\components\InstancedRenderComponent.h"
if (Test-Path $oldFile) {
    Rename-Item -Path $oldFile -NewName "InstancedRenderComponent.h"
}
