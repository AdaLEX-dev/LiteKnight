// Soul Knight Linux — persistent logical-coordinate HUD layout
#pragma once

#include "engine/math/Rect.hpp"
#include "engine/math/Vec2.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace soulknight::game::ui {

struct HudElement {
    std::string id;
    engine::Rect rect;
};

// The HUD is authored in the 1920x1080 logical design space. These defaults are
// data, not scattered renderer constants; UI Design Mode edits the same list and
// serializes it to $XDG_CONFIG_HOME/liteknight/ui_layout.json.
class HudLayout {
public:
    HudLayout();

    void ResetToDefaults();
    const engine::Rect& RectFor(const std::string& id) const;
    engine::Rect* MutableRectFor(const std::string& id);
    const HudElement* HitTest(const engine::Vec2& logical) const;
    bool MoveTo(const std::string& id, const engine::Vec2& top_left);
    bool MoveBy(const std::string& id, const engine::Vec2& delta);

    bool Load(const std::filesystem::path& path, std::string* error);
    bool Save(const std::filesystem::path& path, std::string* error) const;

    const std::vector<HudElement>& Elements() const { return elements_; }
    bool IsDefault() const { return is_default_; }

private:
    void RebuildIndex();

    std::vector<HudElement> elements_;
    std::size_t default_count_ = 0;
    bool is_default_ = true;
};

const HudElement* FindHudElement(const std::vector<HudElement>& elements,
                                 const std::string& id);

} // namespace soulknight::game::ui
