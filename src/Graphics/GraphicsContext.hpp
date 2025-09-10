#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

// Graphics mode information
struct GraphicsModeInfo {
    int width;
    int height;
    int maxColors;
    
    GraphicsModeInfo(int w, int h, int colors) : width(w), height(h), maxColors(colors) {}
};

// Graphics context for drawing operations
class GraphicsContext {
public:
    GraphicsContext();
    
    // Mode management
    void setMode(int mode, uint8_t* pixelBuffer);
    GraphicsModeInfo getModeInfo() const { return modeInfo_; }
    
    // State management
    void setCurrentPoint(int x, int y);
    std::pair<int, int> getCurrentPoint() const { return {currentX_, currentY_}; }
    void setDefaultColor(uint8_t color) { defaultColor_ = color; }
    uint8_t getDefaultColor() const { return defaultColor_; }
    
    // Basic drawing primitives
    bool pset(int x, int y, int color = -1, bool step = false);
    bool line(int x1, int y1, int x2, int y2, int color = -1, bool stepStart = false, bool stepEnd = false);
    bool lineToLastPoint(int x2, int y2, int color = -1, bool step = false);
    bool rectangle(int x1, int y1, int x2, int y2, int color = -1, bool filled = false, bool step = false);
    bool circle(int cx, int cy, int radius, int color = -1, bool step = false);
    
    // Block operations (GET/PUT)
    bool getBlock(int x1, int y1, int x2, int y2, std::vector<uint8_t>& data, bool step = false);
    bool putBlock(int x, int y, const std::vector<uint8_t>& data, const char* mode = "PSET", bool step = false);
    
    // Utility functions
    uint8_t getPixel(int x, int y) const;
    bool isValidCoordinate(int x, int y) const;
    
    // Coordinate transformation for STEP mode
    std::pair<int, int> resolveCoordinates(int x, int y, bool step) const;
    
private:
    GraphicsModeInfo modeInfo_;
    uint8_t* pixelBuffer_;
    int currentX_, currentY_;
    uint8_t defaultColor_;
    
    // Internal drawing methods
    void plotPixel(int x, int y, uint8_t color);
    void drawLineBresenham(int x1, int y1, int x2, int y2, uint8_t color);
    void drawCircleMidpoint(int cx, int cy, int radius, uint8_t color);
    void fillRectangle(int x1, int y1, int x2, int y2, uint8_t color);
    void drawRectangleOutline(int x1, int y1, int x2, int y2, uint8_t color);
    
    // Clipping helpers
    bool clipLine(int& x1, int& y1, int& x2, int& y2) const;
    void clipRectangle(int& x1, int& y1, int& x2, int& y2) const;
};
