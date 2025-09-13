#pragma once

enum class BlendMode
{
	Alpha,        // 通常のアルファブレンド（透明度合成）
	Additive,     // 加算合成（明るくなる）
	Subtractive,  // 減算合成（暗くなる）
	Multiply,     // 乗算合成（色を掛け合わせる）
	Screen,       // スクリーン合成（明るい部分を強調）
	Darken,       // 比較（暗い方）合成
	Lighten,      // 比較（明るい方）合成
	ColorBurn,    // カラーバーン（色を焼き込む）
	ColorDodge    // カラードッジ（色を抜く・明るくする）
};