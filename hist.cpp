#include <iostream>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <omp.h>
#include <algorithm>

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
    
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}


// Method 1: shared histogram, all cores go boom baam, wrong result
void hist_shared_racy(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    #pragma omp parallel for num_threads(T) 
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}

// Method 1: shared histogram but fixed the race condition by atomising the read-write step
void hist_shared(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    #pragma omp parallel for num_threads(T)        
    for (uint32_t i = 0; i < img.size(); i++) {
        #pragma omp atomic                         
        h[img[i]]++;
    }
}

// Method 2: giving each thread its own private bins (catch is that each thread scans the whole image)
void hist_binpart(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    #pragma omp parallel num_threads(T)
    {
        int id = omp_get_thread_num();
        int low = id*256/T;   //first bin of this thread
        int high = (id+1)*256/T;     // last bin +1 of this thread

        uint32_t local[256] = {0};  //private counter of the thread, no contesting
        for (uint32_t i = 0; i < img.size(); i++){
            int v = img[i];
            if (v >= low && v < high) local[v]++;
        }
        for (int b = low; b < high; b++) h[b] = local[b];
    }
}
    


// Checking histograms correctness and comparing badness by the number of bad bins
void check(const char* name, vector<uint32_t>& ref, vector<uint32_t>& h, uint32_t n, int T) {
    uint32_t total = 0;
    int wrong = 0;
    for (int b = 0; b < 256; b++) {
        total += h[b];
        if (h[b] != ref[b]) wrong++;
    }
    cout << name << ": sum of bins = " << total << " (should be " << n << "), wrong bins = " << wrong << (wrong==0 ? ". Your histogram is OK" : ". Your histogram is WRONG") << endl;
}

// median time
double median(vector<double> t) {
    sort(t.begin(), t.end());
    return t[t.size() / 2];
}

// mean time
double mean(vector<double> t) {
    double sum = 0;
    for (int i=0; i<t.size(); i++){
        sum+=t[i];
    }
    return sum/t.size();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "usage: ./hist <image.raw> <number of threads>" << endl;
        return 1;
    }

    vector<uint8_t> img = read_image(argv[1]);
    uint32_t n = img.size();
    int T = atoi(argv[2]);
    cout << "=========== Number of threads (Naive has always 1): " << T << " ===============" << endl;

    vector<uint32_t> ref(256), h(256);              // each pixel intensity is a bin
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0
    for (int b = 0; b < 256; b++) ref[b] = 0;   // all bins start at 0
    hist_naive(img, ref);  //using the single thread for checking corretness of the furthur histograms

    hist_shared_racy(img, h, T);
    check("Shared (racy)", ref, h, n, T);

    for (int b = 0; b < 256; b++) h[b] = 0;
    hist_shared(img, h, T);
    check("Shared (corrected)", ref, h, n, T);

    for (int b = 0; b < 256; b++) h[b] = 0;
    hist_binpart(img, h, T);
    check("Bin partitioned (private counters for each thread)", ref, h, n, T);

    // finding the time
    int runs = 10;
    vector<double> time(runs);
    cout << "=========== Number of runs for finding time of each method: " << runs << " ===============" << endl;

    // Naive
    for (int r = 0; r < runs; r++){
        double start = omp_get_wtime();
        hist_naive(img, h);
        time[r] = omp_get_wtime() - start;
    }
    cout << "TIME for Naive using " << argv[1] << " : (Median=" << median(time)*1000 <<"ms, Mean=" << mean(time)*1000 << "ms)" << endl;


    // Shared racy
    for (int r = 0; r < runs; r++){
        double start = omp_get_wtime();
        hist_shared_racy(img, h, T);
        time[r] = omp_get_wtime() - start;
    }
    cout << "TIME for Shared (racy) using " << argv[1] << " : (Median=" << median(time)*1000 <<"ms, Mean=" << mean(time)*1000 << "ms)" << endl;


    // Shared corrected
    for (int r = 0; r < runs; r++){
        double start = omp_get_wtime();
        hist_shared(img, h, T);
        time[r] = omp_get_wtime() - start;
    }
    cout << "TIME for Shared (corrected) using " << argv[1] << " : (Median=" << median(time)*1000 <<"ms, Mean=" << mean(time)*1000 << "ms)" << endl;


    // Bin partitioned with private couters
    for (int r = 0; r < runs; r++){
        double start = omp_get_wtime();
        hist_binpart(img, h, T);
        time[r] = omp_get_wtime() - start;
    }
    cout << "TIME for Bin partitioned (private counters for each thread) using " << argv[1] << " : (Median=" << median(time)*1000 <<"ms, Mean=" << mean(time)*1000 << "ms)" << endl;
    

    return 0;
}