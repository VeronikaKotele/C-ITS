// C-ITS.cpp : Defines the entry point for the application.
//

#include <mqtt/async_client.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "C-ITS.h"
#include "srem.pb.h"
#include "RoadsideUnit.h"
#include "Vehicle.h"
#include "constants.h"

std::atomic<bool> running{ true };

void start_rsu_subscriber() {
    RoadsideUnit rsu("intersection1");
    try {
		rsu.connect();
		rsu.subscribeToListenSREM();
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		rsu.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "RoadsideUnit thread error: " << e.what() << "\n";
    }
}

void start_bus_publisher() {
    Vehicle vehicle("car1");
    try {
        vehicle.connect();
        while (running)
        {
            if (rand() % 10 < 2) { // 20% chance to send a priority request
                vehicle.requestPriority();
            }
            vehicle.sendSpeedStatus();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
	    vehicle.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Veahicle thread error: " << e.what() << "\n";
    }
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::cout << "Starting C-ITS SREM simulation...\n";

    std::thread rsu_thread(start_rsu_subscriber);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::thread bus_thread(start_bus_publisher);

    std::cout << "Press Enter to stop simulation...\n";
    std::cin.get();

    running = false;

    if (bus_thread.joinable()) {
        bus_thread.join();
    }

    if (rsu_thread.joinable()) {
        rsu_thread.join();
    }

    google::protobuf::ShutdownProtobufLibrary();

    std::cout << "Simulation stopped.\n";
    return 0;
}
