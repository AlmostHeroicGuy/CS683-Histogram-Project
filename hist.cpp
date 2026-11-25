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

// Method 0: plain single-threaded histogram for checking correctness
void hist_naive(vector<uint8_t>& img, vector<uint32_t>& h) {
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}


// Method 1: shared histogram, all cores go boom baam, wrong result
void hist_shared_racy(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0
    #pragma omp parallel for num_threads(T) 
    for (uint32_t i = 0; i < img.size(); i++){
        h[img[i]]++;
    }
}

// Method 1: shared histogram but fixed the race condition by atomising the read-write step
void hist_shared(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0
    #pragma omp parallel for num_threads(T)        
    for (uint32_t i = 0; i < img.size(); i++) {
        #pragma omp atomic                         
        h[img[i]]++;
    }
}

// Method 2: giving each thread its own private bins (catch is that each thread scans the whole image)
void hist_binpart(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b] = 0;   // all bins start at 0
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
    

// Method 3: privatized but PACKED

void hist_packed(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    vector<uint32_t> priv(256 * T, 0);           // T counters for each bin

    #pragma omp parallel num_threads(T)
    {
        int id = omp_get_thread_num();

        #pragma omp for                          // here we split the pixels in the threads
        for (uint32_t i = 0; i < img.size(); i++) {
            priv[img[i] * T + id]++;             // Counter for this thread only
        }
    }

    // adding up the T counters of each bin
    for (int b = 0; b < 256; b++) {
        h[b] = 0;
        for (int t = 0; t < T; t++) h[b] += priv[b * T + t];
    }
}



// Method 4: privatized, SEPARATE
void hist_private(vector<uint8_t>& img, vector<uint32_t>& h, int T) {
    for (int b = 0; b < 256; b++) h[b] = 0;
    #pragma omp parallel num_threads(T)
    {
        alignas(64) uint32_t local[256] = {0};   // my private counters, cache-line aligned

        #pragma omp for
        for (uint32_t i = 0; i < img.size(); i++) {
            local[img[i]]++;                     
        }

        for (int b = 0; b < 256; b++) {          
            #pragma omp atomic
            h[b] += local[b];
        }
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

// print_stuff for better formatting of the data for ease in plotting
void print_stuff(const char* method, const char* image, int T, vector<double>& t) {
    cout << "TIME for " << method << " using " << image << " on " << T << " threads" << " : (Median=" << median(t)*1000 << "ms, Mean=" << mean(t)*1000 << "ms)" << endl;
    cout << "CSV," << method << "," << image << "," << T << "," << median(t)*1000 << "," << mean(t)*1000 << endl;
}

void time_method(int method, const char* name, vector<uint8_t>& img, vector<uint32_t>& h,
                 int T, const char* image) {
    int runs = 10;
    vector<double> time(runs);
    for (int r = 0; r < runs; r++) {
        double start = omp_get_wtime();
        if (method == 0) hist_naive(img, h);
        if (method == 1) hist_shared_racy(img, h, T);
        if (method == 2) hist_shared(img, h, T);
        if (method == 3) hist_binpart(img, h, T);
        if (method == 4) hist_packed(img, h, T);
        if (method == 5) hist_private(img, h, T);
        time[r] = omp_get_wtime() - start;
    }
    print_stuff(name, image, (method == 0 ? 1 : T), time);
}


int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "usage: ./hist <image.raw> <number of threads>" << endl;
        return 1;
    }
    string method = "all";
    if (argc >= 4) method = argv[3];  // "naive", "racy", "shared", "binpart" or "all"

    vector<uint8_t> img = read_image(argv[1]);
    uint32_t n = img.size();
    int T = atoi(argv[2]);
    cout << "=========== Number of threads (Naive has always 1): " << T << " ===============" << endl;

    vector<uint32_t> ref(256), h(256);              // each pixel intensity is a bin
    
    hist_naive(img, ref);  //using the single thread for checking corretness of the furthur histograms


    //checking the correctness
    if (method == "all" || method == "racy")    { hist_shared_racy(img, h, T); check("Shared (racy)", ref, h, n, T); }
    if (method == "all" || method == "shared")  { hist_shared(img, h, T);      check("Shared (atomic)", ref, h, n, T); }
    if (method == "all" || method == "binpart") { hist_binpart(img, h, T);     check("Bin partitioned", ref, h, n, T); }
    if (method == "all" || method == "packed")  { hist_packed(img, h, T);      check("Packed private (interleaved)", ref, h, n, T); }
    if (method == "all" || method == "private") { hist_private(img, h, T);     check("Private (separate arrays)", ref, h, n, T); }

    // finding the time
    cout << "=========== Number of runs for finding time of each method: 10 ===============" << endl;
    if (method == "all" || method == "naive")   time_method(0, "naive",   img, h, T, argv[1]);
    if (method == "all" || method == "racy")    time_method(1, "racy",    img, h, T, argv[1]);
    if (method == "all" || method == "shared")  time_method(2, "shared",  img, h, T, argv[1]);
    if (method == "all" || method == "binpart") time_method(3, "binpart", img, h, T, argv[1]);
    if (method == "all" || method == "packed")  time_method(4, "packed",  img, h, T, argv[1]);
    if (method == "all" || method == "private") time_method(5, "private", img, h, T, argv[1]);

    return 0;
}