#pragma once
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#include <fstream>
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
	void RemoveFromGroupMap(IXAudio2SourceVoice* sourceVoice);
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
	std::vector<FadeData> fadeList_;
	std::unordered_map<IXAudio2SourceVoice*, bool> fadeOutStopMap_;

	bool reverbEnabled_ = true;
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
