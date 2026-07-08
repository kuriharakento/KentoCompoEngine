// WriteIndirectArgs.CS.hlsl
// カウンタバッファの生存数を間接描画引数バッファのInstanceCountに書き込む
// DrawとDrawIndexedの両方に対応するため、ByteAddressBufferを使用してInstanceCountのオフセット（4バイト目）に直接ストアする

// 入力：生存数カウンタ
StructuredBuffer<uint> gCounter : register(t0);

// 出力：間接描画引数バッファ（ByteAddressBuffer）
RWByteAddressBuffer gIndirectArgs : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    // スレッド0のみで実行
    if (dtid.x == 0)
    {
        // DrawArguments と DrawIndexedArguments はどちらも2番目の要素（バイトオフセット4）がInstanceCount
        gIndirectArgs.Store(4, gCounter[0]);
    }
}
