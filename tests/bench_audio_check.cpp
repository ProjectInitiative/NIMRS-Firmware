#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <strings.h> // for strcasecmp

// Mock String class to simulate Arduino String overhead
class String : public std::string {
public:
    String(const char* s) : std::string(s ? s : "") {}
    bool endsWith(const String& suffix) const {
        if (length() < suffix.length()) return false;
        return compare(length() - suffix.length(), suffix.length(), suffix) == 0;
    }
};

void run_benchmark() {
    const char* filename = "sound_effect.mp3";
    const int iterations = 1000000;

    // Benchmark unoptimized approach
    auto start1 = std::chrono::high_resolution_clock::now();
    volatile int matches1 = 0;
    for (int i = 0; i < iterations; ++i) {
        String fn = String(filename);
        if (fn.endsWith(".mp3") || fn.endsWith(".MP3")) {
            matches1++;
        }
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff1 = end1 - start1;

    // Benchmark optimized approach
    auto start2 = std::chrono::high_resolution_clock::now();
    volatile int matches2 = 0;
    for (int i = 0; i < iterations; ++i) {
        size_t len = strlen(filename);
        bool isMp3 = (len >= 4 && strcasecmp(filename + len - 4, ".mp3") == 0);
        if (isMp3) {
            matches2++;
        }
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff2 = end2 - start2;

    std::cout << "Unoptimized (String allocation): " << diff1.count() << " s\n";
    std::cout << "Optimized (strcasecmp): " << diff2.count() << " s\n";
    std::cout << "Speedup: " << diff1.count() / diff2.count() << "x\n";
}

int main() {
    run_benchmark();
    return 0;
}
