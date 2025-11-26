#pragma once
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#include <fstream>
#include <unordered_map>
#include <wrl.h>
#include <xaudio2fx.h> // エフェクト用
//#pragma comment(lib, "XAPOFX.lib")     // エフェクト用
#include <vector>

/**
 * @brief チャンクヘッダ
 */
struct ChunkHeader
{
	// チャンクID
	char id[4];
	// チャンクサイズ
	int32_t size;
};

/**
 * @brief RIFFヘッダ
 */
struct RiffHeader
{
	// RIFFチャンク
	ChunkHeader chunk;
	// WAVEタイプ
	char type[4];
};

/**
 * @brief FMTチャンク
 */
struct FormatChunk
{
	// チャンクヘッダ
	ChunkHeader chunk;
	// 波形フォーマット
	WAVEFORMATEX fmt;
};

/**
 * @brief 音声グループの種類
 */
enum class SoundGroup
{
	// BGM
	BGM,
	// 効果音
	SE,
	// 必要に応じて他のグループを追加
};

/**
 * @brief 音声データ
 */
struct SoundData
{
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
	// 音声グループ
	SoundGroup group;
};

/**
 * @brief フェード処理用データ
 */
struct FadeData
{
	// ソースボイス
	IXAudio2SourceVoice* sourceVoice;
	// 開始音量
	float startVolume;
	// 目標音量
	float targetVolume;
	// 現在の経過時間
	float currentTime;
	// フェードの継続時間
	float duration;
	// フェード中フラグ
	bool isFading;
};

/**
 * @brief オーディオ管理クラス
 */
class Audio
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return Audioクラスのインスタンス
	 */
	static Audio* GetInstance();

	/**
	 * @brief 初期化処理
	 */
	void Initialize();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief 更新処理（フェード処理等）
	 */
	void Update();

	/**
	 * @brief WAVファイルの読み込み
	 * @param filename ファイル名
	 * @return 読み込んだ音声データ
	 */
	SoundData LoadWave(const char* filename);

	/**
	 * @brief WAVファイルの読み込み（名前付き）
	 * @param name 音声データの名前
	 * @param filename ファイル名
	 * @param group 音声グループ
	 */
	void LoadWave(const std::string& name, const char* filename, SoundGroup group);

	/**
	 * @brief 音声の再生
	 * @param soundData 音声データ
	 * @param loop ループ再生するかどうか
	 */
	void PlayWave(SoundData* soundData, bool loop = false);

	/**
	 * @brief 名前指定で音声を再生
	 * @param name 音声データの名前
	 * @param loop ループ再生するかどうか
	 */
	void PlayWave(const std::string& name, bool loop = false);

	/**
	 * @brief 音声の停止
	 * @param name 音声データの名前
	 */
	void StopWave(const std::string& name);

	/**
	 * @brief グループ内の全音声を停止
	 * @param group 音声グループ
	 */
	void StopGroup(SoundGroup group);

	/**
	 * @brief 音量の設定
	 * @param name 音声データの名前
	 * @param volume 音量（0.0f〜1.0f）
	 */
	void SetVolume(const std::string& name, float volume);

	/**
	 * @brief グループ全体の音量を設定
	 * @param group 音声グループ
	 * @param volume 音量（0.0f〜1.0f）
	 */
	void SetGroupVolume(SoundGroup group, float volume);

	/**
	 * @brief フェードイン開始
	 * @param name 音声データの名前
	 * @param duration フェードの継続時間（秒）
	 */
	void FadeIn(const std::string& name, float duration);

	/**
	 * @brief フェードアウト開始
	 * @param name 音声データの名前
	 * @param duration フェードの継続時間（秒）
	 */
	void FadeOut(const std::string& name, float duration);

private:
	/**
	 * @brief エフェクトの初期化
	 */
	void InitializeEffect();

private:
	// XAudio2エンジン
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	// マスターボイス
	IXAudio2MasteringVoice* masterVoice;
	// 音声データのマップ
	std::unordered_map<std::string, SoundData> soundDataMap_;
	// 再生中の音声ソースボイスのマップ
	std::unordered_map<std::string, IXAudio2SourceVoice*> sourceVoiceMap_;
	// グループごとのソースボイスリスト
	std::unordered_map<SoundGroup, std::vector<IXAudio2SourceVoice*>> groupVoicesMap_;
	// フェード操作を管理するリスト
	std::vector<FadeData> fadeList_;
	// サブミックスボイス（エフェクト用）
	IXAudio2SubmixVoice* submixVoice_;
	// 音声リソースのディレクトリパス
	const std::string directoryPath = "Resources/audio/";

private:
	// シングルトンインスタンス
	static Audio* instance_;

	Audio() {}
	~Audio() {}
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;
};

