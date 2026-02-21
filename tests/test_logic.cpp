#include "../src/DspFilters.h"
#include "../src/RippleDetector.h"
#include "../src/BemfEstimator.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>

void test_filters() {
    EmaFilter ema(0.5f);
    assert(ema.update(10.0f) == 5.0f);
    assert(ema.update(10.0f) == 7.5f);
    std::cout << "EmaFilter passed." << std::endl;

    DcBlocker dc(0.9f);
    // Pulse
    float y1 = dc.process(10.0f);
    // y[0] = 0.9*0 + 0.9*(10-0) = 9.0
    assert(abs(y1 - 9.0f) < 0.001f);
    std::cout << "DcBlocker passed." << std::endl;
}

void test_ripple() {
    RippleDetector detector;
    // Generate sine wave 100Hz
    // Period = 10000 us (10ms)
    float sampleRate = 20000.0f; // 20kHz
    float dt = 1.0f / sampleRate;

    std::vector<float> buffer;
    for(int i=0; i<4000; i++) { // 200ms
        buffer.push_back(sin(2 * M_PI * 100 * (i * dt)) + 0.5f); // + DC bias
    }

    // Process in one go
    detector.processBuffer(buffer.data(), buffer.size(), sampleRate);

    float freq = detector.getFrequency();
    std::cout << "Detected Freq: " << freq << " Hz" << std::endl;
    // 100Hz
    assert(freq > 95.0f && freq < 105.0f);
    std::cout << "RippleDetector passed." << std::endl;
}

void test_bemf() {
    BemfEstimator estimator;
    estimator.setMotorParams(2.0f, 5); // 2 Ohm, 5 poles

    // Test Low Speed
    // V_app = 5V, I = 1A -> V_bemf = 5 - 1*2 = 3V
    estimator.updateLowSpeedData(5.0f, 1.0f);
    estimator.calculateEstimate();
    assert(abs(estimator.getBemfVoltage() - 3.0f) < 0.1f);

    // Test High Speed Learning
    // Inject 110Hz ripple -> 660 RPM (Above 600 threshold)
    estimator.updateRippleFreq(110.0f);
    estimator.calculateEstimate();

    float rpm = estimator.getEstimatedRpm();
    std::cout << "Estimated RPM: " << rpm << std::endl;
    assert(abs(rpm - 660.0f) < 1.0f);

    std::cout << "BemfEstimator passed." << std::endl;
}

int main() {
    test_filters();
    test_ripple();
    test_bemf();
    return 0;
}
