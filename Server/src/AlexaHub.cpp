#include "AlexaHub.hpp"

#include <algorithm>

using namespace boost::asio;
using namespace std;

AlexaHub::AlexaHub()
	:	hub{PORT}
	,	ioWork{std::make_unique<io_service::work>(ioService)} 
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
		}} {
	
	handlers.emplace("Alexa.Discovery"s,
		[this](const string& command, const Json::Value& input) -> Json::Value {
			if(command == "Discover") {
				return processDiscover();
			}
			else {
				return Json::Value{};
			}
		});
	handlers.emplace("Alexa.PowerController"s,
		[this](const string& command, const Json::Value& input) -> Json::Value {
			return processPowerController(command, input);
		});
	handlers.emplace("Alexa.BrightnessController"s,
		[this](const string& command, const Json::Value& input) -> Json::Value {
			return processBrightnessController(command, input);
		});
	handlers.emplace("Alexa.ColorController"s,
		[this](const string& command, const Json::Value& input) -> Json::Value {
			return processColorController(command, input);
		});

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
	
	auto directive = root["directive"];

	std::string nspace = directive["header"]["namespace"].asString();
	std::string command = directive["header"]["name"].asString();
	
	Json::Value response;
	
	try {
		response = handlers.at(nspace)(command, root);
	}
	catch(...) {
		std::cerr << "[Error] No handler found for namespace '" << nspace << "'\n";
	}
	
	std::cout << std::endl;

	return Json::FastWriter().write(response);
}

Json::Value AlexaHub::processDiscover() {
	Json::Value response;

	Json::Value endpoints{Json::arrayValue};

	auto lights = getLights();
	for(auto& light : lights) {
		Json::Value endpoint;

		auto types = Json::Value{Json::arrayValue};
		types.append("LIGHT");
		endpoint["displayCategories"] = types;

		endpoint["endpointId"] = light->getFullName();
		endpoint["manufacturerName"] = "ICEE";
		endpoint["description"] = "ICEE - SmartLight";

		endpoint["friendlyName"] = light->getName();
		
		auto capabilities = Json::Value{Json::arrayValue};
		Json::Value capability;
		Json::Value property;

		capability["type"] = "AlexaInterface";
		capability["interface"] = "Alexa";
		capability["version"] = "3";
		capabilities.append(capability);
		
		capability["type"] = "AlexaInterface";
		capability["interface"] = "Alexa.ColorController";
		capability["version"] = "3";
		capability["retrievable"] = true;
		capability["properties"]["supported"] = Json::Value{Json::arrayValue};
		property["name"] = "color";
		capability["properties"]["supported"].append(property);
		capabilities.append(capability);
		
		capability["interface"] = "Alexa.BrightnessController";
		capability["properties"]["supported"] = Json::Value{Json::arrayValue};
		property["name"] = "brightness";
		capability["properties"]["supported"].append(property);
		capabilities.append(capability);

		capability["interface"] = "Alexa.PowerController";
		capability["properties"]["supported"] = Json::Value{Json::arrayValue};
		property["name"] = "powerState";
		capability["properties"]["supported"].append(property);
		capabilities.append(capability);
		
		endpoint["capabilities"] = capabilities;
		endpoints.append(endpoint);
	}

	response["event"]["header"]["messageId"] = "0000-0000-0000-0000";
	response["event"]["header"]["name"] = "Discover.Response";
	response["event"]["header"]["namespace"] = "Alexa.Discovery";
	response["event"]["header"]["payloadVersion"] = "3";
	response["event"]["payload"]["endpoints"] = endpoints;

	return response;
}

Json::Value AlexaHub::processPowerController(const string& command,
	const Json::Value& input) {

	Json::Value result;
	Json::Value property;

	auto lightName = input["directive"]["endpoint"]["endpointId"].asString();

	result["context"]["properties"] = Json::Value{Json::arrayValue};
	property["namespace"] = "Alexa.PowerController";
	property["name"] = "powerState";
	property["value"] = (command == "TurnOn" ? "ON" : "OFF");
	result["context"]["properties"].append(property);

	result["event"]["header"]["namespace"] = "Alexa";
	result["event"]["header"]["name"] = "Response";
	result["event"]["header"]["payloadVersion"] = "3";
	result["event"]["header"]["messageId"] = "0000-0000-0000-0000";
	result["event"]["endpoint"]["endpointId"] =
		input["directive"]["endpoint"]["endpointId"];
	result["event"]["endpoint"]["scope"] = input["directive"]["endpoint"]["scope"];

	auto light = getLightById(lightName);
	if(!light) {
		std::cerr << "[Error] AlexaHub: Light '" << lightName << "' not found"
			<< std::endl;
	}
	else if(command == "TurnOn") {
		hub.turnOn(*light);
	}
	else if(command == "TurnOff") {
		hub.turnOff(*light);
	}
	else {
		std::cerr << "[Error] AlexaHub: Unrecognized command: " << command
			<< std::endl;
	}

	return result;
}

Json::Value AlexaHub::processBrightnessController(const string& command,
	const Json::Value& input) {

	Json::Value result;
	Json::Value property;

	auto lightName = input["directive"]["endpoint"]["endpointId"].asString();



	result["event"]["header"]["namespace"] = "Alexa";
	result["event"]["header"]["name"] = "Response";
	result["event"]["header"]["payloadVersion"] = "3";
	result["event"]["header"]["messageId"] = "0000-0000-0000-0000";
	result["event"]["endpoint"]["endpointId"] =
		input["directive"]["endpoint"]["endpointId"];
	result["event"]["endpoint"]["scope"] = input["directive"]["endpoint"]["scope"];

	auto light = getLightById(lightName);
	if(!light) {
		std::cerr << "[Error] AlexaHub: Light '" << lightName << "' not found"
			<< std::endl;
	}
	else if(command == "SetBrightness") {
		auto brightness = 255*input["directive"]["payload"]["brightness"].asInt()/100;

		hub.setBrightness(*light, brightness);
		
		result["context"]["properties"] = Json::Value{Json::arrayValue};
		property["namespace"] = "Alexa.BrightnessController";
		property["name"] = "brightness";
		property["value"] = input["directive"]["payload"]["brightness"];
		result["context"]["properties"].append(property);

	}
	else if(command == "AdjustBrightness") {
		auto deltaBrightness = input["directive"]["payload"]["brightnessDelta"].asInt();
		
		result["context"]["properties"] = Json::Value{Json::arrayValue};
		property["namespace"] = "Alexa.BrightnessController";
		property["name"] = "brightness";
		property["value"] = 50;
		result["context"]["properties"].append(property);

		hub.changeBrightness(*light, deltaBrightness);
	}
	else {
		std::cerr << "[Error] AlexaHub: Unrecognized command: " << command
			<< std::endl;
	}


	return result;
}

Json::Value AlexaHub::processColorController(const string& command,
	const Json::Value& input) {

	Json::Value result;
	Json::Value property;

	auto lightName = input["directive"]["endpoint"]["endpointId"].asString();

	result["context"]["properties"] = Json::Value{Json::arrayValue};
	property["namespace"] = "Alexa.ColorController";
	property["name"] = "color";
	property["value"] = input["directive"]["payload"]["color"];
	result["context"]["properties"].append(property);

	result["event"]["header"]["namespace"] = "Alexa";
	result["event"]["header"]["name"] = "Response";
	result["event"]["header"]["payloadVersion"] = "3";
	result["event"]["header"]["messageId"] = "0000-0000-0000-0000";
	result["event"]["endpoint"]["endpointId"] =
		input["directive"]["endpoint"]["endpointId"];
	result["event"]["endpoint"]["scope"] = input["directive"]["endpoint"]["scope"];

	auto light = getLightById(lightName);
	if(!light) {
		std::cerr << "[Error] AlexaHub: Light '" << lightName << "' not found"
			<< std::endl;
	}
	else if(command == "SetColor") {
		auto hsb = input["directive"]["payload"]["color"];
		auto color = Color::HSV(
			255.f*hsb["hue"].asDouble()/360.f,
			255.f*hsb["saturation"].asDouble(),
			255.f*hsb["brightness"].asDouble());

		hub.setColor(*light, color);
	}
	else {
		std::cerr << "[Error] AlexaHub: Unrecognized command: " << command
			<< std::endl;
	}

	return result;
}
