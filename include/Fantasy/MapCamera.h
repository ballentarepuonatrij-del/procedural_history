#pragma once
#include <algorithm>
#include <cmath>

namespace fantasy {
// World coordinates are in tiles, screen coordinates in client pixels.
// Pure C++: usable in Windows UI and cross-platform tests alike.
struct MapPoint { double x{},y{}; };
struct MapRect {
    int left{},top{},right{},bottom{};
    int width() const { return std::max(0,right-left); }
    int height() const { return std::max(0,bottom-top); }
    bool contains(int x,int y) const { return x>=left && x<right && y>=top && y<bottom; }
};
class MapCamera {
public:
    void reset(int tilesX,int tilesY) {
        tilesX_=std::max(1,tilesX);tilesY_=std::max(1,tilesY);
        zoom_=1.;centerX_=tilesX_/2.;centerY_=tilesY_/2.;clampCenter();
    }
    void setViewport(MapRect viewport) {
        viewport_=viewport;
        clampCenter();
    }
    double zoom() const { return zoom_; }
    double pixelsPerTile() const {
        if(!viewport_.width() || !viewport_.height()) return 1.;
        return zoom_*std::min(double(viewport_.width())/tilesX_,double(viewport_.height())/tilesY_);
    }
    MapPoint screenToWorld(double x,double y) const {
        double s=pixelsPerTile();
        return {centerX_+(x-midX())/s,centerY_+(y-midY())/s};
    }
    MapPoint worldToScreen(double x,double y) const {
        double s=pixelsPerTile();
        return {midX()+(x-centerX_)*s,midY()+(y-centerY_)*s};
    }
    MapRect drawnRect() const {
        MapPoint topLeft=worldToScreen(0,0);
        const double s=pixelsPerTile();
        int x=static_cast<int>(std::lround(topLeft.x)), y=static_cast<int>(std::lround(topLeft.y));
        return {x,y,x+static_cast<int>(std::lround(tilesX_*s)),y+static_cast<int>(std::lround(tilesY_*s))};
    }
    void zoomAt(double x,double y,double factor) {
        MapPoint anchor=screenToWorld(x,y);
        zoom_=std::clamp(zoom_*factor,1.,12.);
        const double s=pixelsPerTile();
        centerX_=anchor.x-(x-midX())/s;
        centerY_=anchor.y-(y-midY())/s;
        clampCenter();
    }
    void panPixels(double deltaX,double deltaY) {
        double s=pixelsPerTile();
        centerX_-=deltaX/s;centerY_-=deltaY/s;
        clampCenter();
    }
private:
    int tilesX_{88},tilesY_{52};
    MapRect viewport_{};
    double zoom_{1.},centerX_{44.},centerY_{26.};
    double midX() const { return (viewport_.left+viewport_.right)*.5; }
    double midY() const { return (viewport_.top+viewport_.bottom)*.5; }
    void clampCenter() {
        const double s=pixelsPerTile();
        const double hx=viewport_.width()/(2*s),hy=viewport_.height()/(2*s);
        centerX_=hx>=tilesX_*.5?tilesX_*.5:std::clamp(centerX_,hx,tilesX_-hx);
        centerY_=hy>=tilesY_*.5?tilesY_*.5:std::clamp(centerY_,hy,tilesY_-hy);
    }
};
} // namespace fantasy
