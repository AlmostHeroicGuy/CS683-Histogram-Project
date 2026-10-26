#include <iostream>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <omp.h>

using namespace std;

// Read a .raw file into a vector of bytes (one byte = one pixel)
vector<uint8_t> read_image(const char* path) {
    FILE* f = fopen(path, "rb");          
    if (!f) {
        cout << "cannot open " << path << endl;
        exit(1);
    }
    fseek(f, 0, SEEK_END);                // jump to the end of the file
    uint32_t n = ftell(f);                // our position now = file size = number of pixels
    fseek(f, 0, SEEK_SET);                // jump back to the start

    vector<uint8_t> img(n);
    fread(img.data(), 1, n, f);           // read n bytes into img
    fclose(f);
    return img;
}

// Method 0: plain single-threaded histogram = the CORRECT answer
void hist_naive(vector<uint8_t>& img, vector<uint32_t>& h) {
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0

    // TODO: loop over every pixel, add 1 to that pixel's bin
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}

// Method 1: shared histogram, all cores go boom baam, wrong result
void hist_shared_racy(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b]=0;

    #pragma omp parallel for num_threads(T) 
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}

// Method 1: shared histogram but fixed the race condition by atomising the read-write step
void hist_shared(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b] = 0;
    #pragma omp parallel for num_threads(T)        
    for (uint32_t i = 0; i < img.size(); i++) {
        #pragma omp atomic                         
        h[img[i]]++;
    }
}

// comparing histograms
void check(const char* name, vector<uint32_t>& ref, vector<uint32_t>& h, uint32_t n, int T) {
    uint32_t total = 0;
    int wrong = 0;
    for (int b = 0; b < 256; b++) {
        total += h[b];
        if (h[b] != ref[b]) wrong++;
    }
    cout << "Number of threads " << T << endl;
    cout << name << ": sum of bins = " << total << " (should be " << n << "), wrong bins = " << wrong << (wrong==0 ? ". Your histogram is OK" : ". Your histogram is WRONG") << endl;
}



int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "usage: ./hist <image.raw> <number of threads>" << endl;
        return 1;
    }

    vector<uint8_t> img = read_image(argv[1]);
    uint32_t n = img.size();
    int T = atoi(argv[2]);

    vector<uint32_t> ref(256), h(256);              // each pixel intensity is a bin
    hist_naive(img, ref);  //using the single thread for checking corretness of the furthur histograms

    hist_shared_racy(img, h, T);
    check("Shared (racy)", ref, h, n, T);

    hist_shared(img, h, T);
    check("Shared (corrected)", ref, h, n, T);

    return 0;

    // // Check 1: every pixel counted once -> bins add up to n
    // uint64_t total = 0;
    // for (int b = 0; b < 256; b++) total += h[b];
    // cout << "pixels = " << n << ", sum of bins = " << total
    //      << (total == n ? "  -> OK" : "  -> WRONG") << endl;

    // // Check 2: print the big bins (> 1% of all pixels)
    // for (int b = 0; b < 256; b++) {
    //     if (h[b] > n / 100)
    //         cout << "  bin " << b << ": " << h[b]
    //              << "  (" << 100.0 * h[b] / n << "%)" << endl;
    // }
    // return 0;
}