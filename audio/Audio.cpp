#include "Audio.h"

#include "base/PathManager.h"
#include "base/Logger.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{

namespace
{
	constexpr float kFrameRate = 60.0f;
	constexpr float kDeltaTime = 1.0f / kFrameRate;
	constexpr int kStereoChannels = 2;
	constexpr int kDefaultSampleRate = 44100;
	class MediaBufferLock
	{
	public:
		explicit MediaBufferLock(IMFMediaBuffer* buffer) : buffer_(buffer) {}

		HRESULT Lock(BYTE** data, DWORD* length)
		{
			HRESULT hr = buffer_->Lock(data, nullptr, length);
			locked_ = SUCCEEDED(hr);
			return hr;
		}

		~MediaBufferLock()
		{
			if (locked_)
			{
				buffer_->Unlock();
			}
		}

	private:
		Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer_;
		bool locked_ = false;
	};

	struct CoTaskMemDeleter
	{
		void operator()(void* memory) const
		{
			CoTaskMemFree(memory);
		}
	};

	constexpr float kMinPitch = 0.5f;
	constexpr float kMaxPitch = 2.0f;
	constexpr float kDefaultReverbAmount = 0.3f; // デフォルト30%

	XAUDIO2FX_REVERB_I3DL2_PARAMETERS GetReverbPresetParams(ReverbPreset preset)
	{
		switch (preset)
		{
		case ReverbPreset::Generic:
			return XAUDIO2FX_I3DL2_PRESET_GENERIC;
		case ReverbPreset::Room:
			return XAUDIO2FX_I3DL2_PRESET_ROOM;
		case ReverbPreset::Bathroom:
			return XAUDIO2FX_I3DL2_PRESET_BATHROOM;
		case ReverbPreset::StoneRoom:
			return XAUDIO2FX_I3DL2_PRESET_STONEROOM;
		case ReverbPreset::Auditorium:
			return XAUDIO2FX_I3DL2_PRESET_AUDITORIUM;
		case ReverbPreset::ConcertHall:
			return XAUDIO2FX_I3DL2_PRESET_CONCERTHALL;
		case ReverbPreset::Cave:
			return XAUDIO2FX_I3DL2_PRESET_CAVE;
		case ReverbPreset::Arena:
			return XAUDIO2FX_I3DL2_PRESET_ARENA;
		case ReverbPreset::Hangar:
			return XAUDIO2FX_I3DL2_PRESET_HANGAR;
		case ReverbPreset::Forest:
			return XAUDIO2FX_I3DL2_PRESET_FOREST;
		case ReverbPreset::City:
			return XAUDIO2FX_I3DL2_PRESET_CITY;
		case ReverbPreset::Mountains:
			return XAUDIO2FX_I3DL2_PRESET_MOUNTAINS;
		case ReverbPreset::Quarry:
			return XAUDIO2FX_I3DL2_PRESET_QUARRY;
		case ReverbPreset::Plain:
			return XAUDIO2FX_I3DL2_PRESET_PLAIN;
		case ReverbPreset::SmallRoom:
			return XAUDIO2FX_I3DL2_PRESET_SMALLROOM;
		case ReverbPreset::MediumRoom:
			return XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM;
		case ReverbPreset::LargeRoom:
			return XAUDIO2FX_I3DL2_PRESET_LARGEROOM;
		case ReverbPreset::MediumHall:
			return XAUDIO2FX_I3DL2_PRESET_MEDIUMHALL;
		case ReverbPreset::LargeHall:
			return XAUDIO2FX_I3DL2_PRESET_LARGEHALL;
		case ReverbPreset::Plate:
			return XAUDIO2FX_I3DL2_PRESET_PLATE;
		case ReverbPreset::Default:
		default:
			return XAUDIO2FX_I3DL2_PRESET_DEFAULT;
		}
	}

#ifdef USE_IMGUI
	const char* GetGroupName(SoundGroup group)
	{
		switch (group)
		{
		case SoundGroup::BGM:
			return "BGM";
		case SoundGroup::SE:
			return "SE";
		case SoundGroup::Voice:
			return "Voice";
		case SoundGroup::Ambient:
			return "Ambient";
		default:
			return "Unknown";
		}
	}

	struct PresetInfo
	{
		ReverbPreset preset;
		const char* name;
	};

	constexpr PresetInfo kPresetList[] = {
		{ReverbPreset::Default, "Default"},
		{ReverbPreset::Generic, "Generic"},
		{ReverbPreset::Room, "Room"},
		{ReverbPreset::Bathroom, "Bathroom"},
		{ReverbPreset::StoneRoom, "Stone Room"},
		{ReverbPreset::Auditorium, "Auditorium"},
		{ReverbPreset::ConcertHall, "Concert Hall"},
		{ReverbPreset::Cave, "Cave"},
		{ReverbPreset::Arena, "Arena"},
		{ReverbPreset::Hangar, "Hangar"},
		{ReverbPreset::Forest, "Forest"},
		{ReverbPreset::City, "City"},
		{ReverbPreset::Mountains, "Mountains"},
		{ReverbPreset::Quarry, "Quarry"},
		{ReverbPreset::Plain, "Plain"},
		{ReverbPreset::SmallRoom, "Small Room"},
		{ReverbPreset::MediumRoom, "Medium Room"},
		{ReverbPreset::LargeRoom, "Large Room"},
		{ReverbPreset::MediumHall, "Medium Hall"},
		{ReverbPreset::LargeHall, "Large Hall"},
		{ReverbPreset::Plate, "Plate"},
	};
	constexpr int kPresetCount = sizeof(kPresetList) / sizeof(kPresetList[0]);
#endif
} // namespace

std::unique_ptr<Audio> Audio::instance_ = nullptr;

Audio* Audio::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<Audio>();
	}
	return instance_.get();
}

void Audio::Initialize()
{
	HRESULT hr = MFStartup(MF_VERSION);
	assert(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return;
	}
	mediaFoundationInitialized_ = true;

	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));

	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));

	reverbAmount_ = kDefaultReverbAmount;
#ifdef USE_IMGUI
	debugData_.reverbAmount = kDefaultReverbAmount;

	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Audio Debug", [this]()
	{
		this->DrawDebugWindow();
	}, DebugUIArea::Console);
#endif

	InitializeEffect();
	SetReverbPreset(ReverbPreset::LargeHall);
}

void Audio::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	fadeList_.clear();
	fadeOutStopMap_.clear();

	for (auto& pair : sourceVoiceMap_)
	{
		if (pair.second)
		{
			pair.second->Stop(0);
			pair.second->FlushSourceBuffers();
			pair.second->DestroyVoice();
		}
	}
	sourceVoiceMap_.clear();
	pausedMap_.clear();
	playbackTrackingMap_.clear();
	groupVoicesMap_.clear();

	// vectorのデストラクタにより自動解放されるためループ削除
	soundDataMap_.clear();

	if (submixVoiceReverb_)
	{
		submixVoiceReverb_->DestroyVoice();
		submixVoiceReverb_ = nullptr;
	}

	if (submixVoiceDry_)
	{
		submixVoiceDry_->DestroyVoice();
		submixVoiceDry_ = nullptr;
	}

	if (masterVoice_)
	{
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}

	if (xAudio2_)
	{
		xAudio2_->StopEngine();
		xAudio2_.Reset();
	}

	if (mediaFoundationInitialized_)
	{
		MFShutdown();
		mediaFoundationInitialized_ = false;
	}

	instance_.reset();
}

void Audio::Update()
{
	for (auto it = fadeList_.begin(); it != fadeList_.end();)
	{
		FadeData& fade = *it;
		if (!fade.isFading || !fade.sourceVoice)
		{
			++it;
			continue;
		}

		fade.currentTime += kDeltaTime;
		float t = (std::min)(fade.currentTime / fade.duration, 1.0f);
		float volume = fade.startVolume + (fade.targetVolume - fade.startVolume) * t;
		fade.sourceVoice->SetVolume(volume);

		if (t >= 1.0f)
		{
			fade.isFading = false;

			if (fade.targetVolume == 0.0f)
			{
				auto stopIt = fadeOutStopMap_.find(fade.sourceVoice);
				bool shouldStop = (stopIt == fadeOutStopMap_.end()) || stopIt->second;

				if (shouldStop)
				{
					fade.sourceVoice->Stop(0);
					fade.sourceVoice->FlushSourceBuffers();
					fade.sourceVoice->DestroyVoice();

					auto mapIt = sourceVoiceMap_.find(fade.name);
					if (mapIt != sourceVoiceMap_.end() && mapIt->second == fade.sourceVoice)
					{
						sourceVoiceMap_.erase(mapIt);
					}
					RemoveFromGroupMap(fade.sourceVoice);
					pausedMap_.erase(fade.name);
					playbackTrackingMap_.erase(fade.name);
				}
				fadeOutStopMap_.erase(fade.sourceVoice);
			}

			it = fadeList_.erase(it);
			continue;
		}
		++it;
	}
}

SoundData Audio::LoadWave(const char* filename)
{
	SoundData soundData = {};
	const auto logFailure = [this, filename](const std::string& reason)
	{
		// 更新処理から繰り返し呼ばれても、同じファイルの失敗は一度だけ知らせる。
		if (reportedLoadErrors_.insert(filename).second)
		{
			Logger::Log(reason + filename + "\n", Logger::LogLevel::Error);
		}
	};
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open())
	{
		logFailure("音声ファイルを開けませんでした: ");
		return soundData;
	}

	RiffHeader riff{};
	if (!file.read(reinterpret_cast<char*>(&riff), sizeof(riff)) ||
		strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0)
	{
		logFailure("WAVヘッダーを読み取れませんでした: ");
		return soundData;
	}

	FormatChunk format = {};
	if (!file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader)) ||
		strncmp(format.chunk.id, "fmt ", 4) != 0 || format.chunk.size > sizeof(format.fmt) ||
		!file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size))
	{
		logFailure("WAVフォーマットを読み取れませんでした: ");
		return soundData;
	}

	ChunkHeader data;
	if (!file.read(reinterpret_cast<char*>(&data), sizeof(data)))
	{
		logFailure("WAVデータを読み取れませんでした: ");
		return soundData;
	}

	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		file.seekg(data.size, std::ios::cur);
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
	}
	if (!file || strncmp(data.id, "data", 4) != 0 || data.size < 0)
	{
		logFailure("WAVのdataチャンクを読み取れませんでした: ");
		return soundData;
	}
	const std::streampos dataStart = file.tellg();
	file.seekg(0, std::ios::end);
	const std::streampos fileEnd = file.tellg();
	file.seekg(dataStart);
	if (dataStart < 0 || fileEnd < dataStart || static_cast<uint64_t>(data.size) > static_cast<uint64_t>(fileEnd - dataStart))
	{
		logFailure("WAVの音声データが途中で切れています: ");
		return soundData;
	}

	std::vector<BYTE> buffer(data.size);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), data.size))
	{
		logFailure("WAVの音声データが途中で切れています: ");
		return soundData;
	}
	file.close();

	soundData.wfex = format.fmt;
	soundData.buffer = std::move(buffer);
	soundData.bufferSize = data.size;
	return soundData;
}

void Audio::LoadWave(const std::string& name, const char* filename, SoundGroup group)
{
	Load(name, filename, group);
}

void Audio::Load(const std::string& filename, SoundGroup group)
{
	Load(filename, filename, group);
}

void Audio::Load(const std::string& name, const std::string& filename, SoundGroup group)
{
	if (soundDataMap_.find(name) != soundDataMap_.end())
	{
		return;
	}

	std::filesystem::path relativePath = std::filesystem::path(directoryPath_) / filename;
	std::filesystem::path resolved = PathManager::ResolveApplicationResource(relativePath);
	if (!std::filesystem::exists(resolved))
	{
		resolved = PathManager::GetApplicationResourceRoot() / "audio" / filename;
	}
	SoundData soundData = {};
	if (DecodeAudioFile(resolved, group, soundData))
	{
		// 描画幅に合わせた集計を毎フレーム行わず、読み込み時に一度だけ粗い波形を作る。
		BuildWaveformPeaks(soundData);
		soundDataMap_[name] = std::move(soundData);
		loadedSoundNames_.push_back(name);
	}
	else if (reportedLoadErrors_.insert(resolved.string()).second)
	{
		Logger::Log("音声ファイルを読み込めませんでした: " + resolved.string() + "\n", Logger::LogLevel::Error);
	}
}

bool Audio::DecodeAudioFile(const std::filesystem::path& path, SoundGroup group, SoundData& output)
{
	if (!mediaFoundationInitialized_ || !std::filesystem::exists(path))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader;
	HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &sourceReader);
	if (FAILED(hr))
	{
		return false;
	}

	hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if (FAILED(hr))
	{
		return false;
	}
	hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
	if (FAILED(hr))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> requestedType;
	hr = MFCreateMediaType(&requestedType);
	if (FAILED(hr))
	{
		return false;
	}
	hr = requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	if (FAILED(hr))
	{
		return false;
	}
	hr = requestedType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	if (FAILED(hr))
	{
		return false;
	}
	hr = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, requestedType.Get());
	if (FAILED(hr))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> currentType;
	hr = sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &currentType);
	if (FAILED(hr))
	{
		return false;
	}

	WAVEFORMATEX* rawWaveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	hr = MFCreateWaveFormatExFromMFMediaType(currentType.Get(), &rawWaveFormat, &waveFormatSize);
	std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter> waveFormat(rawWaveFormat);
	if (FAILED(hr) || !waveFormat || waveFormatSize < sizeof(WAVEFORMATEX))
	{
		return false;
	}

	SoundData decoded = {};
	decoded.wfex = *waveFormat;
	decoded.group = group;

	for (;;)
	{
		DWORD flags = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;
		hr = sourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
		if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR) != 0)
		{
			return false;
		}
		if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0)
		{
			return false;
		}
		if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
		{
			break;
		}
		if (!sample)
		{
			continue;
		}

		Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
		hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
		if (FAILED(hr))
		{
			return false;
		}

		BYTE* data = nullptr;
		DWORD length = 0;
		MediaBufferLock bufferLock(mediaBuffer.Get());
		hr = bufferLock.Lock(&data, &length);
		if (FAILED(hr))
		{
			return false;
		}

		constexpr size_t kMaximumAudioBytes = static_cast<size_t>(XAUDIO2_MAX_BUFFER_BYTES);
		if (length > kMaximumAudioBytes || decoded.buffer.size() > kMaximumAudioBytes - length)
		{
			return false;
		}
		decoded.buffer.insert(decoded.buffer.end(), data, data + length);
	}

	if (decoded.buffer.empty() || decoded.buffer.size() > (std::numeric_limits<unsigned int>::max)())
	{
		return false;
	}
	decoded.bufferSize = static_cast<unsigned int>(decoded.buffer.size());
	output = std::move(decoded);
	return true;
}

void Audio::PlayWave(SoundData* soundData, bool loop)
{
	if (!soundData || soundData->buffer.empty())
	{
		if (reportedPlaybackErrors_.insert("<invalid SoundData>").second)
		{
			Logger::Log("音声データが無効なため再生できませんでした。\n", Logger::LogLevel::Error);
		}
		return;
	}
	IXAudio2SourceVoice* sourceVoice;
	HRESULT hr;

	// 【修正点】Pointer版PlayWaveでもリバーブへのセンドを行うように修正
	if (submixVoiceDry_ && submixVoiceReverb_)
	{
		XAUDIO2_SEND_DESCRIPTOR sendDescs[2] = {};
		sendDescs[0].Flags = 0;
		sendDescs[0].pOutputVoice = submixVoiceDry_;
		sendDescs[1].Flags = 0;
		sendDescs[1].pOutputVoice = submixVoiceReverb_;

		XAUDIO2_VOICE_SENDS sendList = {};
		sendList.SendCount = 2;
		sendList.pSends = sendDescs;

		hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData->wfex, 0,
										XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList);
	}
	else
	{
		hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData->wfex);
	}
	if (FAILED(hr))
	{
		// ポインタ版は名前を持たないので、失敗の種類ごとに1回だけ出す
		if (reportedPlaybackErrors_.insert("<CreateSourceVoice>").second)
		{
			Logger::Log("音声のSourceVoiceを作成できませんでした。\n", Logger::LogLevel::Error);
		}
		return;
	}

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = soundData->bufferSize;
	buffer.pAudioData = soundData->buffer.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	hr = sourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
	{
		if (reportedPlaybackErrors_.insert("<SubmitSourceBuffer>").second)
		{
			Logger::Log("音声バッファを送信できませんでした。\n", Logger::LogLevel::Error);
		}
		sourceVoice->DestroyVoice();
		return;
	}
	hr = sourceVoice->Start();
	if (FAILED(hr))
	{
		if (reportedPlaybackErrors_.insert("<Start>").second)
		{
			Logger::Log("音声の再生を開始できませんでした。\n", Logger::LogLevel::Error);
		}
		sourceVoice->DestroyVoice();
	}
}

void Audio::PlayWave(const std::string& name, bool loop)
{
	auto it = soundDataMap_.find(name);
	if (it == soundDataMap_.end())
	{
		if (reportedPlaybackErrors_.insert(name).second)
		{
			Logger::Log("登録されていない音声は再生できません: " + name + "\n", Logger::LogLevel::Error);
		}
		return;
	}

	auto voiceIt = sourceVoiceMap_.find(name);
	if (voiceIt != sourceVoiceMap_.end())
	{
		voiceIt->second->Stop(0);
		voiceIt->second->FlushSourceBuffers();
		voiceIt->second->DestroyVoice();
		RemoveFromGroupMap(voiceIt->second);
		sourceVoiceMap_.erase(voiceIt);
		pausedMap_.erase(name);
		playbackTrackingMap_.erase(name);
	}

	SoundData& soundData = it->second;
	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT hr;

	// ドライとリバーブの両方のサブミックスに出力
	if (submixVoiceDry_ && submixVoiceReverb_)
	{
		XAUDIO2_SEND_DESCRIPTOR sendDescs[2] = {};
		sendDescs[0].Flags = 0;
		sendDescs[0].pOutputVoice = submixVoiceDry_;
		sendDescs[1].Flags = 0;
		sendDescs[1].pOutputVoice = submixVoiceReverb_;

		XAUDIO2_VOICE_SENDS sendList = {};
		sendList.SendCount = 2;
		sendList.pSends = sendDescs;

		hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex, 0,
										XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList);
	}
	else
	{
		hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex);
	}
	if (FAILED(hr))
	{
		Logger::Log("音声のSourceVoiceを作成できませんでした: " + name + "\n", Logger::LogLevel::Error);
		return;
	}

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = soundData.bufferSize;
	buffer.pAudioData = soundData.buffer.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	hr = sourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
	{
		Logger::Log("音声バッファを送信できませんでした: " + name + "\n", Logger::LogLevel::Error);
		sourceVoice->DestroyVoice();
		return;
	}
	hr = sourceVoice->Start(0);
	if (FAILED(hr))
	{
		Logger::Log("音声の再生を開始できませんでした: " + name + "\n", Logger::LogLevel::Error);
		sourceVoice->DestroyVoice();
		return;
	}

	sourceVoiceMap_[name] = sourceVoice;
	groupVoicesMap_[soundData.group].push_back(sourceVoice);
	pausedMap_[name] = false;

	// 再生位置の基準を初期化する。新規ボイスなので SamplesPlayed は0から始まる。
	PlaybackTracking tracking;
	tracking.baseSamplesPlayed = 0;
	tracking.startSample = 0;
	tracking.loop = loop;
	playbackTrackingMap_[name] = tracking;
}

uint64_t Audio::GetTotalSampleCount(const SoundData& soundData)
{
	const uint32_t blockAlign = soundData.wfex.nBlockAlign;
	if (blockAlign == 0)
	{
		return 0;
	}
	return static_cast<uint64_t>(soundData.bufferSize) / blockAlign;
}

float Audio::GetDuration(const std::string& name) const
{
	auto it = soundDataMap_.find(name);
	if (it == soundDataMap_.end())
	{
		return 0.0f;
	}

	const uint32_t sampleRate = it->second.wfex.nSamplesPerSec;
	if (sampleRate == 0)
	{
		return 0.0f;
	}

	return static_cast<float>(GetTotalSampleCount(it->second)) / static_cast<float>(sampleRate);
}

uint32_t Audio::GetSampleRate(const std::string& name) const
{
	auto it = soundDataMap_.find(name);
	if (it == soundDataMap_.end())
	{
		return 0;
	}
	return it->second.wfex.nSamplesPerSec;
}

float Audio::GetPlayPosition(const std::string& name) const
{
	auto voiceIt = sourceVoiceMap_.find(name);
	auto soundIt = soundDataMap_.find(name);
	auto trackingIt = playbackTrackingMap_.find(name);
	if (voiceIt == sourceVoiceMap_.end() || soundIt == soundDataMap_.end() || trackingIt == playbackTrackingMap_.end())
	{
		return 0.0f;
	}

	const uint32_t sampleRate = soundIt->second.wfex.nSamplesPerSec;
	if (sampleRate == 0)
	{
		return 0.0f;
	}

	XAUDIO2_VOICE_STATE state = {};
	voiceIt->second->GetState(&state);

	const PlaybackTracking& tracking = trackingIt->second;

	// SamplesPlayed はボイス生成からの累積。ストリーム終端で0に戻る実装があるため、
	// 基準より小さくなった場合は基準からのやり直しとみなす。
	const uint64_t playedSinceBase =
		state.SamplesPlayed >= tracking.baseSamplesPlayed ? state.SamplesPlayed - tracking.baseSamplesPlayed : 0;

	uint64_t positionSample = tracking.startSample + playedSinceBase;

	const uint64_t totalSamples = GetTotalSampleCount(soundIt->second);
	if (totalSamples > 0)
	{
		if (tracking.loop)
		{
			// ループ時は曲頭に折り返す
			positionSample %= totalSamples;
		}
		else if (positionSample > totalSamples)
		{
			positionSample = totalSamples;
		}
	}

	return static_cast<float>(positionSample) / static_cast<float>(sampleRate);
}

bool Audio::Seek(const std::string& name, float seconds)
{
	auto voiceIt = sourceVoiceMap_.find(name);
	auto soundIt = soundDataMap_.find(name);
	auto trackingIt = playbackTrackingMap_.find(name);
	if (voiceIt == sourceVoiceMap_.end() || soundIt == soundDataMap_.end() || trackingIt == playbackTrackingMap_.end())
	{
		return false;
	}

	SoundData& soundData = soundIt->second;
	const uint32_t sampleRate = soundData.wfex.nSamplesPerSec;
	const uint64_t totalSamples = GetTotalSampleCount(soundData);
	if (sampleRate == 0 || totalSamples == 0)
	{
		return false;
	}

	// 移動先をバッファ内に収める
	if (seconds < 0.0f)
	{
		seconds = 0.0f;
	}
	uint64_t targetSample = static_cast<uint64_t>(seconds * static_cast<float>(sampleRate));
	if (targetSample >= totalSamples)
	{
		targetSample = totalSamples - 1;
	}

	IXAudio2SourceVoice* sourceVoice = voiceIt->second;
	PlaybackTracking& tracking = trackingIt->second;

	// 現在のバッファを捨てて、目的位置から再投入する
	HRESULT hr = sourceVoice->Stop(0);
	if (FAILED(hr))
	{
		return false;
	}
	sourceVoice->FlushSourceBuffers();

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = soundData.bufferSize;
	buffer.pAudioData = soundData.buffer.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.PlayBegin = static_cast<UINT32>(targetSample);
	buffer.PlayLength = static_cast<UINT32>(totalSamples - targetSample);
	if (tracking.loop)
	{
		// ループは曲全体を対象にする。シーク位置から末尾までを繰り返すのは意図しない挙動のため。
		buffer.LoopBegin = 0;
		buffer.LoopLength = static_cast<UINT32>(totalSamples);
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	hr = sourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
	{
		return false;
	}

	// 位置計算の基準を今の SamplesPlayed に更新する
	XAUDIO2_VOICE_STATE state = {};
	sourceVoice->GetState(&state);
	tracking.baseSamplesPlayed = state.SamplesPlayed;
	tracking.startSample = targetSample;

	// ポーズ中にシークした場合は停止状態を維持する
	auto pausedIt = pausedMap_.find(name);
	const bool isPaused = pausedIt != pausedMap_.end() && pausedIt->second;
	if (!isPaused)
	{
		hr = sourceVoice->Start(0);
		if (FAILED(hr))
		{
			return false;
		}
	}

	return true;
}

void Audio::StopWave(const std::string& name)
{
	auto it = sourceVoiceMap_.find(name);
	if (it == sourceVoiceMap_.end())
	{
		return;
	}

	it->second->Stop(0);
	it->second->FlushSourceBuffers();
	it->second->DestroyVoice();
	RemoveFromGroupMap(it->second);
	sourceVoiceMap_.erase(it);
	pausedMap_.erase(name);
	playbackTrackingMap_.erase(name);
}

void Audio::StopGroup(SoundGroup group)
{
	auto it = groupVoicesMap_.find(group);
	if (it == groupVoicesMap_.end())
	{
		return;
	}

	for (auto& voice : it->second)
	{
		voice->Stop(0);
		voice->FlushSourceBuffers();
		voice->DestroyVoice();
	}
	it->second.clear();

	for (auto mapIt = sourceVoiceMap_.begin(); mapIt != sourceVoiceMap_.end();)
	{
		auto soundIt = soundDataMap_.find(mapIt->first);
		if (soundIt != soundDataMap_.end() && soundIt->second.group == group)
		{
			playbackTrackingMap_.erase(mapIt->first);
			pausedMap_.erase(mapIt->first);
			mapIt = sourceVoiceMap_.erase(mapIt);
		}
		else
		{
			++mapIt;
		}
	}
}

void Audio::StopAll()
{
	for (auto& pair : sourceVoiceMap_)
	{
		if (pair.second)
		{
			pair.second->Stop(0);
			pair.second->FlushSourceBuffers();
			pair.second->DestroyVoice();
		}
	}
	sourceVoiceMap_.clear();
	pausedMap_.clear();
	playbackTrackingMap_.clear();

	for (auto& pair : groupVoicesMap_)
	{
		pair.second.clear();
	}
}

void Audio::Pause(const std::string& name)
{
	auto it = sourceVoiceMap_.find(name);
	if (it != sourceVoiceMap_.end())
	{
		it->second->Stop(0);
		pausedMap_[name] = true;
	}
}

void Audio::Resume(const std::string& name)
{
	auto it = sourceVoiceMap_.find(name);
	if (it != sourceVoiceMap_.end())
	{
		it->second->Start(0);
		pausedMap_[name] = false;
	}
}

void Audio::PauseGroup(SoundGroup group)
{
	auto it = groupVoicesMap_.find(group);
	if (it == groupVoicesMap_.end())
	{
		return;
	}

	for (auto& voice : it->second)
	{
		voice->Stop(0);
	}

	for (auto& pair : sourceVoiceMap_)
	{
		auto soundIt = soundDataMap_.find(pair.first);
		if (soundIt != soundDataMap_.end() && soundIt->second.group == group)
		{
			pausedMap_[pair.first] = true;
		}
	}
}

void Audio::ResumeGroup(SoundGroup group)
{
	auto it = groupVoicesMap_.find(group);
	if (it == groupVoicesMap_.end())
	{
		return;
	}

	for (auto& voice : it->second)
	{
		voice->Start(0);
	}

	for (auto& pair : sourceVoiceMap_)
	{
		auto soundIt = soundDataMap_.find(pair.first);
		if (soundIt != soundDataMap_.end() && soundIt->second.group == group)
		{
			pausedMap_[pair.first] = false;
		}
	}
}

void Audio::PauseAll()
{
	for (auto& pair : sourceVoiceMap_)
	{
		pair.second->Stop(0);
		pausedMap_[pair.first] = true;
	}
}

void Audio::ResumeAll()
{
	for (auto& pair : sourceVoiceMap_)
	{
		pair.second->Start(0);
		pausedMap_[pair.first] = false;
	}
}

void Audio::SetVolume(const std::string& name, float volume)
{
	auto it = sourceVoiceMap_.find(name);
	if (it != sourceVoiceMap_.end())
	{
		it->second->SetVolume(ClampVolume(volume));
	}
}

void Audio::SetGroupVolume(SoundGroup group, float volume)
{
	auto it = groupVoicesMap_.find(group);
	if (it != groupVoicesMap_.end())
	{
		float v = ClampVolume(volume);
		for (auto& voice : it->second)
		{
			voice->SetVolume(v);
		}
	}
}

void Audio::SetMasterVolume(float volume)
{
	masterVolume_ = ClampVolume(volume);
	if (masterVoice_)
	{
		masterVoice_->SetVolume(masterVolume_);
	}
}

float Audio::GetMasterVolume() const
{
	return masterVolume_;
}

void Audio::SetPitch(const std::string& name, float pitch)
{
	auto it = sourceVoiceMap_.find(name);
	if (it != sourceVoiceMap_.end())
	{
		it->second->SetFrequencyRatio(ClampPitch(pitch));
	}
}

void Audio::FadeIn(const std::string& name, float duration, float targetVolume)
{
	auto it = sourceVoiceMap_.find(name);
	IXAudio2SourceVoice* sourceVoice = nullptr;

	if (it != sourceVoiceMap_.end())
	{
		sourceVoice = it->second;
	}
	else
	{
		auto soundIt = soundDataMap_.find(name);
		if (soundIt == soundDataMap_.end())
		{
			return;
		}

		SoundData& soundData = soundIt->second;
		HRESULT hr;

		if (submixVoiceDry_ && submixVoiceReverb_)
		{
			XAUDIO2_SEND_DESCRIPTOR sendDescs[2] = {};
			sendDescs[0].Flags = 0;
			sendDescs[0].pOutputVoice = submixVoiceDry_;
			sendDescs[1].Flags = 0;
			sendDescs[1].pOutputVoice = submixVoiceReverb_;

			XAUDIO2_VOICE_SENDS sendList = {};
			sendList.SendCount = 2;
			sendList.pSends = sendDescs;

			hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex, 0,
											XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList);
		}
		else
		{
			hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex);
		}

		if (FAILED(hr))
		{
			return;
		}

		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = soundData.bufferSize;
		buffer.pAudioData = soundData.buffer.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

		if (FAILED(sourceVoice->SubmitSourceBuffer(&buffer)) || FAILED(sourceVoice->Start(0)))
		{
			sourceVoice->DestroyVoice();
			return;
		}

		sourceVoiceMap_[name] = sourceVoice;
		groupVoicesMap_[soundData.group].push_back(sourceVoice);
		pausedMap_[name] = false;
	}

	sourceVoice->SetVolume(0.0f);

	FadeData fade = {};
	fade.sourceVoice = sourceVoice;
	fade.name = name;
	fade.startVolume = 0.0f;
	fade.targetVolume = ClampVolume(targetVolume);
	fade.currentTime = 0.0f;
	fade.duration = duration;
	fade.isFading = true;
	fadeList_.push_back(fade);
}

void Audio::FadeOut(const std::string& name, float duration, bool stopOnComplete)
{
	auto it = sourceVoiceMap_.find(name);
	if (it == sourceVoiceMap_.end())
	{
		return;
	}

	float currentVolume = 0.0f;
	it->second->GetVolume(&currentVolume);

	FadeData fade = {};
	fade.sourceVoice = it->second;
	fade.name = name;
	fade.startVolume = currentVolume;
	fade.targetVolume = 0.0f;
	fade.currentTime = 0.0f;
	fade.duration = duration;
	fade.isFading = true;
	fadeList_.push_back(fade);

	fadeOutStopMap_[it->second] = stopOnComplete;
}

bool Audio::IsPlaying(const std::string& name) const
{
	auto it = sourceVoiceMap_.find(name);
	if (it == sourceVoiceMap_.end())
	{
		return false;
	}

	auto pausedIt = pausedMap_.find(name);
	if (pausedIt != pausedMap_.end() && pausedIt->second)
	{
		return false;
	}

	XAUDIO2_VOICE_STATE state;
	it->second->GetState(&state);
	return state.BuffersQueued > 0;
}

bool Audio::IsPaused(const std::string& name) const
{
	auto it = pausedMap_.find(name);
	return it != pausedMap_.end() && it->second;
}

bool Audio::IsLoaded(const std::string& name) const
{
	return soundDataMap_.find(name) != soundDataMap_.end();
}

const std::vector<SoundData::WaveformPeak>* Audio::GetWaveformPeaks(const std::string& name) const
{
	auto it = soundDataMap_.find(name);
	return it != soundDataMap_.end() ? &it->second.waveformPeaks : nullptr;
}

float Audio::GetWaveformSecondsPerPeak(const std::string& name) const
{
	auto it = soundDataMap_.find(name);
	if (it == soundDataMap_.end() || it->second.wfex.nSamplesPerSec == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(it->second.waveformFramesPerPeak) / static_cast<float>(it->second.wfex.nSamplesPerSec);
}

void Audio::BuildWaveformPeaks(SoundData& soundData)
{
	const uint32_t channels = soundData.wfex.nChannels;
	const uint32_t bitsPerSample = soundData.wfex.wBitsPerSample;
	const uint32_t blockAlign = soundData.wfex.nBlockAlign;
	if (channels == 0 || blockAlign == 0 || soundData.wfex.nSamplesPerSec == 0 ||
		(bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32))
	{
		return;
	}

	const uint64_t frameCount = GetTotalSampleCount(soundData);
	const uint32_t framesPerPeak = (std::max)(
		static_cast<uint32_t>(soundData.wfex.nSamplesPerSec) / Audio::kWaveformPeaksPerSecond, 1u);
	soundData.waveformFramesPerPeak = framesPerPeak;
	const size_t peakCount = static_cast<size_t>((frameCount + framesPerPeak - 1) / framesPerPeak);
	soundData.waveformPeaks.reserve(peakCount);

	const BYTE* bytes = soundData.buffer.data();
	for (uint64_t firstFrame = 0; firstFrame < frameCount; firstFrame += framesPerPeak)
	{
		const uint64_t lastFrame = (std::min)(firstFrame + framesPerPeak, frameCount);
		float minimum = 1.0f;
		float maximum = -1.0f;
		for (uint64_t frame = firstFrame; frame < lastFrame; ++frame)
		{
			const BYTE* frameData = bytes + frame * blockAlign;
			for (uint32_t channel = 0; channel < channels; ++channel)
			{
				const BYTE* sample = frameData + channel * (bitsPerSample / 8);
				float value = 0.0f;
				switch (bitsPerSample)
				{
				case 8:
					value = (static_cast<float>(*sample) - 128.0f) / 128.0f;
					break;
				case 16:
				{
					int16_t pcm = 0;
					std::memcpy(&pcm, sample, sizeof(pcm));
					value = static_cast<float>(pcm) / 32768.0f;
					break;
				}
				case 24:
				{
					int32_t pcm = static_cast<int32_t>(sample[0]) |
						(static_cast<int32_t>(sample[1]) << 8) | (static_cast<int32_t>(sample[2]) << 16);
					if ((pcm & 0x00800000) != 0) { pcm |= static_cast<int32_t>(0xFF000000); }
					value = static_cast<float>(pcm) / 8388608.0f;
					break;
				}
				case 32:
				{
					int32_t pcm = 0;
					std::memcpy(&pcm, sample, sizeof(pcm));
					value = static_cast<float>(pcm) / 2147483648.0f;
					break;
				}
				}
				minimum = (std::min)(minimum, value);
				maximum = (std::max)(maximum, value);
			}
		}
		soundData.waveformPeaks.push_back({ minimum, maximum });
	}
}

void Audio::SetReverbEnabled(bool enabled)
{
	reverbEnabled_ = enabled;
	UpdateReverbVolume();
}

bool Audio::IsReverbEnabled() const
{
	return reverbEnabled_;
}

void Audio::SetReverbPreset(ReverbPreset preset)
{
	if (!submixVoiceReverb_)
	{
		return;
	}

	currentPreset_ = preset;

	XAUDIO2FX_REVERB_I3DL2_PARAMETERS i3dl2Params = GetReverbPresetParams(preset);
	XAUDIO2FX_REVERB_PARAMETERS nativeParams;
	ReverbConvertI3DL2ToNative(&i3dl2Params, &nativeParams);

	// WetDryMixは100%に固定（ボリュームで調整するため）
	nativeParams.WetDryMix = 100.0f;

	submixVoiceReverb_->SetEffectParameters(0, &nativeParams, sizeof(nativeParams));
}

void Audio::SetReverbAmount(float amount)
{
	reverbAmount_ = std::clamp(amount, 0.0f, 1.0f);
	UpdateReverbVolume();
}

float Audio::GetReverbAmount() const
{
	return reverbAmount_;
}

void Audio::UpdateReverbVolume()
{
	if (submixVoiceReverb_)
	{
		float reverbVol = reverbEnabled_ ? reverbAmount_ : 0.0f;
		submixVoiceReverb_->SetVolume(reverbVol);
	}
	if (submixVoiceDry_)
	{
		submixVoiceDry_->SetVolume(1.0f);
	}
}

void Audio::UnloadWave(const std::string& name)
{
	StopWave(name);

	auto it = soundDataMap_.find(name);
	if (it != soundDataMap_.end())
	{
		// std::vector handles memory automatically
		soundDataMap_.erase(it);
		loadedSoundNames_.erase(std::remove(loadedSoundNames_.begin(), loadedSoundNames_.end(), name), loadedSoundNames_.end());
	}
}

void Audio::UnloadAll()
{
	StopAll();

	for (auto& pair : soundDataMap_)
	{
		// std::vector handles memory automatically
	}
	soundDataMap_.clear();
	loadedSoundNames_.clear();
}

void Audio::InitializeEffect()
{
	// マスターボイスの詳細を取得
	XAUDIO2_VOICE_DETAILS masterDetails;
	masterVoice_->GetVoiceDetails(&masterDetails);

	UINT32 channels = masterDetails.InputChannels;
	UINT32 sampleRate = masterDetails.InputSampleRate;

	// ドライ用サブミックスボイスを作成（エフェクトなし）
	HRESULT hr = xAudio2_->CreateSubmixVoice(
		&submixVoiceDry_,
		channels,
		sampleRate,
		0, 0, nullptr, nullptr);

	if (FAILED(hr))
	{
		submixVoiceDry_ = nullptr;
	}

	// リバーブエフェクトを作成
	IUnknown* reverbEffect = nullptr;
	hr = XAudio2CreateReverb(&reverbEffect);
	if (FAILED(hr))
	{
		return;
	}

	// 【修正点】リバーブはステレオ(2ch)固定にする
	// マスターが7.1chなどの場合、リバーブエフェクトが対応していないチャンネル数になり失敗するのを防ぐ
	UINT32 reverbChannels = 2;

	XAUDIO2_EFFECT_DESCRIPTOR effectDesc = {};
	effectDesc.InitialState = TRUE;
	effectDesc.OutputChannels = reverbChannels; // エフェクト出力も2ch
	effectDesc.pEffect = reverbEffect;

	XAUDIO2_EFFECT_CHAIN effectChain = {};
	effectChain.EffectCount = 1;
	effectChain.pEffectDescriptors = &effectDesc;

	// リバーブ用サブミックスボイスを作成（2chで作成）
	hr = xAudio2_->CreateSubmixVoice(
		&submixVoiceReverb_,
		reverbChannels,
		sampleRate,
		0, 0, nullptr,
		&effectChain);

	reverbEffect->Release();

	if (FAILED(hr))
	{
		submixVoiceReverb_ = nullptr;
		return;
	}

	// 初期ボリュームを設定
	UpdateReverbVolume();
}

void Audio::RemoveFromGroupMap(IXAudio2SourceVoice* sourceVoice)
{
	for (auto& pair : groupVoicesMap_)
	{
		auto it = std::find(pair.second.begin(), pair.second.end(), sourceVoice);
		if (it != pair.second.end())
		{
			pair.second.erase(it);
			break;
		}
	}
}

float Audio::ClampVolume(float volume) const
{
	return std::clamp(volume, 0.0f, 1.0f);
}

float Audio::ClampPitch(float pitch) const
{
	return std::clamp(pitch, kMinPitch, kMaxPitch);
}

//=============================================================================
// ImGuiデバッグ機能
//=============================================================================
#ifdef USE_IMGUI

void Audio::DrawDebugWindow()
{
	// マスター設定
	if (ImGui::CollapsingHeader("Master", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::SliderFloat("Master Volume", &masterVolume_, 0.0f, 1.0f))
		{
			SetMasterVolume(masterVolume_);
		}

		if (ImGui::Button("Stop All"))
		{
			StopAll();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause All"))
		{
			PauseAll();
		}
		ImGui::SameLine();
		if (ImGui::Button("Resume All"))
		{
			ResumeAll();
		}

		ImGui::Text("Loaded: %zu | Playing: %zu | Fading: %zu",
					soundDataMap_.size(), sourceVoiceMap_.size(), fadeList_.size());
	}

	// グループ設定
	if (ImGui::CollapsingHeader("Groups", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const SoundGroup groups[] = {SoundGroup::BGM, SoundGroup::SE, SoundGroup::Voice, SoundGroup::Ambient};

		for (int i = 0; i < 4; ++i)
		{
			ImGui::PushID(i);
			if (ImGui::SliderFloat(GetGroupName(groups[i]), &debugData_.groupVolumes[i], 0.0f, 1.0f))
			{
				SetGroupVolume(groups[i], debugData_.groupVolumes[i]);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Stop"))
			{
				StopGroup(groups[i]);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Pause"))
			{
				PauseGroup(groups[i]);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Resume"))
			{
				ResumeGroup(groups[i]);
			}
			ImGui::PopID();
		}
	}

	// リバーブ設定
	if (ImGui::CollapsingHeader("Reverb", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool reverb = reverbEnabled_;
		if (ImGui::Checkbox("Enabled", &reverb))
		{
			SetReverbEnabled(reverb);
		}

		ImGui::Separator();
		ImGui::Text("Preset:");

		if (ImGui::BeginCombo("##ReverbPreset", kPresetList[debugData_.currentPreset].name))
		{
			for (int i = 0; i < kPresetCount; ++i)
			{
				bool isSelected = (debugData_.currentPreset == i);
				if (ImGui::Selectable(kPresetList[i].name, isSelected))
				{
					debugData_.currentPreset = i;
					SetReverbPreset(kPresetList[i].preset);
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		if (ImGui::SliderFloat("Reverb Amount", &debugData_.reverbAmount, 0.0f, 1.0f, "%.2f"))
		{
			SetReverbAmount(debugData_.reverbAmount);
		}
		ImGui::TextWrapped("0.0 = No reverb, 1.0 = Full reverb (added to dry signal)");
	}

	// 音声リスト
	if (ImGui::CollapsingHeader("Sounds", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (soundDataMap_.empty())
		{
			ImGui::TextDisabled("No sounds loaded.");
		}
		else
		{
			if (ImGui::BeginListBox("##SoundList", ImVec2(-FLT_MIN, 100)))
			{
				for (const auto& pair : soundDataMap_)
				{
					const std::string& name = pair.first;
					bool playing = IsPlaying(name);
					bool paused = IsPaused(name);

					std::string label = name + " [" + GetGroupName(pair.second.group) + "]";
					if (playing)
					{
						label += " (Playing)";
					}
					else if (paused)
					{
						label += " (Paused)";
					}

					if (ImGui::Selectable(label.c_str(), debugData_.selectedSound == name))
					{
						debugData_.selectedSound = name;

						auto voiceIt = sourceVoiceMap_.find(name);
						if (voiceIt != sourceVoiceMap_.end())
						{
							voiceIt->second->GetVolume(&debugData_.selectedVolume);
							voiceIt->second->GetFrequencyRatio(&debugData_.selectedPitch);
						}
						else
						{
							debugData_.selectedVolume = 1.0f;
							debugData_.selectedPitch = 1.0f;
						}
					}
				}
				ImGui::EndListBox();
			}

			if (!debugData_.selectedSound.empty() && IsLoaded(debugData_.selectedSound))
			{
				ImGui::Separator();
				ImGui::Text("Selected: %s", debugData_.selectedSound.c_str());

				bool playing = IsPlaying(debugData_.selectedSound);
				bool paused = IsPaused(debugData_.selectedSound);

				if (!playing && !paused)
				{
					ImGui::Checkbox("Loop", &debugData_.selectedLoop);
					ImGui::SameLine();
					if (ImGui::Button("Play"))
					{
						PlayWave(debugData_.selectedSound, debugData_.selectedLoop);
					}
				}
				else
				{
					if (ImGui::Button("Stop"))
					{
						StopWave(debugData_.selectedSound);
					}
					ImGui::SameLine();
					if (paused)
					{
						if (ImGui::Button("Resume"))
						{
							Resume(debugData_.selectedSound);
						}
					}
					else
					{
						if (ImGui::Button("Pause"))
						{
							Pause(debugData_.selectedSound);
						}
					}
				}

				if (ImGui::SliderFloat("Volume", &debugData_.selectedVolume, 0.0f, 1.0f))
				{
					SetVolume(debugData_.selectedSound, debugData_.selectedVolume);
				}

				if (ImGui::SliderFloat("Pitch", &debugData_.selectedPitch, 0.5f, 2.0f))
				{
					SetPitch(debugData_.selectedSound, debugData_.selectedPitch);
				}

				ImGui::Separator();
				ImGui::Text("Fade");
				ImGui::SliderFloat("Duration", &debugData_.fadeDuration, 0.1f, 5.0f, "%.1f s");
				ImGui::SliderFloat("Target Vol", &debugData_.fadeTargetVolume, 0.0f, 1.0f);

				if (ImGui::Button("Fade In"))
				{
					FadeIn(debugData_.selectedSound, debugData_.fadeDuration, debugData_.fadeTargetVolume);
				}
				ImGui::SameLine();
				ImGui::Checkbox("Stop##FadeOut", &debugData_.fadeOutStop);
				ImGui::SameLine();
				if (ImGui::Button("Fade Out"))
				{
					FadeOut(debugData_.selectedSound, debugData_.fadeDuration, debugData_.fadeOutStop);
				}

				ImGui::Separator();
				if (ImGui::Button("Unload"))
				{
					UnloadWave(debugData_.selectedSound);
					debugData_.selectedSound.clear();
				}
			}
		}
	}
}

#endif
} // namespace KCE
