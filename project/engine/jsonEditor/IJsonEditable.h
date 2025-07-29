#pragma once
#include <string>

/// JSON編集可能なインターフェース
class IJsonEditable
{
protected:
    virtual void DrawImGui() = 0;
    virtual bool LoadJson(const std::string& path) = 0;
    virtual bool SaveJson(const std::string& path) const = 0;
    virtual ~IJsonEditable() = default;
};

