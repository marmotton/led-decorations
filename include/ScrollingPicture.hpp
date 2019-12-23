#ifndef SCROLLINGPICTURE_HPP
#define SCROLLINGPICTURE_HPP

#include <FastLED.h>
#include <vector>
#include <algorithm>

#include <FS.h>

class ScrollingPicture {
private:
    std::vector< std::vector<CRGB> > &ledmatrix;

    std::vector< std::vector<CRGB> > picture;

    std::vector< std::string > bmp_filenames;
    std::vector< std::string >::iterator current_bmp_filename_it;

    int max_scroll_position;
    int min_scroll_position;
    int scroll_position;

    uint32_t readInt(fs::File &file, uint start_position, uint length) {
        if (length > 4) {
            length = 4;
        }

        char readbuf[5];
        file.seek(start_position, SeekSet);
        file.readBytes(readbuf, length);

        // Convert char* to int, little-endian
        uint32_t readInt = 0;
        for (uint i = 0; i < length; i++) {
            readInt |= readbuf[i] << (8 * i);
        }

        return readInt;
    }

public:
    ScrollingPicture(std::vector< std::vector<CRGB> > &_ledmatrix) : ledmatrix(_ledmatrix) {
        // Initial scroll position is 1 step outside the matrix on the right
        max_scroll_position = _ledmatrix[0].size();
        scroll_position = max_scroll_position;

        // This value will be updated when loading a picture
        min_scroll_position = 0;

        // List available bmp files
        if ( SPIFFS.begin() ) {
            fs::Dir root = SPIFFS.openDir("/");

            while ( root.next() ) {
                std::string filename = std::string( root.fileName().c_str() );

                if ( filename.find(".bmp") != std::string::npos ) {
                    bmp_filenames.push_back( filename );
                }
            }
            current_bmp_filename_it = bmp_filenames.begin();
        }
        else {
            Serial.print("Could not open SPIFFS.");
        }
        
    }

    void loadImage(std::string picture_filename) {
        bool image_loaded = false;

        if ( SPIFFS.begin() ) {
            // Make sure the filename starts with a /
            size_t slash_pos = picture_filename.find("/");
            if ( slash_pos == std::string::npos || slash_pos != 0 ) {
                picture_filename = "/" + picture_filename;
            }

            if ( SPIFFS.exists(picture_filename.c_str()) ) {
                fs::File bmp_image = SPIFFS.open(picture_filename.c_str(), "r");

                // These locations correspond to the bmp image saved by the GIMP, different headers exist
                // GIMP save configuration: "Do not write colorspace information", "24 bit R8 G8 B8"
                // Location of the BMP data is located in bytes 10:13
                uint32_t bmp_data_location = readInt(bmp_image, 10, 4);
                
                // Width of the BMP image is located in bytes 18:21
                uint32_t bmp_width = readInt(bmp_image, 18, 4);

                // Height of the BMP image is located in bytes 22:25
                uint32_t bmp_height = readInt(bmp_image, 22, 4);

                picture.clear();
                for (uint row = 0; row < bmp_height; row++) {
                    std::vector<CRGB> line;

                    // The row size must be a multiple of 4 bytes (padding bytes are added at the end)
                    uint row_offset = row * ( (bmp_width * 24 + 31) / 32 ) * 4;

                    for (uint col = 0; col < bmp_width; col++) {
                        uint pixel_offset = bmp_data_location + row_offset + col * 3; // 24 bit color = 3 Bytes
                        line.push_back( readInt(bmp_image, pixel_offset, 3) );
                    }

                    picture.push_back(line);
                }
                scroll_position = max_scroll_position;
                min_scroll_position = -bmp_width;
                image_loaded = true;
            }
        }

        // If the bmp could not be loaded, initialize the picture with some solid color
        if ( !image_loaded ) {
            picture.resize(ledmatrix.size(), std::vector<CRGB>(ledmatrix[0].size(), 0x404040) );
        }
    }

    void loadNextBMP() {
        if ( current_bmp_filename_it != bmp_filenames.end()-1 ) {
            current_bmp_filename_it++;
        }
        else {
            current_bmp_filename_it = bmp_filenames.begin();
        }
        
        loadImage(*current_bmp_filename_it);
    }

    std::string getCurrentBmpFilename() {
        return *current_bmp_filename_it;
    }

    void nextFrame() {
        
        scroll_position = scroll_position > min_scroll_position ? scroll_position-1 : max_scroll_position;

        // Start with all LEDs black
        for ( auto &row : ledmatrix ) {
            for ( auto &led : row ) {
                led = CRGB::Black;
            }
        }

        // Set the LEDs colors by copying the picture into the correct position
        for ( uint rownum = 0; rownum < ledmatrix.size(); rownum++ ){
            std::vector<CRGB> picture_line = picture[rownum];

            auto picture_begin_it = picture_line.begin();
            auto picture_end_it = picture_line.end();
            auto led_begin_it = ledmatrix[rownum].begin();

            // Left part of the picture is cropped if the scroll position is outside (left) of the LED matrix
            if ( scroll_position < 0 ) {
                picture_begin_it = picture_line.begin() - scroll_position;
            }

            // Right part of the picture is cropped so that it does not go outside (right) of the LED matrix
            if ( (picture_end_it - picture_line.begin() + scroll_position) > ledmatrix[rownum].size() ) {
                picture_end_it = picture_line.begin() + ledmatrix[rownum].size() - scroll_position;
            }

            // Set the start column on the LED matrix for when the scroll has not reached the leftmost column
            if ( scroll_position > 0 ) {
                led_begin_it = led_begin_it + scroll_position;
            }

            // Copy the picture to the LED matrix at the correct position
            std::copy(picture_begin_it, picture_end_it, led_begin_it);
        }
    }
};

#endif