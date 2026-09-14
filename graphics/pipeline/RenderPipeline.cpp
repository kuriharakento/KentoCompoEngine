#include "graphics/pipeline/RenderPipeline.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
RenderPipeline::~RenderPipeline()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

IRenderPass* RenderPipeline::AddPass(RenderPassPtr pass)
{
	if (!pass)
	{
		return nullptr;
	}

	IRenderPass* raw = pass.get();
	passes_.push_back(std::move(pass));
	return raw;
}

IRenderPass* RenderPipeline::InsertPass(size_t index, RenderPassPtr pass)
{
	if (!pass)
	{
		return nullptr;
	}

	IRenderPass* raw = pass.get();
	if (index >= passes_.size())
	{
		passes_.push_back(std::move(pass));
	}
	else
	{
		passes_.insert(passes_.begin() + index, std::move(pass));
	}
	return raw;
}

IRenderPass* RenderPipeline::InsertPassAfter(const std::string& passName, RenderPassPtr pass)
{
	const auto it = std::find_if(passes_.begin(), passes_.end(),
		[&passName](const RenderPassPtr& existing) { return passName == existing->GetName(); });

	if (it == passes_.end())
	{
		return AddPass(std::move(pass));
	}

	const size_t index = static_cast<size_t>(it - passes_.begin()) + 1;
	return InsertPass(index, std::move(pass));
}

IRenderPass* RenderPipeline::FindPass(const std::string& passName) const
{
	const auto it = std::find_if(passes_.begin(), passes_.end(),
		[&passName](const RenderPassPtr& existing) { return passName == existing->GetName(); });
	return it != passes_.end() ? it->get() : nullptr;
}

bool RenderPipeline::RemovePass(const std::string& passName)
{
	const auto it = std::find_if(passes_.begin(), passes_.end(),
		[&passName](const RenderPassPtr& existing) { return passName == existing->GetName(); });

	if (it == passes_.end())
	{
		return false;
	}

	passes_.erase(it);
	return true;
}

void RenderPipeline::Execute(const RenderPassContext& ctx)
{
	// 必要なものが揃っていない状態で走らせると、
	// バリアの状態だけ進んで次のフレームで壊れる。何もせずに返る。
	if (!ctx.IsValid())
	{
		return;
	}

	for (const auto& pass : passes_)
	{
		if (pass && pass->ShouldExecute(ctx))
		{
			pass->Execute(ctx);
		}
	}
}

void RenderPipeline::Clear()
{
	passes_.clear();
}

#ifdef USE_IMGUI
void RenderPipeline::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "レンダリング", "レンダーパイプライン", [this]() { this->DrawImGui(); });
}

void RenderPipeline::DrawImGui()
{
	ImGui::TextDisabled("描画順 (%zu パス)", passes_.size());
	ImGui::Separator();

	for (size_t i = 0; i < passes_.size(); ++i)
	{
		IRenderPass* pass = passes_[i].get();
		if (!pass)
		{
			continue;
		}

		ImGui::PushID(static_cast<int>(i));

		bool enabled = pass->IsEnabled();
		if (ImGui::Checkbox("##enabled", &enabled))
		{
			pass->SetEnabled(enabled);
		}

		ImGui::SameLine();
		if (enabled)
		{
			ImGui::Text("%2zu. %s", i, pass->GetName());
		}
		else
		{
			ImGui::TextDisabled("%2zu. %s", i, pass->GetName());
		}

		ImGui::PopID();
	}
}
#endif
} // namespace KCE
