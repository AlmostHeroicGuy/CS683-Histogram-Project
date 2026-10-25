#include <iostream>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <random>
#include <vector>
// we making sample images with this one
using namespace std;

// Image size rn is 4096*4096 pixels which makes 1 byte per pixel or we can say 16 MB size (bigger than the 12 MB L3 cache)
const uint32_t W = 4096;
const uint32_t H = 4096;
const uint32_t N = W * H;

void write_image(const char* path, vector<uint8_t>& img) {
    FILE* f = fopen(path, "wb");             
    if (!f) { 
        cout << "cannot create the image " << path << ". Does data/ exist?" << endl;
        exit(1); 
    }
    fwrite(img.data(), 1, N, f);   
    fclose(f);
    cout << "Wrote " << path << endl;
}

int main() {
    vector<uint8_t> img(N);
    mt19937 rng(69);   // rng with the holy seed    

    // Image 1 is from UNIFORM distribution between 0 to 255
    for (uint32_t i = 0; i < N; i++) {
        img[i] = rng() & 0xFF;               // rng() gives a random 32-bit number so & with 0xFF keeps only the lowest 8 bits
    }
    write_image("data/uniform.raw", img);

    // Image 2 distribution is peaked with N(128, 3)
    normal_distribution<double> gauss(128.0, 3.0);
    for (uint32_t i = 0; i < N; i++) {
        double v = std::round(gauss(rng));   // random real number near 128, rounded
        if (v < 0)   v = 0;                  // clamping into the valid [0,255] range
        if (v > 255) v = 255;
        img[i] = (uint8_t)v;
    }
    write_image("data/peaked.raw", img);

    return 0;
}