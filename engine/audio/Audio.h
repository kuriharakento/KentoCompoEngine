#pragma once
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#include <fstream>
#include <unordered_map>
#include <wrl.h>
#include <xaudio2fx.h> // エフェクト用
//#pragma comment(lib, "XAPOFX.lib")     // エフェクト用
#include <vector>

//チャンクヘッダ
struct ChunkHeader
{
	char id[4];					//チャンクID
	int32_t size;				//チャンクサイズ
};

//RIFFヘッダ
struct RiffHeader
{
	ChunkHeader chunk;			//RIFF
	char type[4];				//WAVE
};

//FMTチャンク
struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;			//波形フォーマット
};

//音声グループ
enum class SoundGroup
{
	BGM,
	SE,
	// 必要に応じて他のグループを追加
};

//音声データ
struct SoundData
{
	WAVEFORMATEX wfex;			//波形フォーマット
	BYTE* pBuffer;				//バッファの先頭アドレス
	unsigned int bufferSize;	//バッファのサイズ
	SoundGroup group;			//グループ
};

struct FadeData
{
	IXAudio2SourceVoice* sourceVoice;
	float startVolume;
	float targetVolume;
	float currentTime;
	float duration;
	bool isFading;
};

class Audio
{
public:
	//シングルトン
	static Audio* GetInstance();
	//初期化
	void Initialize();
	//終了
	void Finalize();
	//更新
	void Update();
	//音声データの読み込み
	SoundData LoadWave(const char* filename);
	void LoadWave(const std::string& name, const char* filename, SoundGroup group);
	//再生
	void PlayWave(SoundData* soundData, bool loop = false);
	void PlayWave(const std::string& name, bool loop = false);
	//停止
	void StopWave(const std::string& name);
	void StopGroup(SoundGroup group);
	//音量調整
	void SetVolume(const std::string& name, float volume);
	void SetGroupVolume(SoundGroup group, float volume);
	//フェード
	void FadeIn(const std::string& name, float duration);
	void FadeOut(const std::string& name, float duration);

private: //メンバ関数
	void InitializeEffect();

private:
	//IXAudio2
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	//IXAudio2MasteringVoice
	IXAudio2MasteringVoice* masterVoice;

	// 音声データのマップ
	std::unordered_map<std::string, SoundData> soundDataMap_;
	// 再生中の音声ソースボイスのマップ
	std::unordered_map<std::string, IXAudio2SourceVoice*> sourceVoiceMap_;
	// グループごとのソースボイスリスト
	std::unordered_map<SoundGroup, std::vector<IXAudio2SourceVoice*>> groupVoicesMap_;
	// フェード操作を管理するリスト
	std::vector<FadeData> fadeList_;
	//エフェクトチェーン
	IXAudio2SubmixVoice* submixVoice_;

	//ディレクトリパス
	const std::string directoryPath = "Resources/audio/";

private: //シングルトン
	static Audio* instance_;

	Audio() {}
	~Audio() {}
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;
};

