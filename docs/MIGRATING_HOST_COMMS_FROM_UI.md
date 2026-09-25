# Migrating Host Communications from LogicElementsUI

This guide explains how to decouple and migrate microcontroller communications from **LogicElementsUI** (`C:\Users\tanne\OneDrive\Qt\LogicElementsUI`) to the canonical **`src/cli`** host subsystem and **`le_host_comms`** library in **LogicElements**.

---

## 1. Motivation & Background

Historically, `LogicElementsUI` contained the only host-side implementation of the framed UART binary packet protocol (`BoardComms.cpp`), implemented using Qt (`QSerialPort`, `QTimer`, `QObject`).

With the introduction of `src/cli/comms` in the core `LogicElements` repository, all protocol operations are now available as a pure C/C++ static library (`le_host_comms.lib`):
* Zero external dependencies (no Qt required).
* Native Win32 and POSIX serial transports.
* Built-in packet builder, CCITT CRC16 verification, chunk upload state machine, live telemetry polling, and execution control.

---

## 2. Architecture Comparison

### Before
```
LogicElementsUI
├── BoardComms.cpp / .h  ─── (Qt-dependent packet framing, CRC16, chunk uploader, timers)
└── LogicElementsUI.vcxproj links le_compiler_static.lib only
```

### After
```
LogicElements
├── le_host_comms.lib    ─── (Portable C/C++: packet framing, CRC16, upload state machine)
└── le.exe               ─── (CLI tool: upload, control, monitor, board scan, compile, sim)

LogicElementsUI
└── BoardComms.cpp / .h  ─── (Lightweight Qt GUI adapter delegating protocol logic to le_host_comms)
```

---

## 3. Migration Roadmap

### Phase 1: Update Project Configuration (`LogicElementsUI.vcxproj`)

`LogicElementsUI.vcxproj` already links against `le_compiler_static.lib` from `LogicElements/build/Release`. To link `le_host_comms`:

1. **Add Include Directory**:
   Add `C:\Users\tanne\OneDrive\Documents\GitHub\LogicElements\src\cli\comms\include` to `AdditionalIncludeDirectories`.
2. **Add Library Dependency**:
   Add `le_host_comms.lib` to `AdditionalDependencies` in both `Debug|x64` and `Release|x64`:
   ```xml
   <AdditionalDependencies>le_compiler_static.lib;le_host_comms.lib;%(AdditionalDependencies)</AdditionalDependencies>
   ```

---

### Phase 2: Refactor `BoardComms` to Use Canonical Definitions

#### 1. Replace Duplicated Protocol Enums
In [BoardComms.h](file:///C:/Users/tanne/OneDrive/Qt/LogicElementsUI/src/comms/BoardComms.h), include the canonical headers:
```cpp
#include "le_comms.h"
#include "le_host_comms.h"
```
Remove any local duplicate definitions of `COMMS_SYNC_BYTE`, `CMD_PING`, `CMD_PROG_BEGIN`, etc.

#### 2. Delegate CRC16 Calculation
Replace local `BoardComms::crc16Update` and `BoardComms::computeCrc16` with `le_host_crc16`:
```cpp
quint16 BoardComms::computeCrc16(const quint8* data, size_t length)
{
    return le_host_crc16(0xFFFF, data, length);
}
```

#### 3. Delegate Binary Packet Framing
Replace manual byte indexing in `BoardComms::buildPacket`:
```cpp
QByteArray BoardComms::buildPacket(quint8 cmd, quint8 seq, const QByteArray& payload)
{
    quint16 len = static_cast<quint16>(payload.size());
    QByteArray packet(5 + len + 2, 0);

    packet[0] = static_cast<char>(LE_COMMS_SYNC_BYTE);
    packet[1] = static_cast<char>(cmd);
    packet[2] = static_cast<char>(seq);
    packet[3] = static_cast<char>(len & 0xFF);
    packet[4] = static_cast<char>((len >> 8) & 0xFF);

    if (len > 0) {
        std::memcpy(packet.data() + 5, payload.constData(), len);
    }

    uint16_t crc = 0xFFFF;
    crc = le_host_crc16(crc, reinterpret_cast<const uint8_t*>(packet.constData() + 1), 4 + len);
    packet[5 + len]     = static_cast<char>(crc & 0xFF);
    packet[5 + len + 1] = static_cast<char>((crc >> 8) & 0xFF);

    return packet;
}
```

---

### Phase 3: Asynchronous Program Upload Migration Options

You have two clean options for program uploading:

#### Option A: Direct Headless CLI Invocation via `QProcess` (Simplest & Most Decoupled)
`LogicElementsUI` can invoke `le.exe upload` directly in the background using `QProcess`:

```cpp
void BoardComms::uploadProgramViaCli(const QString& lebinFilePath, const QString& portName, quint32 baudRate)
{
    QProcess* proc = new QProcess(this);
    QString cliPath = "C:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/build/Release/le.exe";

    QStringList args;
    args << "upload" << lebinFilePath << "--port" << portName << "--baud" << QString::number(baudRate)
         << "--slot" << QString::number(targetSlot) << "--run";

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        QString out = proc->readAllStandardOutput();
        // Parse progress percentage emitted by CLI (e.g. "[======> ] 45%")
        emit cliOutputReceived(out);
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            emit uploadFinished(true, "Program flashed and running!");
        } else {
            emit uploadFinished(false, "Upload failed.");
        }
        proc->deleteLater();
    });

    proc->start(cliPath, args);
}
```

#### Option B: In-Process Threaded Upload via `le_host_upload_program`
Run `le_host_upload_program` on a background worker thread (`QThread` or `QtConcurrent::run`), passing a lambda as the progress callback:

```cpp
QFuture<void> future = QtConcurrent::run([this, lebinData, portName, baudRate]() {
    le_host_comms_t* client = le_host_open(portName.toUtf8().constData(), baudRate);
    if (!client) {
        emit uploadFinished(false, "Could not open serial port.");
        return;
    }

    char err[256] = {0};
    int rc = le_host_upload_program(
        client,
        0,   // target config slot (must not be the currently active slot)
        reinterpret_cast<const uint8_t*>(lebinData.constData()),
        lebinData.size(),
        64,
        [](size_t uploaded, size_t total, void* udata) {
            auto* self = static_cast<BoardComms*>(udata);
            int pct = (int)((uploaded * 100) / total);
            emit self->uploadProgress(pct, QString("Uploaded %1/%2 bytes").arg(uploaded).arg(total));
        },
        this,
        err,
        sizeof(err)
    );

    le_host_close(client);
    emit uploadFinished(rc == 0, rc == 0 ? "Upload succeeded!" : QString("Upload failed: %1").arg(err));
});
```

---

## 4. Verification Checklist

1. [x] Compile `le_host_comms.lib` in `LogicElements` via CMake.
2. [x] Verify `le.exe` subcommands (`compile`, `disasm`, `board --scan`, `upload`, `monitor`, `control`, `sim`).
3. [ ] Add `le_host_comms.lib` to `LogicElementsUI.vcxproj`.
4. [ ] Include `<le_comms.h>` and `<le_host_comms.h>` in `BoardComms.h`.
5. [ ] Test live program upload and telemetry monitoring from both the `le` CLI and `LogicElementsUI`.
