#include "ArduinoTinyDevice.h"
#include "tfb_physical.h"
#include "tfb_sock.h"

ArduinoTinyDevice::ArduinoTinyDevice(char *name) {
	this->name=name;
	this->outputEnablePin=-1;
	this->stream=NULL;
	this->eventHandler=NULL;
	this->debug_count=0;
}

void ArduinoTinyDevice::setStream(Stream *stream) {
	this->stream=stream;
}

void ArduinoTinyDevice::setOutputEnablePin(int pin) {
	this->outputEnablePin=pin;
}

void ArduinoTinyDevice::setDebugger(void (*debugger)(const char *fmt, va_list args)) {
	this->debugger=debugger;
}

void ArduinoTinyDevice::onEvent(void (*eventHandler)(int event)) {
	this->eventHandler=eventHandler;
}

void ArduinoTinyDevice::debug(const char *fmt, ...) {
    if (!debugger)
      return;

    va_list args;
    va_start(args, fmt);
    debugger(fmt,args);
    va_end(args);
}

size_t ArduinoTinyDevice_write(tfb_physical_t *physical, uint8_t data) {
	ArduinoTinyDevice *device=physical->tag;
	device->stream->write(data);
}

int ArduinoTinyDevice_read(tfb_physical_t *physical) {
	ArduinoTinyDevice *device=physical->tag;
	return device->stream->read();
}

size_t ArduinoTinyDevice_available(tfb_physical_t *physical) {
	ArduinoTinyDevice *device=physical->tag;
	return device->stream->available();
}

uint32_t ArduinoTinyDevice_millis(tfb_physical_t *physical) {
	ArduinoTinyDevice *device=physical->tag;
	return millis();
}

void ArduinoTinyDevice_handle_event(tfb_sock_t *sock, int event) {
	ArduinoTinyDevice *device=sock->tag;
	if (device->eventHandler)
		device->eventHandler(event);
}

void ArduinoTinyDevice_write_enable(tfb_physical_t *physical, bool enable) {
	ArduinoTinyDevice *device=physical->tag;
	if (device->outputEnablePin<0)
		return;

	if (enable) {
		digitalWrite(device->outputEnablePin,HIGH);
		delayMicroseconds(20);
	}

	else {
		delayMicroseconds(20);
		digitalWrite(device->outputEnablePin,LOW);
	}
}

void ArduinoTinyDevice::begin() {
	this->physical=tfb_physical_create();
	this->physical->tag=this;
	this->physical->write=ArduinoTinyDevice_write;
	this->physical->read=ArduinoTinyDevice_read;
	this->physical->available=ArduinoTinyDevice_available;
	this->physical->millis=ArduinoTinyDevice_millis;
	this->physical->write_enable=ArduinoTinyDevice_write_enable;

	if (this->outputEnablePin>=0) {
		pinMode(this->outputEnablePin,OUTPUT);
		digitalWrite(this->outputEnablePin,LOW);
	}

	this->sock=tfb_sock_create(this->physical,this->name);
	this->sock->tag=this;
	tfb_sock_event_func(this->sock,ArduinoTinyDevice_handle_event);
}

void ArduinoTinyDevice::loop() {
	tfb_sock_tick(this->sock);
}

int ArduinoTinyDevice::available() {
	return tfb_sock_available(this->sock);
}

bool ArduinoTinyDevice::isConnected() {
	return tfb_sock_is_connected(this->sock);
}

int ArduinoTinyDevice::recv(uint8_t *buffer, size_t length) {
	return tfb_sock_recv(this->sock,buffer,length);
}

String ArduinoTinyDevice::recvString() {
    int size=this->available();
    if (size<=0) return String();

    String out;
    out.reserve(size);
	for (int i=0; i<size; i++)
    	out+='\0';

    char *buf=out.begin();
	tfb_sock_recv(this->sock,buf,size);

    return out;
}

int ArduinoTinyDevice::send(uint8_t *buffer, size_t length) {
	while (tfb_sock_is_connected(this->sock) &&
			!tfb_sock_is_send_available(this->sock))
		tfb_sock_tick(this->sock);

	return tfb_sock_send(this->sock,buffer,length);
}

void ArduinoTinyDevice::flush() {
	while (tfb_sock_is_connected(this->sock) &&
			!tfb_sock_is_flushed(this->sock))
		tfb_sock_tick(this->sock);
}

bool ArduinoTinyDevice::isFlushed() {
	return tfb_sock_is_flushed(this->sock);
}
