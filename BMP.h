//
// Created by jonat on 08-09-2024.
//

#ifndef BMP_H
#define BMP_H
#include <cstdint>

struct BMPHeader {
    uint16_t file_type{0x4D42};  // "BM"
    uint32_t file_size{0};
    uint16_t reserved1{0};
    uint16_t reserved2{0};
    uint32_t offset_data{0};
};

struct BMPInfoHeader {
    uint32_t size{0};             // Header size
    int32_t width{0};             // Image width
    int32_t height{0};            // Image height
    uint16_t planes{1};           // Number of color planes
    uint16_t bit_count{24};       // Bits per pixel (24 for RGB)
    uint32_t compression{0};      // Compression type
    uint32_t size_image{0};       // Image size (can be 0 for uncompressed images)
    int32_t x_pixels_per_meter{0};
    int32_t y_pixels_per_meter{0};
    uint32_t colors_used{0};      // Number of colors used (0 for default)
    uint32_t colors_important{0}; // Important colors (0 for default)
};

#endif //BMP_H
