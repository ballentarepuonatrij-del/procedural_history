#pragma once

#include <array>
#include <optional>
#include <vector>

namespace fantasy {
// Screen-space geometry independent of GDI, usable by other map renderers.
struct LabelRect {
    int left{},top{},right{},bottom{};
};

inline bool labelRectanglesOverlap(const LabelRect& a,const LabelRect& b,int gap=0) {
    return a.left < b.right+gap && a.right+gap > b.left &&
           a.top < b.bottom+gap && a.bottom+gap > b.top;
}

class LabelLayout {
public:
    explicit LabelLayout(LabelRect viewport,int spacing=3):viewport_(viewport),spacing_(spacing) {}

    // Icons are always visible. Labels must stay clear of them as well as one another.
    void reserveSymbol(LabelRect bounds) { occupied_.push_back(bounds); }

    // Try eight stable positions around an anchor (or centered first for geographic names).
    // A label that cannot fit is omitted; we never draw over an existing label or marker.
    std::optional<LabelRect> place(int x,int y,int width,int height,bool centerFirst=false) {
        if(width<=0 || height<=0) return std::nullopt;
        const std::array<LabelRect,9> options{{
            {x-width/2,y-height/2,x-width/2+width,y-height/2+height},
            {x+13,y-height/2,x+13+width,y-height/2+height},
            {x-13-width,y-height/2,x-13,y-height/2+height},
            {x-width/2,y-13-height,x-width/2+width,y-13},
            {x-width/2,y+13,x-width/2+width,y+13+height},
            {x+13,y-13-height,x+13+width,y-13},
            {x-13-width,y-13-height,x-13,y-13},
            {x+13,y+13,x+13+width,y+13+height},
            {x-13-width,y+13,x-13,y+13+height}
        }};
        const int first=centerFirst?0:1;
        for(int i=0;i<9;++i) {
            const LabelRect candidate=options[static_cast<size_t>((first+i)%9)];
            if(candidate.left<viewport_.left+2 || candidate.top<viewport_.top+2 ||
               candidate.right>viewport_.right-2 || candidate.bottom>viewport_.bottom-2)
                continue;
            bool clash=false;
            for(const auto& used:occupied_)if(labelRectanglesOverlap(candidate,used,spacing_)) {
                clash=true;break;
            }
            if(clash) continue;
            occupied_.push_back(candidate);
            return candidate;
        }
        return std::nullopt;
    }

    const std::vector<LabelRect>& occupied() const {return occupied_;}

private:
    LabelRect viewport_{};
    int spacing_{};
    std::vector<LabelRect> occupied_;
};
} // namespace fantasy
