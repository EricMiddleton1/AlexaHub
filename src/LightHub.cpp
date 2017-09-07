#include "LightHub.hpp"

#include "LightStripAnalog.hpp"
#include "LightStripDigital.hpp"
#include "LightStripMatrix.hpp"

#include <chrono>

using namespace std;

LightHub::LightHub(uint16_t _port, uint32_t _discoveryPeriod)
	:	socket(ioService, boost::asio::ip::udp::v4())
	,	port{_port}
	,	discoveryTimer(ioService, std::chrono::milliseconds(_discoveryPeriod),
		[this](){ discover(); })	{

	//Allow the socket to send broadcast packets
	socket.set_option(boost::asio::socket_base::broadcast(true));
	socket.set_option(boost::asio::socket_base::reuse_address(true));

	//Construct the work unit for the io_service
	ioWork.reset(new boost::asio::io_service::work(ioService));

	//Start the async thread
	asyncThread = std::thread(std::bind(&LightHub::threadRoutine, this));

	std::cout << "[Info] LightHub::LightHub: Now listening for packets"
		<< std::endl;

	std::cout << "[Info] LightHub::LightHub: Performing initial network "
		"discovery" << std::endl;

	//Post constructor setup (on local thread)
	ioService.post([this]() {
		discover();

		startListening();
	});
}

LightHub::~LightHub() {
	//Delete the work unit, allow io_service.run() to return
	ioWork.reset();

	asyncThread.join();
}

void LightHub::threadRoutine() {
	ioService.run();
}


void LightHub::discover() {
	socket.async_send_to(boost::asio::buffer(
		Packet::Ping().asDatagram()),
		boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), port),
		boost::bind(&LightHub::handleSendBroadcast, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

std::shared_ptr<LightNode> LightHub::getNodeByName(const std::string& name) {
	std::unique_lock<std::mutex> nodeLock(nodeMutex);

	auto found = std::find_if(std::begin(nodes), std::end(nodes),
		[&name](const std::shared_ptr<LightNode>& node) {
			return node->getName() == name;
		});
	
	if(found == std::end(nodes)) {
		//We didn't find a node
		throw Exception(LIGHT_HUB_NODE_NOT_FOUND, "LightHub::getNodeByName: "
			"node not found");
	}
	else {
		return *found;
	}
}

std::shared_ptr<LightNode> LightHub::getNodeByAddress(
	const boost::asio::ip::address& addr) {
	std::unique_lock<std::mutex> nodeLock(nodeMutex);

	auto found = std::find_if(std::begin(nodes), std::end(nodes),
		[&addr](const std::shared_ptr<LightNode>& node) {
			return node->getAddress() == addr;
		});

	if(found == std::end(nodes)) {
		//We didn't find a node
		throw Exception(LIGHT_HUB_NODE_NOT_FOUND, "LightHub::getNodeByAddress: "
			"node not found");
	}
	else {
		return *found;
	}
}

size_t LightHub::getNodeCount() const {
	return nodes.size();
}

size_t LightHub::getConnectedNodeCount() const {
	size_t connectedCount = 0;
	std::unique_lock<std::mutex> nodeLock(nodeMutex);

	for(auto& node : nodes) {
		connectedCount += node->getState() == LightNode::State::CONNECTED;
	}

	return connectedCount;
}

std::vector<std::shared_ptr<LightNode>> LightHub::getNodes() const {
	std::unique_lock<std::mutex> nodeLock(nodeMutex);

	return nodes;
}

void LightHub::handleSendBroadcast(const boost::system::error_code& ec,
	size_t) {
	if(ec) {
		std::cout << "[Error] Failed to send broadcast ping message: "
			<< ec.message() << std::endl;
	}
}

void LightHub::startListening() {
	//Start the async receive
	socket.async_receive_from(boost::asio::buffer(readBuffer),
		receiveEndpoint,
		boost::bind(&LightHub::handleReceive, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void LightHub::handleReceive(const boost::system::error_code& ec,
	size_t bytesTransferred) {
	
	if(ec) {
		std::cout << "[Error] Failed to receive from UDP socket" << std::endl;
	}
	else {
		Packet p;

		try {
			//Parse the datagram into a packet
			p = Packet(std::vector<uint8_t>(std::begin(readBuffer),
				std::begin(readBuffer) + bytesTransferred));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() == Packet::PACKET_INVALID_HEADER ||
					e.getErrorCode() == Packet::PACKET_INVALID_SIZE) {
				//Might be from some other application, we can ignore
				std::cout << "[Warning] LightHub::handleReceive: Invalid datagram "
					"received from " << receiveEndpoint.address() << std::endl;
			}
			else {
				//Weird!
				std::cout << "[Error] LightHub::handleReceive: Exception caught: "
					<< e.what() << std::endl;
			}
		}
		
		std::shared_ptr<LightNode> sendNode;

		//try to find the node associated with this packet
		try {
			sendNode = getNodeByAddress(receiveEndpoint.address());

			//Let the node handle the packet
			sendNode->receivePacket(p);
		}
		catch(const Exception& e) {
			if(e.getErrorCode() == LIGHT_HUB_NODE_NOT_FOUND) {
					//The sender is not in the list of connected nodes
					
				if(p.getID() == Packet::INFO) {
					auto payload = p.getPayload();
					if(payload.size() < 4) {
						std::cout << "[Error] LightHub::handleReceive: Info payload less than 4 "
							"bytes" << std::endl;
					}
					else {
						uint8_t analogCount = payload[0],
							digitalCount = payload[1],
							matrixCount = payload[2];
						
						if(payload.size() < (4 + 2*(digitalCount+matrixCount))) {
							throw Exception(LIGHT_HUB_INVALID_PAYLOAD,
								"LightHub::handleReceive: Expected payload at least "
								+ std::to_string(4 + 2*(digitalCount+matrixCount)) + " bytes, but "
								"is only " + std::to_string(payload.size()) + " bytes");
						}

						std::vector<std::shared_ptr<LightStrip>> strips;

						for(size_t i = 0; i < analogCount; ++i) {
							strips.emplace_back(std::make_shared<LightStripAnalog>());
						}

						for(size_t i = 0; i < digitalCount; ++i) {
							uint16_t size = (payload[3 + 2*i] << 8) | (payload[4 + 2*i]);

							strips.emplace_back(std::make_shared<LightStripDigital>(size));
						}

						for(size_t i = 0; i < matrixCount; ++i) {
							uint8_t width = payload[3 + 2*digitalCount + 2*i],
								height = payload[4 + 2*digitalCount + 2*i];

							std::cout << "Matrix: " << (int)width << ", " << (int)height << std::endl;

							strips.emplace_back(std::make_shared<LightStripMatrix>(width, height));
						}

						std::string name(payload.begin() + 3 + 2*(digitalCount+matrixCount),
							payload.end());
						
						auto newNode = std::make_shared<LightNode>(ioService, name, strips,
							receiveEndpoint);

						{
							//Store the new node
							std::unique_lock<std::mutex> nodeLock(nodeMutex);
							nodes.push_back(newNode);
						}

						//Send a signal
						sigNodeDiscover(newNode);
					}
				}
			}
			else {
				//Some other error occurred
				std::cout << "[Error] LightHub::handleReceive: Exception thrown: "
					<< e.what() << std::endl;
			}
		}
	}

	startListening();
}

void LightHub::updateLights() {
	std::unique_lock<std::mutex> nodeLock(nodeMutex);

	for(auto& node : nodes) {
		node->update();
	}
}