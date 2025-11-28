#pragma once
#include <Arduino.h>
#include "tfb_sock.h"
#include "tfb_physical.h"

class ArduinoTinyDevice {
public:
	ArduinoTinyDevice(char *name);
	void begin();
	void setDebugger(void (*debugger)(const char *fmt, va_list args));
	void setStream(Stream *stream);
	void setOutputEnablePin(int pin);
	void loop();
	int available();
	bool isConnected();
	int recv(uint8_t *buffer, size_t length);
	String recvString();
	void onEvent(void (*eventHandler)(int event));
	int send(uint8_t *buffer, size_t length);
	void flush();
	bool isFlushed();

private:
	int debug_count;
	char *name;
    void (*debugger)(const char *fmt, va_list args);
    void debug(const char *fmt, ...);
    tfb_physical_t *physical;
    int outputEnablePin;
    Stream *stream;
    tfb_sock_t *sock;
    void (*eventHandler)(int event);

	friend size_t ArduinoTinyDevice_write(tfb_physical_t *physical, uint8_t data);
	friend int ArduinoTinyDevice_read(tfb_physical_t *physical);
	friend size_t ArduinoTinyDevice_available(tfb_physical_t *physical);
	friend void ArduinoTinyDevice_write_enable(tfb_physical_t *physical, bool enable);
	friend uint32_t ArduinoTinyDevice_millis(tfb_physical_t *physical);
	friend void ArduinoTinyDevice_handle_event(tfb_sock_t *sock, int event);
};
