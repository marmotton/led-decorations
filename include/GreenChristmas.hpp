#ifndef GREENCHRISTMAS_HPP
#define GREENCHRISTMAS_HPP

#include <FastLED.h>
#include <vector>
#include <algorithm>

class GreenChristmas {
private:
    std::vector< std::vector<CRGB> > &ledmatrix;

    std::vector< std::vector<uint8_t> > dots;

    static CRGB applyPalette(uint8_t color) {
        return (CRGB)ColorFromPalette(colorPalette, color);
    }

    static CHSVPalette16 colorPalette;

public:
    GreenChristmas(std::vector< std::vector<CRGB> > &_ledmatrix) : ledmatrix(_ledmatrix) {
        // Initialize the dots with some green color (see the color palette)
        dots.resize(_ledmatrix.size(), std::vector<uint8_t>(_ledmatrix[0].size(), 63) );
    }

    void nextFrame()
    {
        // Compute the new colors
        for ( auto &row : dots ) {
            for (uint8_t &color : row) {
                int change = 0;

                if( color < 128 ) {
                    // Slight twinkle in green areas
                    change = random8(3) - 1;
                }
                else {
                    // More twinkle on candles and decorations
                    change = random8(9) - 4;
                }
                color += change;
                
                // Light some candles in green areas
                if( random16() < 100 && color < 128 ) {
                    color = 160;
                };

                // Blow out some candles
                if(random16() < 200 && color > 127) {
                    color = 31;
                };
            }
        }

        // Assign the new colors
        int rownum = 0;
        for ( auto row : dots) {
            std::transform(row.begin(), row.end(), ledmatrix[rownum].begin(), applyPalette);
            rownum++;
        }        
    }
};

CHSVPalette16 GreenChristmas::colorPalette = {
    // Greens 0-127
    CHSV(90, 255, 160),
    CHSV(96, 255, 150),
    CHSV(96, 255, 140),
    CHSV(105, 255, 160),
    CHSV(90, 255, 160),
    CHSV(96, 255, 150),
    CHSV(96, 255, 140),
    CHSV(105, 255, 160),

    // Candles 128-191
    CHSV(40, 255, 150),
    CHSV(60, 255, 220),
    CHSV(64, 255, 230),
    CHSV(70, 255, 160),

    // Decorations 192-255
    CHSV(0, 255, 255),
    CHSV(224, 255, 255),
    CHSV(192, 255, 255),
    CHSV(160, 255, 255)
};

#endif