#include "GraphicsContext.hpp"
#include <string>
#include <cstring>
#include <algorithm>

GraphicsContext::GraphicsContext() 
    : modeInfo_(320, 200, 16), pixelBuffer_(nullptr), currentX_(0), currentY_(0), defaultColor_(15) {
}

void GraphicsContext::setMode(int mode, uint8_t* pixelBuffer) {
    pixelBuffer_ = pixelBuffer;
    
    // Set mode-specific properties
    switch (mode) {
        case 0:  // Text mode - no graphics
            modeInfo_ = GraphicsModeInfo(80, 25, 16);
            break;
        case 1:  // CGA 320x200, 4 colors
            modeInfo_ = GraphicsModeInfo(320, 200, 4);
            break;
        case 2:  // CGA 640x200, 2 colors
            modeInfo_ = GraphicsModeInfo(640, 200, 2);
            break;
        case 7:  // EGA 320x200, 16 colors
            modeInfo_ = GraphicsModeInfo(320, 200, 16);
            break;
        case 8:  // EGA 640x200, 16 colors
            modeInfo_ = GraphicsModeInfo(640, 200, 16);
            break;
        case 9:  // EGA 640x350, 16 colors
            modeInfo_ = GraphicsModeInfo(640, 350, 16);
            break;
        case 10: // EGA 640x350, 4 colors
            modeInfo_ = GraphicsModeInfo(640, 350, 4);
            break;
        case 11: // VGA 640x480, 2 colors
            modeInfo_ = GraphicsModeInfo(640, 480, 2);
            break;
        case 12: // VGA 640x480, 16 colors
            modeInfo_ = GraphicsModeInfo(640, 480, 16);
            break;
        case 13: // VGA 320x200, 256 colors
            modeInfo_ = GraphicsModeInfo(320, 200, 256);
            break;
        default:
            modeInfo_ = GraphicsModeInfo(320, 200, 16);
            break;
    }
}

void GraphicsContext::setCurrentPoint(int x, int y) {
    currentX_ = x;
    currentY_ = y;
}

std::pair<int, int> GraphicsContext::resolveCoordinates(int x, int y, bool step) const {
    if (step) {
        return {currentX_ + x, currentY_ + y};
    }
    return {x, y};
}

bool GraphicsContext::isValidCoordinate(int x, int y) const {
    return x >= 0 && x < modeInfo_.width && y >= 0 && y < modeInfo_.height;
}

uint8_t GraphicsContext::getPixel(int x, int y) const {
    if (!pixelBuffer_ || !isValidCoordinate(x, y)) {
        return 0;
    }
    return pixelBuffer_[y * modeInfo_.width + x];
}

void GraphicsContext::plotPixel(int x, int y, uint8_t color) {
    if (!pixelBuffer_ || !isValidCoordinate(x, y)) {
        return;
    }
    
    // Clamp color to valid range for current mode
    color = color % modeInfo_.maxColors;
    pixelBuffer_[y * modeInfo_.width + x] = color;
}

bool GraphicsContext::pset(int x, int y, int color, bool step) {
    auto coords = resolveCoordinates(x, y, step);
    
    if (!isValidCoordinate(coords.first, coords.second)) {
        return false;
    }
    
    uint8_t drawColor = (color >= 0) ? static_cast<uint8_t>(color) : defaultColor_;
    plotPixel(coords.first, coords.second, drawColor);
    
    // Update current point
    setCurrentPoint(coords.first, coords.second);
    
    return true;
}

bool GraphicsContext::line(int x1, int y1, int x2, int y2, int color, bool stepStart, bool stepEnd) {
    auto startCoords = resolveCoordinates(x1, y1, stepStart);
    auto endCoords = resolveCoordinates(x2, y2, stepEnd);
    
    uint8_t drawColor = (color >= 0) ? static_cast<uint8_t>(color) : defaultColor_;
    
    // Clip line to screen bounds
    int clippedX1 = startCoords.first, clippedY1 = startCoords.second;
    int clippedX2 = endCoords.first, clippedY2 = endCoords.second;
    
    if (!clipLine(clippedX1, clippedY1, clippedX2, clippedY2)) {
        return false; // Line completely outside screen
    }
    
    drawLineBresenham(clippedX1, clippedY1, clippedX2, clippedY2, drawColor);
    
    // Update current point to end of line
    setCurrentPoint(endCoords.first, endCoords.second);
    
    return true;
}

bool GraphicsContext::lineToLastPoint(int x2, int y2, int color, bool step) {
    return line(currentX_, currentY_, x2, y2, color, false, step);
}

bool GraphicsContext::rectangle(int x1, int y1, int x2, int y2, int color, bool filled, bool step) {
    auto coords1 = resolveCoordinates(x1, y1, step);
    auto coords2 = resolveCoordinates(x2, y2, step);
    
    uint8_t drawColor = (color >= 0) ? static_cast<uint8_t>(color) : defaultColor_;
    
    // Ensure proper ordering
    int minX = std::min(coords1.first, coords2.first);
    int maxX = std::max(coords1.first, coords2.first);
    int minY = std::min(coords1.second, coords2.second);
    int maxY = std::max(coords1.second, coords2.second);
    
    // Clip to screen bounds
    clipRectangle(minX, minY, maxX, maxY);
    
    if (filled) {
        fillRectangle(minX, minY, maxX, maxY, drawColor);
    } else {
        drawRectangleOutline(minX, minY, maxX, maxY, drawColor);
    }
    
    // Update current point to second corner
    setCurrentPoint(coords2.first, coords2.second);
    
    return true;
}

bool GraphicsContext::circle(int cx, int cy, int radius, int color, bool step) {
    auto center = resolveCoordinates(cx, cy, step);
    
    if (radius < 0) {
        return false; // Invalid radius
    }
    
    uint8_t drawColor = (color >= 0) ? static_cast<uint8_t>(color) : defaultColor_;
    
    drawCircleMidpoint(center.first, center.second, radius, drawColor);
    
    // Update current point to center
    setCurrentPoint(center.first, center.second);
    
    return true;
}

bool GraphicsContext::getBlock(int x1, int y1, int x2, int y2, std::vector<uint8_t>& data, bool step) {
    auto coords1 = resolveCoordinates(x1, y1, step);
    auto coords2 = resolveCoordinates(x2, y2, step);
    
    // Ensure proper ordering
    int minX = std::min(coords1.first, coords2.first);
    int maxX = std::max(coords1.first, coords2.first);
    int minY = std::min(coords1.second, coords2.second);
    int maxY = std::max(coords1.second, coords2.second);
    
    // Clip to screen bounds
    clipRectangle(minX, minY, maxX, maxY);
    
    int width = maxX - minX + 1;
    int height = maxY - minY + 1;
    
    // Format: [width, height, pixel_data...]
    data.clear();
    data.push_back(static_cast<uint8_t>(width & 0xFF));
    data.push_back(static_cast<uint8_t>((width >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(height & 0xFF));
    data.push_back(static_cast<uint8_t>((height >> 8) & 0xFF));
    
    // Copy pixel data
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            data.push_back(getPixel(x, y));
        }
    }
    
    return true;
}

bool GraphicsContext::putBlock(int x, int y, const std::vector<uint8_t>& data, const char* mode, bool step) {
    if (data.size() < 4) {
        return false; // Invalid data format
    }
    
    auto coords = resolveCoordinates(x, y, step);
    
    int width = data[0] | (data[1] << 8);
    int height = data[2] | (data[3] << 8);
    
    if (data.size() < static_cast<size_t>(4 + width * height)) {
        return false; // Not enough data
    }
    
    // Determine blend mode
    bool isXor = (mode && std::string(mode) == "XOR");
    
    // Draw pixels
    size_t dataIndex = 4;
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            int px = coords.first + dx;
            int py = coords.second + dy;
            
            if (isValidCoordinate(px, py)) {
                uint8_t newColor = data[dataIndex];
                
                if (isXor) {
                    uint8_t currentColor = getPixel(px, py);
                    newColor = currentColor ^ newColor;
                }
                
                plotPixel(px, py, newColor);
            }
            
            dataIndex++;
        }
    }
    
    return true;
}

void GraphicsContext::drawLineBresenham(int x1, int y1, int x2, int y2, uint8_t color) {
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1, y = y1;
    
    while (true) {
        plotPixel(x, y, color);
        
        if (x == x2 && y == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void GraphicsContext::drawCircleMidpoint(int cx, int cy, int radius, uint8_t color) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    
    auto plotCirclePoints = [&](int x, int y) {
        // 8-way symmetry
        plotPixel(cx + x, cy + y, color);
        plotPixel(cx - x, cy + y, color);
        plotPixel(cx + x, cy - y, color);
        plotPixel(cx - x, cy - y, color);
        plotPixel(cx + y, cy + x, color);
        plotPixel(cx - y, cy + x, color);
        plotPixel(cx + y, cy - x, color);
        plotPixel(cx - y, cy - x, color);
    };
    
    plotCirclePoints(x, y);
    
    while (x < y) {
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
        plotCirclePoints(x, y);
    }
}

void GraphicsContext::fillRectangle(int x1, int y1, int x2, int y2, uint8_t color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            plotPixel(x, y, color);
        }
    }
}

void GraphicsContext::drawRectangleOutline(int x1, int y1, int x2, int y2, uint8_t color) {
    // Top and bottom edges
    for (int x = x1; x <= x2; x++) {
        plotPixel(x, y1, color);
        plotPixel(x, y2, color);
    }
    
    // Left and right edges
    for (int y = y1; y <= y2; y++) {
        plotPixel(x1, y, color);
        plotPixel(x2, y, color);
    }
}

bool GraphicsContext::clipLine(int& x1, int& y1, int& x2, int& y2) const {
    // Simple clipping - could be improved with Cohen-Sutherland
    x1 = std::max(0, std::min(x1, modeInfo_.width - 1));
    y1 = std::max(0, std::min(y1, modeInfo_.height - 1));
    x2 = std::max(0, std::min(x2, modeInfo_.width - 1));
    y2 = std::max(0, std::min(y2, modeInfo_.height - 1));
    return true; // Always draw something for now
}

void GraphicsContext::clipRectangle(int& x1, int& y1, int& x2, int& y2) const {
    x1 = std::max(0, x1);
    y1 = std::max(0, y1);
    x2 = std::min(modeInfo_.width - 1, x2);
    y2 = std::min(modeInfo_.height - 1, y2);
}
