#pragma once
#include <d3d12.h>

class DirectXCommon;

/**
 * @brief ポストエフェクトのインターフェースクラス
 *
 * すべてのポストエフェクトが実装すべき基本的なインターフェースを定義する。
 * エフェクトの有効/無効の切り替え機能を提供する。
 */
class IPostEffect
{
public:
    virtual ~IPostEffect() = default;

    /**
     * @brief エフェクトの有効/無効を設定する
     * @param enabled true: エフェクト有効、false: エフェクト無効
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * @brief エフェクトが有効かどうかを取得する
     * @return true: エフェクト有効、false: エフェクト無効
     */
    virtual bool IsEnabled() const = 0;
};

