#include "AlexaHub.hpp"

#include <chrono>
#include <thread>

int main() {
	AlexaHub hub{};

	hub.run();

	return 0;
}
