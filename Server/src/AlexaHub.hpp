#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <functional>

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

	Json::Value processDiscover();
	Json::Value processPowerController(const std::string& command,
		const Json::Value& input);
	Json::Value processBrightnessController(const std::string& command,
		const Json::Value& input);
	Json::Value processColorController(const std::string& command,
		const Json::Value& input);

	LightHub hub;

	boost::asio::io_service ioService;
	std::unique_ptr<boost::asio::io_service::work> ioWork;
	CloudServer server;

	std::map<std::string,
		std::function<Json::Value(const std::string&, const Json::Value)>> handlers;
};
