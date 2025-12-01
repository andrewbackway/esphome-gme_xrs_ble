# XRS AT Command Protocol – Reverse‑Engineered Specification

## 1. Overview

This document describes the observed protocol used between an Android client and an XRS radio over Bluetooth. The description is based on static analysis of the app’s behaviour and observed patterns, not on any official documentation.

- **Transport:** Bluetooth Classic RFCOMM (serial‑over‑Bluetooth, SPP‑style).
- **Encoding:** Text, effectively UTF‑8.
- **Framing:** Line‑oriented, each message is terminated by a carriage return and line feed (`\r\n`).
- **Style:** AT‑like textual command set:
  - Outgoing: `AT...`, `ATS...`, `AT+...`
  - Incoming: typically lines beginning with `+` or `AT`.

From the radio’s perspective, the client is essentially a terminal sending AT commands and parsing textual responses.

---

## 2. Transport & Session

### 2.1 Link Setup

- A Bluetooth RFCOMM socket is opened to the radio using one of the service UUIDs advertised by the device.
- Once the socket is connected:
  - A single input stream is used to read bytes from the radio.
  - A single output stream is used to send bytes to the radio.
- All higher‑level protocol operations are built on top of this bidirectional byte stream.

### 2.2 Auto‑Reconnect Behaviour

- When the connection is lost or a connect attempt fails, the client enters a reconnect mode.
- In reconnect mode:
  - The Bluetooth adapter periodically starts device discovery.
  - For each discovered device:
    - The device name must contain `"XRS"` (case‑insensitive).
    - The address must match the most recent stored radio MAC address.
    - The device must be bonded/paired.
  - When a matching device is found, a new connection attempt is made to that radio.
- Discovery attempts are repeated on a fixed interval until a connection is re‑established or reconnect mode is canceled.

---

## 3. Message Framing

### 3.1 Outgoing Messages

All outgoing messages follow this format:

```text
<COMMAND_LINE>\r\n
```

- `COMMAND_LINE` is an ASCII string such as:

  ```text
  AT+WGTLOC=HHMMSS,<latitude>,<longitude>
  ```

- The line terminator `\r\n` is required for the radio to recognize the end of a message.

### 3.2 Incoming Messages

Incoming bytes are processed as follows:

1. Bytes are read into a buffer and decoded as text using the platform’s default charset (effectively UTF‑8).
2. The client maintains a partial line accumulator string.
3. When a new chunk of text arrives:
   - If the chunk contains `"+"` or `"AT"` (case‑insensitive), it is treated as the start of a new message and replaces the accumulator.
   - Otherwise, the chunk is appended to the existing accumulator.
4. When the accumulator contains `"\r\n"`:
   - Everything up to and including the first `"\r\n"` is treated as one complete message.
   - That complete line is dispatched to the application‑level parser.
   - Any remaining text after `"\r\n"` stays in the accumulator as the start of the next message.

From a protocol perspective, each logical message is exactly one line of text ending in CRLF, and messages appear sequentially in the stream.

---

## 4. Command Reference

### Legend

- **Dir:**
  - `TX` = client → radio
  - `RX` = radio → client (inferred)
- **Syntax:** On‑wire text, without the terminating `\r\n`.
- **Notes:** Behaviour inferred from surrounding logic and usage patterns.

---

### 4.1 Basic Identification & Handshake

These commands are sent automatically shortly after a connection is established.

| Command   | Dir | Syntax   | Parameters | Notes |
|----------|-----|----------|-----------|-------|
| `ATE1`   | TX  | `ATE1`   | none      | Enables command echo on the radio. Sent once after connection. |
| `AT+GMI` | TX  | `AT+GMI` | none      | Requests manufacturer information. |
| `AT+GSN` | TX  | `AT+GSN` | none      | Requests device serial number. |
| `AT+GMM` | TX  | `AT+GMM` | none      | Requests model identifier. |
| `AT+GMR` | TX  | `AT+GMR` | none      | Requests firmware revision. |

The responses to these commands are textual lines, which are parsed elsewhere to populate device information within the client.

---

### 4.2 Channel / Squelch Information

| Command      | Dir | Syntax        | Parameters | Notes |
|-------------|-----|---------------|-----------|-------|
| `AT+WGCHSQ` | TX  | `AT+WGCHSQ`   | none      | Requests channel/squelch information table from the radio. |

- Responses to this command are parsed into an internal table that includes:
  - Channel identifiers.
  - Frequencies (as floating‑point values).
  - Labels and other metadata.
- This table is later used by the client to display or manage channel information, but is only processed while a connection is active.

---

### 4.3 Location Upload

| Command     | Dir | Syntax                                       | Parameters                                        | Notes |
|------------|-----|----------------------------------------------|--------------------------------------------------|-------|
| `AT+WGTLOC`| TX  | `AT+WGTLOC=HHMMSS,<latitude>,<longitude>`    | `HHMMSS` (UTC time), latitude, longitude         | Sends current GPS position to the radio. |

**Details:**

- `HHMMSS` is the current UTC time formatted with zero‑padded hours, minutes, and seconds.
- Latitude and longitude are decimal degrees in textual form (e.g. `-37.8136`, `144.9631`).
- The client sends this command no more often than approximately once every 30 seconds, and only when:
  - A radio connection is active.
  - Location sharing is enabled in the client settings.

**Example:**

```text
AT+WGTLOC=104522,-33.8650,151.2094\r\n
```

---

### 4.4 Status / User Message

| Command     | Dir | Syntax                            | Parameters                                      | Notes |
|------------|-----|-----------------------------------|------------------------------------------------|-------|
| `AT+WGTMSG`| TX  | `AT+WGTMSG="@username#status"`    | `username` + `status`, combined and quoted     | Sends a user status message to the radio. |

**Details:**

- The client builds a payload in the form:

  ```text
  @<username>#<status>
  ```

- The payload is then wrapped in double quotes and appended after `AT+WGTMSG=`.
- The username and status come from user‑configurable settings.
- This command is sent:
  - When a connection becomes active.
  - Whenever the user’s status text changes within the app.

**Example:**

```text
AT+WGTMSG="@Andrew#On the road"\r\n
```

---

### 4.5 Active Mute / Phone Integration

The radio can be instructed to adjust its audio behaviour based on the phone’s call state.

| Command        | Dir | Syntax           | Parameters | Notes |
|----------------|-----|------------------|-----------|-------|
| `AT+WGACTM=0`  | TX  | `AT+WGACTM=0`    | none      | Indicates that a call is in progress; engages radio mute mode. |
| `AT+WGACTM=1`  | TX  | `AT+WGACTM=1`    | none      | Indicates that no call is active; releases radio mute mode. |
| `AT+WGACTM=2`  | TX  | `AT+WGACTM=2`    | none      | Periodic “keep‑alive” or confirmation while a call is active. |

**Behaviour from the client’s perspective:**

- If “active mute” is enabled in settings:
  - When an outgoing or active call is detected:
    - Send `AT+WGACTM=0` once.
    - Then, every few seconds, send `AT+WGACTM=2` while the call remains active.
  - When the call ends:
    - Send `AT+WGACTM=1`.
    - Stop sending periodic `AT+WGACTM=2`.

The exact internal meaning of parameter values is implemented in radio firmware, but this pattern clearly toggles and maintains a “mute while on call” mode.

---

### 4.6 Configuration Register – `ATS109`

| Command  | Dir | Syntax        | Parameters     | Notes |
|---------|-----|---------------|----------------|-------|
| `ATS109`| TX  | `ATS109=<int>`| `<int>` bitmask | Sets device configuration via a numeric bitmask. |

**Observed behaviour:**

- The client treats this register as a bitfield.
- One particular bit controls whether the radio is allowed to share location data.
- When the user toggles a “share location” setting:
  - The client:
    - Reads the current integer value for this register (obtained previously from the radio).
    - Converts it to a binary string.
    - Flips the bit corresponding to the share‑location setting.
    - Converts the binary back to decimal.
    - Sends `ATS109=<newDecimalValue>`.

**Example:**

```text
ATS109=12345\r\n
```

The specific mapping between bits and features is not visible from this analysis, but the presence of a dedicated "share location" bit is clear.

---

## 5. Incoming Messages – High‑Level Types

Although exact text formats of incoming messages are not fully exposed in these files, the client’s behaviour reveals several categories.

### 5.1 Status / Alert Messages

- The client receives messages from the radio that are interpreted as human‑readable status or alert strings.
- When a “show radio messages” setting is enabled and such a message is received:
  - The client displays a brief on‑screen notification (toast) composed from parts of the received data.
- These messages are typically lines beginning with `+` or `AT` and ending with `\r\n`.

### 5.2 Channel / ACMA Data

- Responses to channel‑related commands (notably `AT+WGCHSQ`) are parsed into a table of:

  - Channel IDs.
  - Frequencies.
  - Labels and associated metadata.

- This table is used by the client to display channel and frequency information, and is only updated while the radio is connected.

### 5.3 Progress / Level Updates

- The radio periodically reports an integer value in the range 0–100.
- The client interprets this as a percentage and:
  - Updates a progress display.
  - Categorises it into coarse levels (0, 25, 50, 75, 100) for additional visual or behavioural cues.
- The exact meaning of this percentage is not explicit, but possibilities include signal level, update progress, or similar.

---

## 6. Connection State Model

### 6.1 Client‑Side State

The client maintains a notion of connection state, which includes:

- The currently selected radio device (Bluetooth address).
- Whether a Bluetooth session to that device is active.
- Timestamps for the last successful connection.
- Flags for “reconnect in progress”.

This state drives:

- Whether Bluetooth discovery is running.
- Whether location updates are being requested from the operating system.
- Whether certain AT commands (such as location upload and status messages) are sent.

### 6.2 Radio‑Visible Behaviour

From the radio’s point of view, connection state changes correspond to:

- **On connect:**
  - An initial burst of handshake commands (identifier queries and channel info).
  - A status message (`AT+WGTMSG`).
  - Periodic `AT+WGTLOC` commands if location sharing is enabled.
- **During normal operation:**
  - Occasional configuration updates (`ATS109=...`).
  - Location messages and user status updates as user settings change.
  - Active mute commands if phone call integration is enabled.
- **On disconnect:**
  - The client stops sending any AT commands.
  - Bluetooth discovery may resume later to attempt reconnection to the same device.

---

## 7. Timing & Throttling Summary

- Location → `AT+WGTLOC`:
  - Not more frequent than roughly every 30 seconds.
- Breadcrumb / map updates (internal to the client):
  - Only when the user has moved more than ~100 meters and at least ~30 seconds have elapsed since the last breadcrumb.
- Handshake sequence (`ATE1`, `AT+GMI`, `AT+GSN`, `AT+GMM`, `AT+GMR`, `AT+WGCHSQ`):
  - Commands are sent in quick succession during the first few seconds after a connection is established.
- Reconnect discovery:
  - Bluetooth discovery is started on a repeating interval while in reconnect mode.
- Active mute keep‑alive (`AT+WGACTM=2`):
  - Sent every few seconds while a phone call is in progress.

---

## 8. Example Session Outline

A typical session between client and radio follows this pattern:

1. **Connect over Bluetooth RFCOMM.**
2. **Handshake:**
   - `ATE1`
   - `AT+GMI`
   - `AT+GSN`
   - `AT+GMM`
   - `AT+GMR`
   - `AT+WGCHSQ`
3. **Initial status:**
   - `AT+WGTMSG="@username#status"`
4. **Periodic operation (while connected):**
   - Every ~30s (if allowed): `AT+WGTLOC=HHMMSS,lat,lon`
   - Occasionally: `ATS109=<int>` when configuration changes.
   - Occasionally: `AT+WGTMSG="@username#status"` if user status changes.
5. **Phone call integration (if enabled):**
   - Call starts: `AT+WGACTM=0`, then `AT+WGACTM=2` every few seconds.
   - Call ends: `AT+WGACTM=1`.
6. **On connection loss:**
   - The client stops sending commands.
   - Reconnect logic periodically scans for the last‑known XRS radio and attempts to reconnect.

This specification should be sufficient to build a compatible client or a test harness that can talk to the XRS radio over Bluetooth and exercise its key features (handshake, configuration, location updates, status messages, and phone‑call‑driven mute).