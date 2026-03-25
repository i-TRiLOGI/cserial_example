#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

typedef enum {
    PARITY_NONE,
    PARITY_EVEN,
    PARITY_ODD
} parity_t;

typedef struct {
    const char *port;       // e.g. "/dev/ttyUSB0"
    int         baud;       // 1200/2400/4800/9600/19200/38400/57600/115200
    int         data_bits;  // 7 or 8
    int         stop_bits;  // 1 or 2
    int         timeout_ms;  // read timeout, milliseconds
    parity_t    parity;     // NONE, EVEN, or ODD
} serial_config_t;

// Map integer baud rate to the POSIX Bxxx constant.
static speed_t baud_to_speed(int baudrate) {
    switch (baudrate) {
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B0;
    }
}

int serial_open(const serial_config_t *config) {
    // open device
    int fd = open(config->port, O_RDWR);
    if (fd < 0) {
        perror("serial_open: open");
        return -1;
    }

    // read current attributes
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("serial_open: tcgetattr");
        close(fd);
        return -1;
    }

    // set baud rate
    speed_t speed = baud_to_speed(config->baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // raw mode (disables canonical mode, echo, signals) 
    cfmakeraw(&tty);

    // control flags
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= (config->data_bits == 7) ? CS7 : CS8;

    // stop bits
    if (config->stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    // parity
    switch (config->parity) {
        case PARITY_NONE:
            tty.c_cflag &= ~PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case PARITY_EVEN:
            tty.c_cflag |=  PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case PARITY_ODD:
            tty.c_cflag |=  PARENB;
            tty.c_cflag |=  PARODD;
            break;
        default:
            break;
    }
    // Enable receiver, ignore modem status lines
    tty.c_cflag |= (CREAD | CLOCAL);

    // read timeout - convert to deciseconds (0.1s)
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = config->timeout_ms / 100;

    // apply
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("serial_open: tcsetattr");
        close(fd);
        return -1;
    }

    // flush stale data
    tcflush(fd, TCIOFLUSH);

    return fd;
}

ssize_t serial_write(int fd, const void *buf, size_t len) {
    if (fd < 0 || !buf) { errno = EINVAL; return -1; }

    size_t  written = 0;
    const char *p   = (const char *)buf;

    while (written < len) {
        ssize_t n = write(fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;   // retry on signal
            perror("serial_write: write");
            return -1;
        }
        written += (size_t)n;
    }

    // Wait until all bytes are physically transmitted
    tcdrain(fd);
    return (ssize_t) written;
}

ssize_t serial_read(int fd, char *buf, size_t max_len) {
    if (fd < 0 || !buf || max_len == 0) { errno = EINVAL; return -1; }

    size_t total = 0;

    while (total < max_len - 1) {
        ssize_t n = read(fd, buf + total, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("serial_read: read");
            return -1;
        }
        if (n == 0) break; // no more data

        total += (size_t)n;
        if (buf[total - 1] == '\r' || buf[total - 1] == '\n') break; // stop at newline
    }

    buf[total] = '\0';
    return (ssize_t)total;
}

void serial_close(int fd) {
    if (fd >= 0) close(fd);
}

int main(void) {
    serial_config_t config;

    // default
    config.port = "/dev/ttyUSB0";
    config.baud = 38400;
    config.data_bits = 8;
    config.stop_bits = 1;
    config.timeout_ms = 500;
    config.parity = PARITY_NONE;

    printf("Opening port %s\n", config.port);

    int fd = serial_open(&config);
    if (fd < 0) return EXIT_FAILURE;
    printf("Port opened (fd=%d).\n", fd);

    while (1) {
        printf("Enter a command (type \"exit\" to close program):\n");
        char command[80];
        scanf("%s", command);
        if (strcmp(command, "exit") == 0) {
            break;
        }
        const char *carriageRet = "\r";
        strcat(command, carriageRet);
        // send command
        ssize_t written = serial_write(fd, command, strlen(command));
        if (written < 0) {
            serial_close(fd);
            break;
        }
        printf("Sent %zd byte(s).\n", written);

        // read response
        char response[4096];
        ssize_t received = serial_read(fd, response, sizeof(response));
        if (received < 0) {
            serial_close(fd);
            return EXIT_FAILURE;
        }

        if (received == 0) {
            printf("No response received (timed out after %d ms).\n", config.timeout_ms);
        } else {
            printf("Received %zd byte(s): ", received);
            // Print raw bytes; show non-printable chars as hex
            for (ssize_t i = 0; i < received; i++) {
                unsigned char c = (unsigned char)response[i];
                if (c >= 0x20 && c < 0x7F)
                    putchar(c);
                else
                    printf("\\x%02X", c);
            }
            putchar('\n');
        }
    }

    serial_close(fd);
    printf("Port closed.\n");
    return EXIT_SUCCESS;
}
