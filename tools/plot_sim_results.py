import sys
import matplotlib.pyplot as plt
import re


def parse_sim_results(file_path):
    times = []
    targets = []
    rpms = []
    currents = []
    duties = []

    with open(file_path, "r") as f:
        for line in f:
            # Look for state lines (Time, TgtSpeed, SimRPM, SimCurr, Duty, Stalled)
            # Example: 0.000      15        0.000      0.000      0.000      NO
            match = re.match(
                r"(\d+\.\d+)\s+(\d+)\s+(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(\w+)",
                line,
            )
            if match:
                times.append(float(match.group(1)))
                targets.append(float(match.group(2)))
                rpms.append(float(match.group(3)))
                currents.append(float(match.group(4)))
                duties.append(float(match.group(5)))

    return times, targets, rpms, currents, duties


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_sim_results.py <results.txt>")
        return

    times, targets, rpms, currents, duties = parse_sim_results(sys.argv[1])

    if not times:
        print("No data found to plot.")
        return

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12), sharex=True)

    # Plot RPM and Target
    ax1.plot(times, rpms, label="Simulated RPM", color="blue")
    ax1.set_ylabel("RPM")
    ax1.set_title("NIMRS PID Simulation Results")
    ax1.grid(True)
    ax1.legend()

    # Plot Current
    ax2.plot(times, currents, label="Armature Current (A)", color="red")
    ax2.set_ylabel("Current (A)")
    ax2.grid(True)
    ax2.legend()

    # Plot Duty Cycle
    ax3.plot(times, duties, label="PWM Duty Cycle", color="green")
    ax3.set_ylabel("Duty")
    ax3.set_xlabel("Time (s)")
    ax3.grid(True)
    ax3.legend()

    plt.tight_layout()
    plt.savefig("build/sim_plot.png")
    print("Plot saved to build/sim_plot.png")


if __name__ == "__main__":
    main()
