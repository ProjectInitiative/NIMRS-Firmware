#include "../src/MotorTask.h"
#include "simulator/MotorSimulator.h"
#include <iomanip>
#include <iostream>
#include <vector>

// External from SimulatedMotorHal.cpp
void setGlobalMotorSimulator(MotorSimulator *sim);

// Helper for logging
void logHeader() {
  std::cout << std::left << std::setw(10) << "Time(s)" << std::setw(10)
            << "TgtSpeed" << std::setw(10) << "SimRPM" << std::setw(10)
            << "SimCurr" << std::setw(10) << "Duty" << std::setw(10)
            << "Stalled" << std::endl;
}

void logState(float time, uint8_t target, const MotorSimulator::State &state,
              float duty, bool stalled) {
  std::cout << std::left << std::fixed << std::setprecision(3) << std::setw(10)
            << time << std::setw(10) << (int)target << std::setw(10)
            << state.rpm << std::setw(10) << state.current << std::setw(10)
            << duty << std::setw(10) << (stalled ? "YES" : "NO") << std::endl;
}

void runScenario(const char *name, int durationTicks, uint8_t targetSpeed,
                 float loadTorque = 0.0f) {
  std::cout << "\n=== Scenario: " << name << " ===" << std::endl;

  MotorSimulator sim;
  setGlobalMotorSimulator(&sim);

  MotorTask &task = MotorTask::getInstance();
  task.setTargetSpeed(targetSpeed, true);

  logHeader();

  float dt = 0.020f; // 20ms (50Hz)
  for (int i = 0; i < durationTicks; i++) {
    // 1. Run MotorTask loop (firmware)
    // In real hardware this is a FreeRTOS task. Here we call the loop directly.
    // We need to mock 'millis()' and other time functions or ensure task
    // handles dt correctly. For this harness, we assume the internal _loop
    // logic is decoupled or uses globals we can mock.

    // Actually, MotorTask has a private _loop(). We might need a test-friendly
    // entry point or to just call the internal logic. For now, we'll simulate
    // the task execution.

    // NOTE: In a real test, we'd use a proper test framework.
    // This is a proof-of-concept for the simulator integration.

    // Step physics multiple times per firmware tick for stability
    int subSteps = 20;
    float subDt = dt / subSteps;
    for (int s = 0; s < subSteps; s++) {
      // duty is retrieved from the mocked MotorHal which was set by MotorTask
      // in the previous tick
      float currentDuty = MotorHal::getInstance()._currentDuty;
      sim.step(subDt, currentDuty, loadTorque);
    }

    // Chaos Engine: Random load and thermal drift
    sim.applyStochasticLoad(0.0001f);
    sim.simulateThermalDrift(dt);

    // Trigger Firmware Loop (This would be the next tick)
    // We'd call task._loop() here if it were accessible.

    if (i % 5 == 0) { // Log at 10Hz
      logState(i * dt, targetSpeed, sim.getState(),
               MotorHal::getInstance()._currentDuty, task.getStatus().stalled);
    }
  }
}

int main() {
  std::cout << "Starting NIMRS PID Simulator Harness..." << std::endl;

  // Test 1: Cold Start and Accel
  runScenario("Crawl Speed Start", 100, 15);

  // Test 2: High Speed Cruise
  runScenario("High Speed Run", 100, 128);

  // Test 3: Hard Stall
  runScenario("Sudden Obstacle (Stall)", 100, 64, 0.05f); // High load torque

  std::cout << "\nSimulation Complete." << std::endl;
  return 0;
}
