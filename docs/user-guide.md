# NIMRS User Guide

## Getting Started

1.  **Power Up:** Connect the decoder to a 21-Pin tester or locomotive.
2.  **WiFi Setup:**
    - On first boot, connect to the WiFi Access Point **`NIMRS-Decoder`**.
    - A "Sign In" or Config Portal should appear.
    - Enter your home WiFi credentials.
    - The decoder will reboot and connect to your network.

## Web Dashboard

Navigate to `http://<decoder-ip>/` (e.g., `http://192.168.1.50`).

### Authentication

By default, the web interface is protected with:

- **Username:** `admin`
- **Password:** `admin`

**Changing Credentials:**

1. Navigate to the **System** tab.
2. Expand the **Security** section.
3. Enter new credentials and click **Update Credentials**.

**Disabling Authentication:**
To disable authentication entirely, leave the **Username** field blank and click **Update Credentials**.

**Authentication API:**
You can also manage authentication programmatically via the API:

- **Endpoint:** `POST /api/config/webauth`
- **Parameters:** `user`, `pass`
- **Example:** To disable auth, send `user=` (empty) and `pass=` (empty).
  ```bash
  curl -u admin:admin -X POST http://<ip>/api/config/webauth -d "user=&pass="
  ```

### Dashboard Features

- **Status:** Shows basic health (Speed, Direction, WiFi).
- **Live Logs:** Click **Live Logs** to see real-time debug information (DCC packets, Motor logic).
- **Firmware Update:** Click **Firmware Update** to upload a new version.

## Motor Calibration

To ensure optimal low-speed performance, the decoder needs to know the Armature Resistance ($R_a$) of the motor. You can measure this automatically via the web interface.

1.  Place the locomotive on a powered track.
2.  Navigate to the **Debug** tab on the web dashboard.
3.  Locate the **Motor Calibration** card.
4.  **Important:** Firmly hold the locomotive wheels or flywheel to prevent rotation (STALL condition). This ensures the Back-EMF is zero.
5.  Click **Measure Resistance**.
6.  The motor will hum for about 1 second.
7.  The measured resistance will be displayed and automatically saved to **CV 149**.

### Manual Measurement Method

If automatic measurement fails or is not possible, you can measure the resistance manually:

1.  **Power Off:** Ensure the locomotive is powered off and remove the decoder from the socket.
2.  **Measure:** Using a multimeter, measure the resistance (in Ohms) between **Pin 18** (Motor 2) and **Pin 19** (Motor 1) on the 21-pin socket, or directly across the motor tabs.
3.  **Calculate:** Convert the resistance to CV units by multiplying by 100 (1 unit = 0.01 Ohms).
    - Example: 12.5 Ohms \* 100 = **1250**
4.  **Write:** Enter the calculated value into **CV 149**.

## Firmware Updates (OTA)

You can update the decoder wirelessly:

1.  Download the latest `nimrs-firmware.bin` from GitHub Releases.
2.  Go to `http://<decoder-ip>/update`.
3.  Select the `.bin` file.
4.  Click **Update**. The decoder will flash and reboot automatically.

## DCC Configuration

The decoder supports standard NMRA CVs:

- **CV 1:** Short Address (Default: 3)
- **CV 3:** Acceleration Rate (Momentum)
- **CV 4:** Deceleration Rate (Momentum)
- **CV 29:** Configuration Register (Direction, Steps, Address Mode)

## Troubleshooting

- **Blue LED Flashing:** Hardware Serial traffic (TX).
- **No Motor Movement:** Check if `VMOTOR_PG` (Power Good) is high in logs. Check CV2 (Start Voltage).
- **Won't Connect to WiFi:** Reset the board 3 times quickly (TODO: Implement Factory Reset) or re-flash to clear NVS.
