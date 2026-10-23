#include "input/SensorInput.h"
#include "base/Logger.h"
#include "input/SerialPort.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// これだけデータが来なければ IsConnected を false にする
		constexpr auto kConnectedTimeout = std::chrono::milliseconds(500);
		// これだけデータが来なければポートを閉じて開き直す（抜き差しされたとき用）
		constexpr auto kReopenTimeout = std::chrono::seconds(3);
		// ポートが開けなかったときに次に試すまでの間
		constexpr auto kRetryInterval = std::chrono::milliseconds(500);
		// 1 行の長さの上限。改行が来ないまま超えたら、壊れたデータとして捨てる
		constexpr size_t kMaxLineLength = 256;

		// 名前に使える文字
		bool IsNameChar(char c)
		{
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
		}

		void SkipSpaces(const char*& p)
		{
			while (*p == ' ' || *p == '\t')
			{
				++p;
			}
		}
	} // namespace

	std::unique_ptr<SensorInput> SensorInput::instance_ = nullptr;

	SensorInput* SensorInput::GetInstance()
	{
		if (!instance_)
		{
			instance_ = std::make_unique<SensorInput>();
		}
		return instance_.get();
	}

	SensorInput::~SensorInput()
	{
		Finalize();
	}

	void SensorInput::Initialize()
	{
		current_ = {};
		previous_ = {};
		warnedNames_.clear();

#ifdef USE_IMGUI
		// 呼び直されても二重に出さない
		DebugUIManager::GetInstance()->Unregister(this);
		DebugUIManager::GetInstance()->RegisterSettingsPage(this, "システム", "センサー入力", [this]()
		{ this->DrawImGui(); });
#endif
	}

	void SensorInput::Finalize()
	{
		Disconnect();
#ifdef USE_IMGUI
		if (DebugUIManager::HasInstance())
		{
			DebugUIManager::GetInstance()->Unregister(this);
		}
#endif
	}

	void SensorInput::Connect(const std::string& portName, uint32_t baudRate)
	{
		Disconnect();

		portName_ = portName;
		baudRate_ = static_cast<int>(baudRate);
		{
			std::lock_guard lock(pendingMutex_);
			pending_ = {};
			pendingLinesThisFrame_ = 0;
			lastReceiveTime_ = {};
			lastLine_.clear();
			injected_ = false;
		}

		running_ = true;
		thread_ = std::thread(&SensorInput::ReceiveLoop, this, portName, baudRate);
	}

	void SensorInput::Disconnect()
	{
		if (!thread_.joinable())
		{
			return;
		}
		{
			std::lock_guard lock(stopMutex_);
			running_ = false;
		}
		stopCv_.notify_all();
		thread_.join();
	}

	void SensorInput::Update()
	{
		previous_ = current_;

		Frame next{};
		const Clock::time_point now = Clock::now();
		{
			std::lock_guard lock(pendingMutex_);
			const bool connected = (portOpen_ || injected_) && totalLines_ > 0 && now - lastReceiveTime_ < kConnectedTimeout;
			if (connected)
			{
				next = pending_;
				if (pendingLinesThisFrame_ == 0)
				{
					for (size_t i = 0; i < next.channelCount; ++i)
					{
						next.channels[i].max = next.channels[i].value;
						next.channels[i].min = next.channels[i].value;
					}
				}
			}
			next.connected = connected;
			pendingLinesThisFrame_ = 0;

			if (now - rateStartTime_ >= std::chrono::seconds(1))
			{
				const float seconds = std::chrono::duration<float>(now - rateStartTime_).count();
				linesPerSecond_ = static_cast<float>(totalLines_ - rateStartLines_) / seconds;
				rateStartLines_ = totalLines_;
				rateStartTime_ = now;
			}
		}
		current_ = next;

		if (current_.connected != loggedConnected_)
		{
			loggedConnected_ = current_.connected;
			Logger::Log(loggedConnected_ ? "SensorInput: 受信を始めた" : "SensorInput: 受信が途切れた",
						loggedConnected_ ? Logger::LogLevel::Info : Logger::LogLevel::Warning);
		}
	}

	const SensorInput::Channel* SensorInput::Find(const Frame& frame, std::string_view name)
	{
		for (size_t i = 0; i < frame.channelCount; ++i)
		{
			if (name == frame.channels[i].name.data())
			{
				return &frame.channels[i];
			}
		}
		return nullptr;
	}

	const SensorInput::Channel* SensorInput::FindCurrent(std::string_view name) const
	{
		const Channel* channel = Find(current_, name);
		if (!channel && current_.connected && std::find(warnedNames_.begin(), warnedNames_.end(), name) == warnedNames_.end())
		{
			warnedNames_.emplace_back(name);
			Logger::Log("SensorInput: \"" + std::string(name) + "\" が届いていない。名前の打ち間違いか、マイコン側の送り忘れ",
						Logger::LogLevel::Warning);
		}
		return channel;
	}

	bool SensorInput::Has(std::string_view name) const
	{
		return Find(current_, name) != nullptr;
	}

	float SensorInput::GetValue(std::string_view name) const
	{
		const Channel* channel = FindCurrent(name);
		return channel ? channel->value : 0.0f;
	}

	float SensorInput::GetPrevValue(std::string_view name) const
	{
		const Channel* channel = Find(previous_, name);
		return channel ? channel->value : 0.0f;
	}

	float SensorInput::GetMax(std::string_view name) const
	{
		const Channel* channel = FindCurrent(name);
		return channel ? channel->max : 0.0f;
	}

	float SensorInput::GetMin(std::string_view name) const
	{
		const Channel* channel = FindCurrent(name);
		return channel ? channel->min : 0.0f;
	}

	bool SensorInput::IsOn(std::string_view name, float threshold) const
	{
		const Channel* channel = FindCurrent(name);
		return channel && channel->max >= threshold;
	}

	bool SensorInput::IsTriggered(std::string_view name, float threshold) const
	{
		const Channel* prev = Find(previous_, name);
		return IsOn(name, threshold) && !(prev && prev->max >= threshold);
	}

	bool SensorInput::IsReleased(std::string_view name, float threshold) const
	{
		const Channel* prev = Find(previous_, name);
		return !IsOn(name, threshold) && prev && prev->max >= threshold;
	}

	void SensorInput::InjectLine(const std::string& line)
	{
		{
			std::lock_guard lock(pendingMutex_);
			injected_ = true;
		}
		ParseLine(line);
	}

	void SensorInput::ReceiveLoop(std::string portName, uint32_t baudRate)
	{
		SerialPort port;
		std::string line;
		char chunk[256];
		// 開いた直後の 1 行目は途中から始まっている可能性があるので捨てる
		bool skipFirstLine = true;
		Clock::time_point lastDataTime{};

		auto setPortOpen = [this](bool open)
		{
			std::lock_guard lock(pendingMutex_);
			portOpen_ = open;
		};

		while (running_)
		{
			if (!port.IsOpen())
			{
				if (!port.Open(portName, baudRate))
				{
					std::unique_lock lock(stopMutex_);
					stopCv_.wait_for(lock, kRetryInterval, [this]()
					{ return !running_; });
					continue;
				}
				line.clear();
				skipFirstLine = true;
				lastDataTime = Clock::now();
				setPortOpen(true);
			}

			const int readBytes = port.Read(chunk, sizeof(chunk));
			if (readBytes < 0 || (readBytes == 0 && Clock::now() - lastDataTime > kReopenTimeout))
			{
				port.Close();
				setPortOpen(false);
				continue;
			}
			if (readBytes == 0)
			{
				continue;
			}
			lastDataTime = Clock::now();

			for (int i = 0; i < readBytes; ++i)
			{
				const char c = chunk[i];
				if (c == '\n')
				{
					if (!skipFirstLine)
					{
						ParseLine(line);
					}
					skipFirstLine = false;
					line.clear();
				}
				else if (c != '\r')
				{
					line.push_back(c);
					if (line.size() > kMaxLineLength)
					{
						line.clear();
						skipFirstLine = true;
					}
				}
			}
		}

		port.Close();
		setPortOpen(false);
	}

	void SensorInput::ParseLine(const std::string& line)
	{
		Frame parsed{};
		bool ok = true;

		// "名前:値" をカンマ区切りで読む。1 つでも形が崩れていたら、行ごと捨てる
		const char* p = line.c_str();
		SkipSpaces(p);
		while (*p != '\0')
		{
			const char* nameBegin = p;
			while (IsNameChar(*p))
			{
				++p;
			}
			const size_t nameLength = static_cast<size_t>(p - nameBegin);
			SkipSpaces(p);
			if (nameLength == 0 || nameLength > kMaxNameLength || *p != ':')
			{
				ok = false;
				break;
			}
			++p;

			char* end = nullptr;
			const float v = std::strtof(p, &end);
			if (end == p)
			{
				ok = false;
				break;
			}
			p = end;

			// 同じ名前が 2 回あったら後のほうを使う
			const std::string_view name(nameBegin, nameLength);
			Channel* channel = const_cast<Channel*>(Find(parsed, name));
			if (!channel && parsed.channelCount < kMaxChannels)
			{
				channel = &parsed.channels[parsed.channelCount++];
				std::memcpy(channel->name.data(), nameBegin, nameLength);
			}
			if (channel)
			{
				channel->value = v;
			}

			SkipSpaces(p);
			if (*p == ',')
			{
				++p;
				SkipSpaces(p);
				if (*p == '\0')
				{
					ok = false;
					break;
				}
			}
			else if (*p != '\0')
			{
				ok = false;
				break;
			}
		}

		std::lock_guard lock(pendingMutex_);
		if (!ok || parsed.channelCount == 0)
		{
			// 文字だけの行（マイコン側のデバッグ出力など）もここに来る
			++rejectedLines_;
			lastRejectedLine_ = line;
			return;
		}

		// 同じフレームに前の行があれば、名前ごとに最大・最小を引き継ぐ
		for (size_t i = 0; i < parsed.channelCount; ++i)
		{
			Channel& channel = parsed.channels[i];
			channel.max = channel.value;
			channel.min = channel.value;
			if (pendingLinesThisFrame_ > 0)
			{
				if (const Channel* old = Find(pending_, channel.name.data()))
				{
					channel.max = (std::max)(old->max, channel.value);
					channel.min = (std::min)(old->min, channel.value);
				}
			}
		}
		pending_ = parsed;
		++pendingLinesThisFrame_;
		lastReceiveTime_ = Clock::now();
		lastLine_ = line;
		++totalLines_;
	}

	void SensorInput::DrawImGui()
	{
#ifdef USE_IMGUI
		static char portBuffer[32] = {};
		if (portBuffer[0] == '\0')
		{
			strncpy_s(portBuffer, portName_.c_str(), _TRUNCATE);
		}
		ImGui::InputText("ポート", portBuffer, sizeof(portBuffer));
		ImGui::InputInt("通信速度", &baudRate_);
		if (ImGui::Button("接続"))
		{
			Connect(portBuffer, static_cast<uint32_t>((std::max)(baudRate_, 1)));
		}
		ImGui::SameLine();
		if (ImGui::Button("切断"))
		{
			Disconnect();
		}

		std::string lastLine;
		std::string lastRejectedLine;
		bool portOpen = false;
		uint64_t totalLines = 0;
		uint64_t rejectedLines = 0;
		float linesPerSecond = 0.0f;
		{
			std::lock_guard lock(pendingMutex_);
			lastLine = lastLine_;
			lastRejectedLine = lastRejectedLine_;
			portOpen = portOpen_;
			totalLines = totalLines_;
			rejectedLines = rejectedLines_;
			linesPerSecond = linesPerSecond_;
		}

		const char* status = current_.connected ? "受信中" : (portOpen ? "ポートは開いたが、データが来ていない" : (running_ ? "ポートを開けない（再試行中）" : "未接続"));
		ImGui::Text("状態: %s", status);
		ImGui::Text("受信: %llu 行（%.0f 行/秒）", static_cast<unsigned long long>(totalLines), linesPerSecond);
		ImGui::Text("最後の行: %s", lastLine.c_str());
		if (rejectedLines > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "読めなかった行: %llu（最後: %s）", static_cast<unsigned long long>(rejectedLines), lastRejectedLine.c_str());
		}
		if (!warnedNames_.empty())
		{
			std::string names;
			for (const std::string& name : warnedNames_)
			{
				names += names.empty() ? name : ", " + name;
			}
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "読もうとしたが届いていない名前: %s", names.c_str());
		}

		ImGui::Separator();
		if (ImGui::BeginTable("SensorChannels", 5, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("名前");
			ImGui::TableSetupColumn("値");
			ImGui::TableSetupColumn("最大");
			ImGui::TableSetupColumn("最小");
			ImGui::TableSetupColumn("オン");
			ImGui::TableHeadersRow();
			for (size_t i = 0; i < current_.channelCount; ++i)
			{
				const Channel& channel = current_.channels[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(channel.name.data());
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", channel.value);
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", channel.max);
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", channel.min);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(channel.max >= 0.5f ? "ON" : "-");
			}
			ImGui::EndTable();
		}
#endif
	}
} // namespace KCE
