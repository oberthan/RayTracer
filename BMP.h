//
// Created by jonat on 08-09-2024.
//

#ifndef BMP_H
#define BMP_H
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "classes.h"

// BMP File Header (14 bytes)
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type{ 0x4D42 };  // File type always BM which is 0x4D42
    uint32_t file_size{ 0 };       // Size of the file (in bytes)
    uint16_t reserved1{ 0 };      // Reserved, must be 0
    uint16_t reserved2{ 0 };      // Reserved, must be 0
    uint32_t offset_data{ 0 };     // Start position of pixel data (bytes from the beginning of the file)
};
#pragma pack(pop)

// BMP Info Header (40 bytes)
struct BMPInfoHeader {
    uint32_t size{ 40 };         // Size of this header (40 bytes)
    int32_t width{ 0 };          // Width of the bitmap in pixels
    int32_t height{ 0 };         // Height of the bitmap in pixels
    uint16_t planes{ 1 };        // Number of color planes must be 1
    uint16_t bit_count{ 24 };     // Number of bits per pixel (24 for RGB)
    uint32_t compression{ 0 };   // Compression type (0 = uncompressed)
    uint32_t size_image{ 0 };     // Size of the raw bitmap data (including padding)
    int32_t x_pixels_per_meter{ 0 }; // Horizontal resolution
    int32_t y_pixels_per_meter{ 0 }; // Vertical resolution
    uint32_t colors_used{ 0 };    // Number of colors in the palette
    uint32_t colors_important{ 0 }; // Important colors (0 means all colors are important)
};

struct ColorUInt {
    uint8_t r, g, b;

    ColorUInt(float r, float g, float b)
        : r(static_cast<uint8_t>(r*255.0f)),
          g(static_cast<uint8_t>(g*255.0f)),
          b(static_cast<uint8_t>(b*255.0f)) {
    }
};

// BMP File Struct
struct BMP {
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    Color* pixelData;  // Pointer to an array of Color objects

    // Constructor to initialize BMP with a specific width and height
    BMP(int32_t width, int32_t height) {
        //infoHeader.size = sizeof(BMPInfoHeader);
        infoHeader.width = width;
        infoHeader.height = height;
        infoHeader.bit_count = 24;
        infoHeader.size_image = width * height * sizeof(ColorUInt);
        int32_t dataSize = width * height * sizeof(Color);
        pixelData = new Color[width * height];
        fileHeader.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
        fileHeader.file_size = fileHeader.offset_data + infoHeader.size_image;
    }

    BMP(std::string fileName){
        read(fileName);
    }

    // Destructor to clean up dynamically allocated memory
    ~BMP() {
        delete[] pixelData;
    }

    void SetPixel(int x, int y, const Color& color) const {
        pixelData[y*infoHeader.width+x] = color;
    }
    void SetPixels(Color* pixels) {
        pixelData = pixels;
    }
    // Method to write BMP data to a file
    void write(const std::string& fileName) {
        std::ofstream outFile(fileName, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error opening file!" << std::endl;
            return;
        }

        // Write headers
        outFile.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));

        outFile.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

        int rowPadding = (4 - (infoHeader.width * 3) % 4) % 4;

        //SetPixel(0,0,Color(1,0,0));
        // Write pixel data (convert from Color to BMP-compatible format)
        for (int i = 0; i < infoHeader.height; ++i) {
            for (int j = 0; j < infoHeader.width; ++j) {
                Color& color = pixelData[(infoHeader.height-1-i) * infoHeader.width + j];

                ColorUInt colorUInt(color.b, color.g, color.r);
                // BMP stores pixel data as BGR, not RGB
                outFile.write(reinterpret_cast<const char*>(&colorUInt), sizeof(colorUInt));

            }

            for (int p = 0; p < rowPadding; ++p) {
                outFile << static_cast<char>(0);
            }

        }

        outFile.close();
        std::cout << "BMP saved as " << fileName << std::endl;
    }
    BMP read(std::string fileName) {
        std::ifstream inFile(fileName, std::ios::binary);
        if (!inFile) {
            std::cerr << "Error opening file!" << std::endl;

        }
    }
};

#endif //BMP_H
