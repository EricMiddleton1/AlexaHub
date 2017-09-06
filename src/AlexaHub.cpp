#include "AlexaHub.hpp"

#include <algorithm>

using namespace boost::asio;

AlexaHub::Light::operator bool() const {
	return strip.get() != nullptr;
}

AlexaHub::AlexaHub()
	:	hub{PORT}
	,	server{ioService, SERVER_PORT, [this](const std::string& msg) {
			return processCloudMsg(msg);
		}}
	,	ioWork{std::make_unique<io_service::work>(ioService)}
	,	updateTimer{ioService, std::chrono::milliseconds(1000), [this]() {
		auto lights = getLights();
/*
		std::cout << "Discovered Lights:\n";
		for(auto& light : lights) {
			std::cout << "\t" << light.id << "\n";
		}
		std::cout << std::endl;
*/
	}} {
}

AlexaHub::~AlexaHub() {
	ioWork.reset();
}

void AlexaHub::run() {
	ioService.run();
}

std::vector<AlexaHub::Light> AlexaHub::getLights() const {
	std::vector<Light> strips;

	auto nodes = hub.getNodes();

	for(auto& node : nodes) {
		int index = 0;
		std::for_each(node->stripBegin(), node->stripEnd(),
			[&strips, &node, &index](auto& strip) {
				auto type = strip->getType();
				if(type == LightStrip::Type::Analog || type == LightStrip::Type::Digital) {
					strips.push_back({strip, getLightName(node, index++)});
				}
			});
	}

	return strips;
}

std::string AlexaHub::getLightName(const std::shared_ptr<LightNode>& node, int index) {

	return node->getName() + ":" + std::to_string(index);
}

AlexaHub::Light AlexaHub::getLightById(const std::string& id) const {
	auto lights = getLights();

	auto itr = std::find_if(lights.begin(), lights.end(), [&id](const Light& l) {
		return id == l.id;
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
				auto c = Color::HSV(hsb["hue"].asDouble(),
					hsb["saturation"].asDouble(),
					hsb["brightness"].asDouble());

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

		device["applianceId"] = light.id;
		device["manufacturerName"] = "ICEE";
		device["modelName"] = "ICEE - SmartLight";
		device["version"] = "0.1";

		auto name = light.id;
		std::replace(name.begin(), name.end(), ':', ' ');
		device["friendlyName"] = name;
		device["friendlyDescription"] = name + " connected via AlexaHub by ICEE";
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

Json::Value AlexaHub::processSetColor(Light& device, const Color& c) {
	device.strip->tcpSetColor(c);

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "SetColorConfirmation";
	response["header"]["payloadVersion"] = 2;

	response["payload"]["achievedState"]["color"]["hue"] = c.getHue();
	response["payload"]["achievedState"]["color"]["saturation"] = c.getHSVSaturation();
	response["payload"]["achievedState"]["color"]["brightness"] = c.getValue();

	return response;
}

Json::Value AlexaHub::processSetPercentage(Light& device, double brightness) {
	device.strip->tcpSetBrightness(brightness / 100.0);

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "SetPercentageConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}

Json::Value AlexaHub::processTurnOn(Light& device) {
	device.strip->tcpSetBrightness(1.0);

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "TurnOnConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}

Json::Value AlexaHub::processTurnOff(Light& device) {
	device.strip->tcpSetBrightness(0.0);

	Json::Value response;

	response["header"]["messageId"] = "0000-0000-0000-0000";
	response["header"]["namespace"] = "Alexa.ConnectedHome.Control";
	response["header"]["name"] = "TurnOffConfirmation";
	response["header"]["payloadVersion"] = 2;

	return response;
}
