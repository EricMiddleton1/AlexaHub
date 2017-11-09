#include "AlexaHub.hpp"

#include <algorithm>

using namespace boost::asio;

AlexaHub::AlexaHub()
	:	hub{PORT}
	,	server{ioService, SERVER_PORT, [this](const std::string& msg) {
			try {
				auto response = processCloudMsg(msg);

				std::cout << "\n" << response << std::endl;

				return response;
			}
			catch(const std::exception& e) {
				std::cout << "[Error] AlexaHub::processCloudMsg: " << e.what() << std::endl;

				return std::string{};
			}
		}}
	,	ioWork{std::make_unique<io_service::work>(ioService)}
	,	updateTimer{ioService, std::chrono::milliseconds(1000), [this]() {
			for(const auto& node : hub) {
				//std::cout << node.second.name << "\n";

				for(const auto& light : node.second.lights) {
					//std::cout << "\t" << light->getName() << "\n";
				}
			}
			//std::cout << std::endl;
	}} {
}

AlexaHub::~AlexaHub() {
	ioWork.reset();
}

void AlexaHub::run() {
	ioService.run();
}

std::vector<std::shared_ptr<Light>> AlexaHub::getLights() const {
	std::vector<std::shared_ptr<Light>> lights;

	for(auto& node : hub) {
		for(const auto& light : node.second.lights) {
			lights.push_back(light);
		}
	}

	return lights;
}

std::shared_ptr<Light> AlexaHub::getLightById(const std::string& id) const {
	auto lights = getLights();

	auto itr = std::find_if(lights.begin(), lights.end(), [&id](const auto& l) {
		return id == l->getFullName();
	});

	if(itr == lights.end()) {
		return {};
	}
	else {
		return *itr;
	}
}

std::string AlexaHub::processCloudMsg(const std::string& msg) {
	std::cout << "[Info] Received Cloud Message:\n" << msg << "\n";

	Json::Value root = msg;
	Json::Reader reader;
	reader.parse(msg, root);
	
	std::string nspace = root["header"]["namespace"].asString();
	std::string command = root["header"]["name"].asString();

	Json::Value response;
	std::string responseStr;
	
	if(nspace == "Alexa.ConnectedHome.Control") {
		auto deviceName = root["payload"]["appliance"]["applianceId"].asString();
		auto device = getLightById(deviceName);
		
		if(device) {
			if(command == "SetColorRequest") {
				const auto hsb = root["payload"]["color"];
				auto c = Color::HSV(255.f*hsb["hue"].asDouble()/360.f,
					255.f*hsb["saturation"].asDouble(),
					255.f*hsb["brightness"].asDouble());

					response = processSetColor(device, c);
			}
			else if(command == "SetPercentageRequest") {
				response = processSetPercentage(device,
					root["payload"]["percentageState"]["value"].asDouble());
			}
			else if(command == "TurnOnRequest") {
				response = processTurnOn(device);
			}
			else if(command == "TurnOffRequest") {
				response = processTurnOff(device);
			}
		}
	}
	else if(nspace == "Alexa.ConnectedHome.Discovery") {
		if(command == "DiscoverAppliancesRequest") {
			response = processDiscover();
		}
	}

	if(response.isMember("header")) {
		responseStr = Json::FastWriter().write(response);

		std::cout << responseStr << "\n";
	}
	else {
		std::cout << "\t[Error] Empty response!\n";
	}
	
	std::cout << std::endl;

	return responseStr;
}

Json::Value AlexaHub::processDiscover() {
	Json::Value response;

	Json::Value devices{Json::arrayValue};

	auto lights = getLights();
	for(auto& light : lights) {
		Json::Value device;

		auto types = Json::Value{Json::arrayValue};
		types.append("LIGHT");
		device["applianceTypes"] = types;

		device["applianceId"] = light->getFullName();
		device["manufacturerName"] = "ICEE";
		device["modelName"] = "ICEE - SmartLight";
		device["version"] = "0.1";

		device["friendlyName"] = light->getName();
		device["friendlyDescription"] = light->getName() + " connected via AlexaHub by ICEE";
		device["isReachable"] = true;
		
		auto actions = Json::Value{Json::arrayValue};
		actions.append("setColor");
		actions.append("turnOn");
		actions.append("turnOff");
		actions.append("setPercentage");
		device["actions"] = actions;

		device["additionalApplianceDetails"] = Json::objectValue;

		devices.append(device);
	}

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["name"] = "DiscoverAppliancesResponse";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Discovery";
	response["header"]["payloadVersion"] = 2;
	response["payload"]["discoveredAppliances"] = devices;

	return response;
}

Json::Value AlexaHub::processSetColor(std::shared_ptr<Light>& device, const Color& c) {
	device->getBuffer().setAll(c);

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "SetColorConfirmation";
	response["header"]["payloadVersion"] = 2;

	response["payload"]["achievedState"]["color"]["hue"] = c.getHue();
	response["payload"]["achievedState"]["color"]["saturation"] = c.getSat();
	response["payload"]["achievedState"]["color"]["brightness"] = c.getVal();

	return response;
}

Json::Value AlexaHub::processSetPercentage(std::shared_ptr<Light>& device, 
	double brightness) {
	
	device->getBuffer().setAll({255.f*brightness/100.f, 255.f*brightness/100.f,
		255.f*brightness/100.f});

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "SetPercentageConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}

Json::Value AlexaHub::processTurnOn(std::shared_ptr<Light>& device) {
	device->getBuffer().setAll({255, 255, 255});

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "TurnOnConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}

Json::Value AlexaHub::processTurnOff(std::shared_ptr<Light>& device) {
	device->getBuffer().setAll({0, 0, 0});

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "TurnOffConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}
