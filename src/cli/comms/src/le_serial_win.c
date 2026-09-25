/**
 * @file le_serial_win.c
 * @brief Win32 serial communication implementation.
 */

#if defined(_WIN32)

#include "le_serial.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct le_serial {
    HANDLE handle;
    uint32_t current_timeout_ms;
};

le_serial_t* le_serial_open(const char* port_name, uint32_t baud_rate)
{
    if (!port_name || !port_name[0]) {
        return NULL;
    }

    char formatted_path[128];
    if (strncmp(port_name, "\\\\.\\", 4) == 0) {
        snprintf(formatted_path, sizeof(formatted_path), "%s", port_name);
    } else {
        snprintf(formatted_path, sizeof(formatted_path), "\\\\.\\%s", port_name);
    }

    HANDLE h = CreateFileA(
        formatted_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return NULL;
    }

    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return NULL;
    }

    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 2000;

    SetCommTimeouts(h, &timeouts);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

    le_serial_t* ser = (le_serial_t*)malloc(sizeof(le_serial_t));
    if (!ser) {
        CloseHandle(h);
        return NULL;
    }
    ser->handle = h;
    ser->current_timeout_ms = 100;
    return ser;
}

void le_serial_close(le_serial_t* port)
{
    if (!port) return;
    if (port->handle != INVALID_HANDLE_VALUE) {
        PurgeComm(port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
        CloseHandle(port->handle);
        port->handle = INVALID_HANDLE_VALUE;
    }
    free(port);
}

int le_serial_read(le_serial_t* port, uint8_t* buffer, size_t max_len, uint32_t timeout_ms)
{
    if (!port || port->handle == INVALID_HANDLE_VALUE || !buffer || max_len == 0) {
        return -1;
    }

    if (port->current_timeout_ms != timeout_ms) {
        COMMTIMEOUTS timeouts;
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = timeout_ms;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 2000;
        SetCommTimeouts(port->handle, &timeouts);
        port->current_timeout_ms = timeout_ms;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(port->handle, buffer, (DWORD)max_len, &bytes_read, NULL)) {
        return -1;
    }
    return (int)bytes_read;
}

int le_serial_write(le_serial_t* port, const uint8_t* data, size_t len)
{
    if (!port || port->handle == INVALID_HANDLE_VALUE || !data || len == 0) {
        return -1;
    }

    DWORD total_written = 0;
    while (total_written < len) {
        DWORD written = 0;
        if (!WriteFile(port->handle, data + total_written, (DWORD)(len - total_written), &written, NULL)) {
            return -1;
        }
        if (written == 0) {
            break;
        }
        total_written += written;
    }
    return (int)total_written;
}

int le_serial_flush(le_serial_t* port)
{
    if (!port || port->handle == INVALID_HANDLE_VALUE) return -1;
    return PurgeComm(port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT) ? 0 : -1;
}

int le_serial_list_ports(char ports[][64], size_t max_ports)
{
    if (!ports || max_ports == 0) return 0;

    int count = 0;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char valueName[256];
        BYTE data[256];

        while (count < (int)max_ports) {
            DWORD valueLen = sizeof(valueName);
            DWORD dataLen = sizeof(data);
            DWORD type = 0;

            LONG res = RegEnumValueA(hKey, index, valueName, &valueLen, NULL, &type, data, &dataLen);
            if (res != ERROR_SUCCESS) {
                break;
            }

            if (type == REG_SZ) {
                snprintf(ports[count], 64, "%s", (const char*)data);
                count++;
            }
            index++;
        }
        RegCloseKey(hKey);
    }
    return count;
}

#endif /* _WIN32 */
