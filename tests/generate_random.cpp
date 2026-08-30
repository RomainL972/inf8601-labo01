#include <iostream>
#include <random>

#include "image.h"

namespace {
constexpr size_t kWidth  = 256;
constexpr size_t kHeight = 256;
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <output.png>\n";
        return 1;
    }

    image_t* image = image_create(0, kWidth, kHeight);
    if (image == nullptr) {
        std::cerr << "image_create failed\n";
        return 1;
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (size_t y = 0; y < kHeight; ++y) {
        for (size_t x = 0; x < kWidth; ++x) {
            pixel_t* pixel = image_get_pixel(image, x, y);
            pixel->bytes[0] = static_cast<unsigned char>(byte_dist(rng));
            pixel->bytes[1] = static_cast<unsigned char>(byte_dist(rng));
            pixel->bytes[2] = static_cast<unsigned char>(byte_dist(rng));
            pixel->bytes[3] = 255;
        }
    }

    int result = image_save_png(image, argv[1]);
    image_destroy(image);

    if (result < 0) {
        std::cerr << "image_save_png failed\n";
        return 1;
    }

    return 0;
}
