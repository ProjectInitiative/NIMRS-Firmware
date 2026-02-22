# API Documentation

This documentation is automatically generated from the source code.

## Table of Contents

- [Config](#config)
  - [Set Hostname](#set-hostname)
  - [Set Web Credentials](#set-web-credentials)
- [Control](#control)
  - [Play Audio](#play-audio)
  - [Control Decoder](#control-decoder)
  - [Read/Write CV](#read-write-cv)
  - [Read All CVs](#read-all-cvs)
  - [Bulk Write CVs](#bulk-write-cvs)
  - [Get CV Definitions](#get-cv-definitions)
- [Files](#files)
  - [Delete File](#delete-file)
  - [Delete File (Method)](#delete-file-method)
  - [List Files](#list-files)
  - [Upload File](#upload-file)
- [Logs](#logs)
  - [Get Logs](#get-logs)
- [Motor](#motor)
  - [Start Calibration](#start-calibration)
  - [Get Calibration Status](#get-calibration-status)
  - [Start Motor Test](#start-motor-test)
  - [Get Motor Test Data](#get-motor-test-data)
- [Status](#status)
  - [System Status](#system-status)
- [System](#system)
  - [Root Index](#root-index)
  - [Index HTML](#index-html)
- [WiFi](#wifi)
  - [Reset WiFi](#reset-wifi)
  - [Save WiFi Config](#save-wifi-config)
  - [Scan WiFi](#scan-wifi)

## Config

### Set Hostname

`POST /api/config/hostname`

Updates the device hostname. Requires a restart.

#### Parameters

| Name | Type   | Description                  |
| ---- | ------ | ---------------------------- |
| name | String | New hostname (max 31 chars). |

#### Success Response

| Type   | Field | Description                         |
| ------ | ----- | ----------------------------------- |
| String | text  | "Hostname saved. Restart required." |

#### Error Response

| Type   | Field | Description                              |
| ------ | ----- | ---------------------------------------- |
| String | text  | "Missing name" or "Invalid name length". |

---

### Set Web Credentials

`POST /api/config/webauth`

Updates web interface credentials. Empty strings disable

#### Parameters

| Name | Type   | Description              |
| ---- | ------ | ------------------------ |
| user | String | Username (max 31 chars). |
| pass | String | Password (max 31 chars). |

#### Success Response

| Type   | Field | Description                                |
| ------ | ----- | ------------------------------------------ |
| String | text  | "Web credentials saved. Restart required." |

#### Error Response

| Type   | Field | Description                                 |
| ------ | ----- | ------------------------------------------- |
| String | text  | "Missing user or pass" or "Invalid length". |

---

## Control

### Play Audio

`POST /api/audio/play`

Plays a specific audio file.

#### Parameters

| Name | Type   | Description      |
| ---- | ------ | ---------------- |
| file | String | Filename (path). |

#### Success Response

| Type   | Field | Description |
| ------ | ----- | ----------- |
| String | text  | "Playing"   |

---

### Control Decoder

`POST /api/control`

Controls speed, direction, functions, etc.

#### Parameters

| Name    | Type   | Description                        |
| ------- | ------ | ---------------------------------- |
| plain   | JSON   | JSON payload.                      |
| action  | String | Action to perform ("stop",         |
| [value] | Mixed  | Value for the action.              |
| [index] | Number | Function index for "set_function". |

#### Success Response

| Type | Field  | Description      |
| ---- | ------ | ---------------- |
| JSON | status | {"status": "ok"} |

---

### Read/Write CV

`POST /api/cv`

Reads or writes a single CV.

#### Parameters

| Name    | Type   | Description                   |
| ------- | ------ | ----------------------------- |
| plain   | JSON   | JSON payload.                 |
| cmd     | String | "read" or "write".            |
| cv      | Number | CV number.                    |
| [value] | Number | Value to write (for "write"). |

---

### Read All CVs

`GET /api/cv/all`

Reads all known CVs.

#### Success Response

| Type | Field | Description                      |
| ---- | ----- | -------------------------------- |
| JSON | cvs   | Key-value map of CV ID to value. |

---

### Bulk Write CVs

`POST /api/cv/all`

Writes multiple CVs.

#### Parameters

| Name  | Type | Description                                      |
| ----- | ---- | ------------------------------------------------ |
| plain | JSON | JSON object where keys are CV IDs and values are |

#### Success Response

| Type | Field  | Description      |
| ---- | ------ | ---------------- |
| JSON | status | {"status": "ok"} |

---

### Get CV Definitions

`GET /api/cv/defs`

Retrieves definition of all supported CVs.

#### Success Response

| Type  | Field | Description                |
| ----- | ----- | -------------------------- |
| Array | cvs   | Array of {cv, name, desc}. |

---

## Files

### Delete File

`POST /api/files/delete`

Deletes a file.

#### Parameters

| Name | Type   | Description       |
| ---- | ------ | ----------------- |
| path | String | Path to the file. |

#### Success Response

| Type   | Field | Description |
| ------ | ----- | ----------- |
| String | text  | "Deleted"   |

#### Error Response

| Type   | Field | Description                                 |
| ------ | ----- | ------------------------------------------- |
| String | text  | "File not found" or "Missing path argument" |

---

### Delete File (Method)

`DELETE /api/files/delete`

Deletes a file.

#### Parameters

| Name | Type   | Description       |
| ---- | ------ | ----------------- |
| path | String | Path to the file. |

#### Success Response

| Type   | Field | Description |
| ------ | ----- | ----------- |
| String | text  | "Deleted"   |

#### Error Response

| Type   | Field | Description                                 |
| ------ | ----- | ------------------------------------------- |
| String | text  | "File not found" or "Missing path argument" |

---

### List Files

`GET /api/files/list`

Lists all files in LittleFS.

#### Success Response

| Type  | Field | Description                    |
| ----- | ----- | ------------------------------ |
| Array | files | Array of objects {name, size}. |

---

### Upload File

`POST /api/files/upload`

Uploads a file (multipart/form-data).

#### Parameters

| Name | Type | Description         |
| ---- | ---- | ------------------- |
| file | File | The file to upload. |

#### Success Response

| Type   | Field | Description |
| ------ | ----- | ----------- |
| String | text  | "Upload OK" |

---

## Logs

### Get Logs

`GET /api/logs`

Retrieves system logs.

#### Parameters

| Name   | Type   | Description                        |
| ------ | ------ | ---------------------------------- |
| [type] | String | Filter by type: "data" or "debug". |

#### Success Response

| Type  | Field | Description           |
| ----- | ----- | --------------------- |
| Array | logs  | Array of log strings. |

---

## Motor

### Start Calibration

`POST /api/motor/calibrate`

Starts motor resistance measurement.

#### Success Response

| Type | Field  | Description           |
| ---- | ------ | --------------------- |
| JSON | status | {"status": "started"} |

---

### Get Calibration Status

`GET /api/motor/calibrate`

Gets current calibration state.

#### Success Response

| Type   | Field      | Description                              |
| ------ | ---------- | ---------------------------------------- |
| String | state      | "IDLE", "MEASURING", "DONE", or "ERROR". |
| Number | resistance | Measured resistance in 10mOhm units.     |

---

### Start Motor Test

`POST /api/motor/test`

Starts the motor test profile.

#### Success Response

| Type | Field  | Description           |
| ---- | ------ | --------------------- |
| JSON | status | {"status": "started"} |

---

### Get Motor Test Data

`GET /api/motor/test`

Retrieves motor test telemetry data.

#### Success Response

| Type | Field | Description     |
| ---- | ----- | --------------- |
| JSON | data  | Telemetry JSON. |

---

## Status

### System Status

`GET /api/status`

Retrieves the current system status.

#### Success Response

| Type    | Field     | Description                    |
| ------- | --------- | ------------------------------ |
| Number  | address   | Current DCC address.           |
| Number  | speed     | Current speed (0-126).         |
| String  | direction | "forward" or "reverse".        |
| Boolean | wifi      | WiFi connection status.        |
| Number  | uptime    | System uptime in seconds.      |
| String  | version   | Firmware build version.        |
| String  | hash      | Git commit hash.               |
| String  | hostname  | Device hostname.               |
| Number  | fs_total  | Total filesystem size.         |
| Number  | fs_used   | Used filesystem size.          |
| Array   | functions | Array of 29 booleans (F0-F28). |

---

## System

### Root Index

`GET /`

Serves the main web interface.

---

### Index HTML

`GET /index.html`

Serves the main web interface (alias).

---

## WiFi

### Reset WiFi

`POST /api/wifi/reset`

Clears WiFi credentials and restarts.

#### Success Response

| Type   | Field | Description                          |
| ------ | ----- | ------------------------------------ |
| String | text  | "WiFi settings reset. Restarting..." |

---

### Save WiFi Config

`POST /api/wifi/save`

Saves WiFi credentials and restarts.

#### Parameters

| Name | Type   | Description |
| ---- | ------ | ----------- |
| ssid | String | SSID.       |
| pass | String | Password.   |

#### Success Response

| Type   | Field | Description                             |
| ------ | ----- | --------------------------------------- |
| String | text  | "WiFi credentials saved. Restarting..." |

---

### Scan WiFi

`GET /api/wifi/scan`

Scans for available networks.

#### Success Response

| Type  | Field    | Description                 |
| ----- | -------- | --------------------------- |
| Array | networks | Array of {ssid, rssi, enc}. |

---
