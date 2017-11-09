#pragma once

#include <vector>
#include <memory>

#include "LightHub.hpp"
#include "Light.hpp"
#include "CloudServer.hpp"
#include "PeriodicTimer.hpp"

#include "json/json.h"

class AlexaHub {
public:
	AlexaHub();
	~AlexaHub();

	void run();

private:
	const uint16_t PORT = 5492;
	const uint16_t SERVER_PORT = 9160;

	std::vector<std::shared_ptr<Light>> getLights() const;
	std::shared_ptr<Light> getLightById(const std::string& id) const;

	std::string processCloudMsg(const std::string& msg);

	Json::Value processSetColor(std::shared_ptr<Light>& device, const Color& c);
	Json::Value processSetPercentage(std::shared_ptr<Light>& device, double brightness);
	Json::Value processTurnOn(std::shared_ptr<Light>& device);
	Json::Value processTurnOff(std::shared_ptr<Light>& device);

	Json::Value processDiscover();

	LightHub hub;

	boost::asio::io_service ioService;
	std::unique_ptr<boost::asio::io_service::work> ioWork;
	CloudServer server;

	PeriodicTimer updateTimer;
};
