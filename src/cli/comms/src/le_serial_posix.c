/**
 * @file le_serial_posix.c
 * @brief POSIX termios serial communication implementation.
 */

#if !defined(_WIN32)

#include "le_serial.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct le_serial {
    int fd;
};

static speed_t get_posix_baud(uint32_t baud)
{
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return B115200;
    }
}

le_serial_t* le_serial_open(const char* port_name, uint32_t baud_rate)
{
    if (!port_name || !port_name[0]) return NULL;

    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        return NULL;
    }

    fcntl(fd, F_SETFL, 0);

    struct termios options;
    tcgetattr(fd, &options);

    speed_t speed = get_posix_baud(baud_rate);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~OPOST;

    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1; // 100ms default inter-byte timeout

    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    le_serial_t* ser = (le_serial_t*)malloc(sizeof(le_serial_t));
    if (!ser) {
        close(fd);
        return NULL;
    }
    ser->fd = fd;
    return ser;
}

void le_serial_close(le_serial_t* port)
{
    if (!port) return;
    if (port->fd >= 0) {
        tcflush(port->fd, TCIOFLUSH);
        close(port->fd);
        port->fd = -1;
    }
    free(port);
}

int le_serial_read(le_serial_t* port, uint8_t* buffer, size_t max_len, uint32_t timeout_ms)
{
    if (!port || port->fd < 0 || !buffer || max_len == 0) return -1;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(port->fd, &set);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int res = select(port->fd + 1, &set, NULL, NULL, &tv);
    if (res < 0) {
        return -1;
    } else if (res == 0) {
        return 0; // timeout
    }

    ssize_t n = read(port->fd, buffer, max_len);
    return (int)n;
}

int le_serial_write(le_serial_t* port, const uint8_t* data, size_t len)
{
    if (!port || port->fd < 0 || !data || len == 0) return -1;

    size_t total_written = 0;
    while (total_written < len) {
        ssize_t n = write(port->fd, data + total_written, len - total_written);
        if (n < 0) return -1;
        if (n == 0) break;
        total_written += (size_t)n;
    }
    return (int)total_written;
}

int le_serial_flush(le_serial_t* port)
{
    if (!port || port->fd < 0) return -1;
    return tcflush(port->fd, TCIOFLUSH) == 0 ? 0 : -1;
}

int le_serial_list_ports(char ports[][64], size_t max_ports)
{
    if (!ports || max_ports == 0) return 0;
    int count = 0;

    DIR* dir = opendir("/dev");
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < (int)max_ports) {
        if (strncmp(entry->d_name, "ttyUSB", 6) == 0 ||
            strncmp(entry->d_name, "ttyACM", 6) == 0 ||
            strncmp(entry->d_name, "cu.usb", 6) == 0) {
            snprintf(ports[count], 64, "/dev/%s", entry->d_name);
            count++;
        }
    }
    closedir(dir);
    return count;
}

#endif /* !_WIN32 */
