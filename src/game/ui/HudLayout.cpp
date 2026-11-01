#include "HudLayout.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace soulknight::game::ui {
namespace {
using Json = nlohmann::json;

constexpr float kLogicalWidth = 1920.0f;
constexpr float kLogicalHeight = 1080.0f;

std::vector<HudElement> DefaultElements() {
    return {
        // The three status rows are the actual reference bar slots inside the
        // recovered ui_15 plate. The plate itself is derived from their union
        // in GameUI so UI Design Mode still edits the real HP/armor/energy
        // elements rather than a separate debug-only widget.
        {"hp",            {90.0f, 26.0f, 276.0f, 36.0f}},
        {"armor",         {90.0f, 72.0f, 276.0f, 36.0f}},
        {"energy",        {90.0f, 118.0f, 276.0f, 36.0f}},
        {"coin",          {1652.0f, 20.0f, 118.0f, 44.0f}},
        {"pause",         {1812.0f, 16.0f, 92.0f, 82.0f}},
        {"minimap",       {1664.0f, 112.0f, 240.0f, 196.0f}},
        {"ability",       {20.0f, 846.0f, 150.0f, 150.0f}},
        {"weapon_slots",  {190.0f, 986.0f, 220.0f, 80.0f}},
        {"active_weapon", {190.0f, 940.0f, 220.0f, 30.0f}},
        {"pickup_prompt", {700.0f, 886.0f, 520.0f, 148.0f}},
        {"stage",         {16.0f, 190.0f, 360.0f, 26.0f}},
    };
}

engine::Rect ReadRect(const Json& value) {
    engine::Rect rect{};
    if (!value.is_object()) return rect;
    rect.x = value.value("x", 0.0f);
    rect.y = value.value("y", 0.0f);
    rect.w = value.value("w", 0.0f);
    rect.h = value.value("h", 0.0f);
    return rect;
}

Json WriteRect(const engine::Rect& rect) {
    return Json{{"x", rect.x}, {"y", rect.y},
                {"w", rect.w}, {"h", rect.h}};
}

bool ValidRect(const engine::Rect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.w) && std::isfinite(rect.h) &&
           rect.w > 0.0f && rect.h > 0.0f;
}

} // namespace

HudLayout::HudLayout() { ResetToDefaults(); }

void HudLayout::ResetToDefaults() {
    elements_ = DefaultElements();
    default_count_ = elements_.size();
    is_default_ = true;
    RebuildIndex();
}

void HudLayout::RebuildIndex() {}

const HudElement* FindHudElement(const std::vector<HudElement>& elements,
                                 const std::string& id) {
    for (const HudElement& element : elements) {
        if (element.id == id) return &element;
    }
    return nullptr;
}

const engine::Rect& HudLayout::RectFor(const std::string& id) const {
    static const engine::Rect empty{};
    const HudElement* element = FindHudElement(elements_, id);
    return element != nullptr ? element->rect : empty;
}

engine::Rect* HudLayout::MutableRectFor(const std::string& id) {
    for (HudElement& element : elements_) {
        if (element.id == id) return &element.rect;
    }
    return nullptr;
}

const HudElement* HudLayout::HitTest(const engine::Vec2& logical) const {
    // Reverse order makes the top-most drawn element win when overlapping.
    for (auto it = elements_.rbegin(); it != elements_.rend(); ++it) {
        const engine::Rect& r = it->rect;
        if (logical.x >= r.x && logical.x <= r.x + r.w &&
            logical.y >= r.y && logical.y <= r.y + r.h) {
            return &*it;
        }
    }
    return nullptr;
}

bool HudLayout::MoveTo(const std::string& id, const engine::Vec2& top_left) {
    engine::Rect* rect = MutableRectFor(id);
    if (rect == nullptr || !std::isfinite(top_left.x) ||
        !std::isfinite(top_left.y)) {
        return false;
    }
    rect->x = std::clamp(top_left.x, 0.0f, kLogicalWidth - rect->w);
    rect->y = std::clamp(top_left.y, 0.0f, kLogicalHeight - rect->h);
    is_default_ = false;
    return true;
}

bool HudLayout::MoveBy(const std::string& id, const engine::Vec2& delta) {
    engine::Rect* rect = MutableRectFor(id);
    if (rect == nullptr) return false;
    return MoveTo(id, {rect->x + delta.x, rect->y + delta.y});
}

bool HudLayout::Load(const std::filesystem::path& path, std::string* error) {
    ResetToDefaults();
    if (error != nullptr) error->clear();
    if (!std::filesystem::exists(path)) return false;

    std::ifstream stream(path);
    if (!stream) {
        if (error != nullptr) *error = "could not open UI layout";
        return false;
    }
    Json document = Json::parse(stream, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        if (error != nullptr) *error = "UI layout is malformed";
        return false;
    }
    // Version 2 moves the HUD onto the recovered reference plate and compact
    // minimap. Version 1 layouts contain the old native anchors; resetting them
    // is intentional so a stale 0.5.1 file cannot keep the new HUD misaligned.
    const int version = document.value("version", 0);
    if (version != 2) {
        if (error != nullptr) *error = version == 1
            ? "UI layout version 1 migrated to version 2 defaults"
            : "unsupported UI layout version";
        return false;
    }
    if (!document.contains("elements") || !document["elements"].is_object()) {
        if (error != nullptr) *error = "UI layout has no elements object";
        return false;
    }
    std::size_t loaded_count = 0;
    for (const auto& [id, value] : document["elements"].items()) {
        engine::Rect rect = ReadRect(value);
        if (!ValidRect(rect)) continue;
        engine::Rect* target = MutableRectFor(id);
        if (target == nullptr) continue;
        *target = rect;
        ++loaded_count;
    }
    is_default_ = loaded_count == 0;
    return loaded_count > 0;
}

bool HudLayout::Save(const std::filesystem::path& path,
                     std::string* error) const {
    if (error != nullptr) error->clear();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (error != nullptr) *error = ec.message();
        return false;
    }
    Json elements = Json::object();
    for (const HudElement& element : elements_) {
        elements[element.id] = WriteRect(element.rect);
    }
    Json document = Json{{"version", 2},
                         {"coordinate_space", "logical_1920x1080"},
                         {"elements", std::move(elements)}};
    std::ofstream stream(path, std::ios::trunc);
    if (!stream) {
        if (error != nullptr) *error = "could not write UI layout";
        return false;
    }
    stream << document.dump(2) << '\n';
    if (!stream.good()) {
        if (error != nullptr) *error = "UI layout write failed";
        return false;
    }
    return true;
}

} // namespace soulknight::game::ui
