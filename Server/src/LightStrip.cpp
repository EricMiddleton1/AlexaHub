#include "LightStrip.hpp"
#include "LightNode.hpp"

#include <string>

size_t LightStrip::globalCount = 0;

LightBuffer::LightBuffer(LightStrip *_strip)
	:	strip(_strip) {
	
	strip->bufferMutex.lock();
}

LightBuffer::~LightBuffer() {
	std::unique_lock<std::mutex> pixelLock(strip->pixelMutex);

	strip->pixels = strip->pixelBuffer;

	strip->bufferMutex.unlock();
}

LightStrip::Type LightBuffer::getType() const {
	return strip->type;
}

void LightBuffer::setAll(const Color& c) {
	for(auto& pixel : strip->pixelBuffer) {
		pixel = c;
	}
}

std::vector<Color>& LightBuffer::getPixelBuffer() {
	return strip->pixelBuffer;
}

const std::vector<Color>& LightBuffer::getPixelBufferConst() const {
	return strip->pixelBuffer;
}

LightStrip::LightStrip(Type _type, size_t _size)
	:	type{_type}
	,	pixels{_size}
	,	pixelBuffer{_size} {

	std::unique_lock<std::mutex> countLock(countMutex);

	id = globalCount++;
}

void LightStrip::setNode(LightNode* _node) {
	node = _node;
}

size_t LightStrip::getID() const {
	return id;
}

LightStrip::Type LightStrip::getType() const {
	return type;
}

std::vector<Color> LightStrip::getPixels() const {
	std::unique_lock<std::mutex> pixelLock(pixelMutex);

	return pixels;
}

size_t LightStrip::getSize() const {
	std::unique_lock<std::mutex> pixelLock(pixelMutex);

	return pixels.size();
}

void LightStrip::tcpSetBrightness(double brightness) {
	//TODO

	getBuffer()->setAll(Color::HSV(0, 0, brightness));

	node->update();
}

void LightStrip::tcpSetColor(const Color& c) {
	//TODO

	getBuffer()->setAll(c);

	node->update();
}

void LightStrip::tcpTurnOn() {
	//TODO

	getBuffer()->setAll(Color::HSV(0, 0, 1.0));

	node->update();
}

void LightStrip::tcpTurnOff() {
	//TODO

	getBuffer()->setAll(Color::HSV(0, 0, 0.0));

	node->update();
}
