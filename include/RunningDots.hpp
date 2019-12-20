#ifndef RUNNINGDOTS_HPP
#define RUNNINGDOTS_HPP

#include <FastLED.h>
#include <vector>
#include <algorithm>

class RunningDots {
private:
    std::vector< std::vector<CRGB> > &ledmatrix;
    std::vector<uint8_t> intensities;
    std::vector<int> positions;
    int maxPosition;
    int minPosition;
    int tailLength;
    int headLength;
    int hue;

public:
    RunningDots(std::vector< std::vector<CRGB> > &_ledmatrix, int _tailLength, int _headLength) : ledmatrix(_ledmatrix), positions(_ledmatrix.size(), -_headLength-1) {
        tailLength = _tailLength;
        headLength = _headLength;
        maxPosition = _ledmatrix[0].size() + _tailLength;
        minPosition = -_headLength;

        hue = 204;

        // Tail and main dot intensities
        for ( int i = 0; i < _tailLength + 1; i++ ) {
            uint8_t intensity = triwave8( i * 80 / _tailLength + 47 );
            intensities.push_back( dim8_raw(intensity) );
        }
        // Head intensities
        for ( int i = 1; i < _headLength + 1; i++ ) {
            uint8_t intensity = triwave8( 127 + i * 80 / _headLength );
            intensities.push_back( dim8_raw(intensity) );
        }
    }

    void nextFrame() {
        for ( auto &position : positions ) {
            // Set new position for lines that are already running
            if ( position >= minPosition ) {
                position = (position < maxPosition) ? position+1 : minPosition-1;
            }

            // Start some lines randomly
            if ( position < minPosition && random16() < 400 ) {
                position = minPosition;
            }
        }
        
        // Set the LEDs intensities
        for ( uint rownum = 0; rownum < ledmatrix.size(); rownum++ ){
            if ( positions[rownum] >= 0 ) {
                for ( uint colnum = 0; colnum < ledmatrix[rownum].size(); colnum++ ){
                    uint intensitiesIndex = colnum - positions[rownum] + tailLength;
                    
                    if ( intensitiesIndex >= 0 && intensitiesIndex < intensities.size() ) {
                        ledmatrix[rownum][colnum] = CHSV(hue, 255, intensities[intensitiesIndex] );
                    }
                    else {
                        ledmatrix[rownum][colnum] = CRGB::Black;
                    }
                }
            }
        }
    }

    void setHue(uint8_t _hue) {
        hue = _hue;
    }
};

#endif