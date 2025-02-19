#include <drivers/serial/serial.hpp>
#include <fs/devfs.hpp>
#include <proc/mutex.hpp>

namespace fs {
    struct UARTNode : DevINode {
        drivers::SerialPort port;
        proc::Mutex mutex;
        UARTNode(drivers::SerialPort port);
        virtual IFile *open(bool writable);
        virtual ~UARTNode();
    };

    struct UARTFile : IFile {
        UARTNode *node;
        UARTFile(UARTNode *node);
        virtual int64_t read(int64_t size, uint8_t *buf);
        virtual int64_t write(int64_t size, const uint8_t *buf);
    };
} // namespace fs