#pragma once
#include <d3d12.h>

class DirectXCommon;

class IPostEffect
{
public:
    virtual ~IPostEffect() = default;

    // エフェクトの有効/無効
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
};

