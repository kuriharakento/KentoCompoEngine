#pragma once
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>
#include <xaudio2fx.h>

namespace KCE
{
struct ChunkHeader
{
	char id[4];
	int32_t size;
};

struct RiffHeader
{
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

enum class SoundGroup
{
	BGM,
	SE,
	Voice,
	Ambient,
};

enum class ReverbPreset
{
	Default,
	Generic,
	Room,
	Bathroom,
	StoneRoom,
	Auditorium,
	ConcertHall,
	Cave,
	Arena,
	Hangar,
	Forest,
	City,
	Mountains,
	Quarry,
	Plain,
	SmallRoom,
	MediumRoom,
	LargeRoom,
	MediumHall,
	LargeHall,
	Plate,
};

struct SoundData
{
	WAVEFORMATEX wfex;
	std::vector<BYTE> buffer;
	unsigned int bufferSize;
	SoundGroup group;
};

/**
 * @brief 1つのソースボイスの再生位置を追跡するための情報
 * @details SamplesPlayed はボイス生成からの累積値であり、シークしても巻き戻らない。
 *          そのため「直前にどこから再生を始めたか」を自前で覚えておき、差分で位置を求める。
 */
struct PlaybackTracking
{
	uint64_t baseSamplesPlayed = 0; //!< 再生開始／シーク時点の SamplesPlayed
	uint64_t startSample = 0;		//!< そのとき再生を開始したバッファ内サンプル位置
	bool loop = false;				//!< ループ再生かどうか
};

struct FadeData
{
	IXAudio2SourceVoice* sourceVoice;
	std::string name;
	float startVolume;
	float targetVolume;
	float currentTime;
	float duration;
	bool isFading;
};

#ifdef USE_IMGUI
struct AudioDebugData
{
	bool windowVisible = false;
	float groupVolumes[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	std::string selectedSound;
	float selectedVolume = 1.0f;
	float selectedPitch = 1.0f;
	bool selectedLoop = false;
	float fadeDuration = 1.0f;
	float fadeTargetVolume = 1.0f;
	bool fadeOutStop = true;
	int currentPreset = 0;
	float reverbAmount = 0.3f;
};
#endif

class Audio
{
public:
	static Audio* GetInstance();
	void Initialize();
	void Finalize();
	void Update();

	SoundData LoadWave(const char* filename);
	void LoadWave(const std::string& name, const char* filename, SoundGroup group);

	/**
	 * @brief Application側のaudioフォルダから音声を読み込む。
	 * @param filename 音声ファイル名。キャッシュキーにも使用する。
	 * @param group 音声グループ
	 */
	void Load(const std::string& filename, SoundGroup group);

	/**
	 * @brief Application側のaudioフォルダから音声を指定名で読み込む。
	 * @param name キャッシュに登録する名前
	 * @param filename 音声ファイル名
	 * @param group 音声グループ
	 */
	void Load(const std::string& name, const std::string& filename, SoundGroup group);

	void PlayWave(SoundData* soundData, bool loop = false);
	void PlayWave(const std::string& name, bool loop = false);
	void StopWave(const std::string& name);
	void StopGroup(SoundGroup group);
	void StopAll();

	void Pause(const std::string& name);
	void Resume(const std::string& name);
	void PauseGroup(SoundGroup group);
	void ResumeGroup(SoundGroup group);
	void PauseAll();
	void ResumeAll();

	void SetVolume(const std::string& name, float volume);
	void SetGroupVolume(SoundGroup group, float volume);
	void SetMasterVolume(float volume);
	float GetMasterVolume() const;
	void SetPitch(const std::string& name, float pitch);

	void FadeIn(const std::string& name, float duration, float targetVolume = 1.0f);
	void FadeOut(const std::string& name, float duration, bool stopOnComplete = true);

	bool IsPlaying(const std::string& name) const;
	bool IsPaused(const std::string& name) const;
	bool IsLoaded(const std::string& name) const;

	// --- 再生位置（シーケンサの時間の権威） ---

	/**
	 * @brief 現在の再生位置を秒で取得する
	 * @details SEQUENCER_PLAN 3.8。deltaTime の積算は数分の楽曲で必ずズレるため、
	 *          演出の時刻はこの値を権威とする。
	 *          IXAudio2SourceVoice::GetState() の SamplesPlayed から算出し、
	 *          Seek() で設定した開始位置を加味する。ループ時は長さで折り返す。
	 * @param name 再生中の音声名
	 * @return 再生位置（秒）。再生していない場合は0
	 */
	float GetPlayPosition(const std::string& name) const;

	/**
	 * @brief 再生位置を指定秒に移動する
	 * @details PCMを全展開済みという特性を活かし、SubmitSourceBuffer の PlayBegin で
	 *          任意位置から再投入する。再生中でなければ何もしない。
	 * @param name 再生中の音声名
	 * @param seconds 移動先の位置（秒）。範囲外はクランプされる
	 * @return シークに成功したら真
	 */
	bool Seek(const std::string& name, float seconds);

	/**
	 * @brief 音声の総再生時間を秒で取得する
	 * @param name 読み込み済みの音声名
	 * @return 総再生時間（秒）。未読み込みなら0
	 */
	float GetDuration(const std::string& name) const;

	/**
	 * @brief 音声のサンプリングレートを取得する
	 * @param name 読み込み済みの音声名
	 * @return サンプリングレート（Hz）。未読み込みなら0
	 */
	uint32_t GetSampleRate(const std::string& name) const;

	void SetReverbEnabled(bool enabled);
	bool IsReverbEnabled() const;
	void SetReverbPreset(ReverbPreset preset);
	void SetReverbAmount(float amount);
	float GetReverbAmount() const;

	void UnloadWave(const std::string& name);
	void UnloadAll();

#ifdef USE_IMGUI
	void SetDebugWindowVisible(bool visible) { debugData_.windowVisible = visible; }
	bool IsDebugWindowVisible() const { return debugData_.windowVisible; }
	void ToggleDebugWindow() { debugData_.windowVisible = !debugData_.windowVisible; }
#endif

private:
	friend std::unique_ptr<Audio> std::make_unique<Audio>();

	void InitializeEffect();
	bool DecodeAudioFile(const std::filesystem::path& path, SoundGroup group, SoundData& output);
	void RemoveFromGroupMap(IXAudio2SourceVoice* sourceVoice);

	/**
	 * @brief 音声データの総サンプル数を求める
	 * @param soundData 対象の音声データ
	 * @return サンプル数。ブロックアラインが0なら0
	 */
	static uint64_t GetTotalSampleCount(const SoundData& soundData);

	float ClampVolume(float volume) const;
	float ClampPitch(float pitch) const;
	void UpdateReverbVolume();

#ifdef USE_IMGUI
	void DrawDebugWindow();
#endif

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	IXAudio2SubmixVoice* submixVoiceDry_ = nullptr;	   // ドライ音用
	IXAudio2SubmixVoice* submixVoiceReverb_ = nullptr; // リバーブ用

	std::unordered_map<std::string, SoundData> soundDataMap_;
	std::unordered_map<std::string, IXAudio2SourceVoice*> sourceVoiceMap_;
	std::unordered_map<SoundGroup, std::vector<IXAudio2SourceVoice*>> groupVoicesMap_;
	std::unordered_map<std::string, bool> pausedMap_;
	std::unordered_map<std::string, PlaybackTracking> playbackTrackingMap_;
	std::vector<FadeData> fadeList_;
	std::unordered_map<IXAudio2SourceVoice*, bool> fadeOutStopMap_;

	bool reverbEnabled_ = true;
	bool mediaFoundationInitialized_ = false;
	float masterVolume_ = 1.0f;
	float reverbAmount_ = 0.3f;
	ReverbPreset currentPreset_ = ReverbPreset::Default;
	const std::string directoryPath_ = "audio/";

#ifdef USE_IMGUI
	AudioDebugData debugData_;
#endif

	static std::unique_ptr<Audio> instance_;
	Audio() = default;
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

public:
	~Audio() = default;
};
} // namespace KCE
