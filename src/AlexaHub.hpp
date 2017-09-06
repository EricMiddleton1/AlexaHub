#pragma once

#include <vector>
#include <memory>

#include "LightHub.hpp"
#include "LightNode.hpp"
#include "LightStrip.hpp"
#include "CloudServer.hpp"
#include "PeriodicTimer.hpp"

#include "json/json.h"

class AlexaHub {
public:
	AlexaHub();
	~AlexaHub();

	void run();

private:
	const uint16_t PORT = 54923;
	const uint16_t SERVER_PORT = 8080;

	struct Light {
		std::shared_ptr<LightStrip> strip;
		std::string id;

		operator bool() const;
	};

	std::vector<Light> getLights() const;
	static std::string getLightName(const std::shared_ptr<LightNode>& node, int index);
	Light getLightById(const std::string& id) const;

	std::string processCloudMsg(const std::string& msg);

	Json::Value processSetColor(Light& device, const Color& c);
	Json::Value processSetPercentage(Light& device, double brightness);
	Json::Value processTurnOn(Light& device);
	Json::Value processTurnOff(Light& device);

	Json::Value processDiscover();

	LightHub hub;

	boost::asio::io_service ioService;
	std::unique_ptr<boost::asio::io_service::work> ioWork;
	CloudServer server;

	PeriodicTimer updateTimer;
};
