#include "CloudServer.hpp"

#include <algorithm>
#include <iostream>

using namespace boost::asio;

CloudServer::CloudServer(io_service& _ioService, uint16_t _port,
	const ReceiveHandler& _handler)
	:	ioService{_ioService}
	,	endpoint{ip::tcp::v4(), _port}
	, acceptor{ioService, endpoint}
	,	socket{ioService}
	,	handler{_handler} {
	
	ioService.post([this]() {
		startAccept();
	});
}

void CloudServer::startAccept() {
	std::cout << "[Info] CloudServer: Starting Accept" << std::endl;

	acceptor.async_accept(socket, clientEndpoint, [this](const boost::system::error_code& ec) {
		if(ec) {
			std::cout << "[Error] CloudServer::handleAccept: " << ec.message() << std::endl;
			startAccept();
		}
		else {
			std::cout << "[Info] CoudServer: Client connected" << std::endl;
			startListen();
		}
	});
}

void CloudServer::startListen() {
	socket.async_receive(buffer(readBuffer), [this](const boost::system::error_code& ec,
		size_t bytesTransferred) {

		if(ec || bytesTransferred == 0) {
			if(ec) {
				std::cout << "[Error] CloudServer::cbReceive: " << ec.message() << std::endl;
			}
			std::cout << "[Info] CloudServer: Client Disconnected" << std::endl;
			std::cout << msgBuffer << std::endl;

			socket.cancel();
			socket.close();
			msgBuffer.clear();

			startAccept();
		}
		else {
			msgBuffer.insert(msgBuffer.end(), readBuffer.begin(), readBuffer.begin() + bytesTransferred);

			std::string msg;
			while(parseMessage(msgBuffer, msg)) {
				auto response = new std::string{handler(msg)};
				
				if(response->length() > 0) {
					socket.async_send(buffer(*response), [response](const boost::system::error_code& ec,
						std::size_t bytesTransferred) {
						if(ec) {
							std::cout << "[Error] CloudServer::cbSendResponse: " << ec.message() << std::endl;
						}
						else if(bytesTransferred != response->size()) {
							std::cout << "[Error] CloudServer::cbSendResponse: Tried to send " << response->size()
								<< " bytes, actually sent " << bytesTransferred << std::endl;
						}
						else {
							std::cout << "[Info] CloudServer::cbSendResponse: Response sent" << std::endl;
						}

						delete response;
					});
				}
				else {
					delete response;

					std::cout << "[Info] CloudServer: Empty response, closing socket" << std::endl;

					socket.cancel();
					socket.close();
					msgBuffer.clear();

					startAccept();

					return;
				}
			}

			startListen();
		}
	});
}

bool CloudServer::parseMessage(std::string& buffer, std::string& msg) {
	const std::string endToken{"\r\n\r\n"};

	auto itr = std::search(buffer.begin(), buffer.end(),
		endToken.begin(), endToken.end());

	if(itr == buffer.end()) {
		return false;
	}
	else {
		msg = std::string(buffer.begin(), itr);
		buffer.erase(buffer.begin(), itr + endToken.length());

		return true;
	}
}
