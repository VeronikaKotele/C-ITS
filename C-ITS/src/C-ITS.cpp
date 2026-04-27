// C-ITS.cpp : Defines the entry point for the application.
//

#include <mqtt/async_client.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "C-ITS.h"
#include "RoadsideUnit.h"
#include "Vehicle.h"
#include "constants.h"

std::atomic<bool> running{ true };

void start_rsu_subscriber() {
    RoadsideUnit rsu(123);
    try {
		rsu.connect();
        rsu.subscribeToListenCam();
		rsu.subscribeToListenSrem();
        rsu.startProcessingPriorityRequests();
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
		}
        rsu.stopProcessingRequests();
		rsu.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "RoadsideUnit thread error: " << e.what() << "\n";
    }
}

void start_bus_publisher() {
    Vehicle vehicle(rand() % 100, VehicleType::BUS);
    try {
        vehicle.connect();
        vehicle.subscribeToListenSsem();
        while (running)
        {
            if ((rand() % 10) < 2) { // 20% chance to send a priority request
                vehicle.requestPriority();
            }
            else {
                vehicle.sendSpeedStatus();
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
	    vehicle.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Vehicle thread error: " << e.what() << "\n";
    }
}

void start_car_publisher() {
    Vehicle vehicle(rand() % 100, VehicleType::CAR);
    try {
        vehicle.connect();
        while (running)
        {
            vehicle.sendSpeedStatus();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        vehicle.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Vehicle thread error: " << e.what() << "\n";
    }
}

void start_emergency_publisher() {
    Vehicle vehicle(rand() % 100, VehicleType::EMERGENCY);
    try {
        vehicle.connect();
        vehicle.subscribeToListenSsem();
        auto local_counter = 0;
        while (running)
        {
			if (local_counter % 10 < 5) { // fisrt 5 cesonds send priority request
                vehicle.requestPriority();
            }
            else { // then 5 seconds send speed status, and repeat
                vehicle.sendSpeedStatus();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            local_counter++;
        }
        vehicle.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Vehicle thread error: " << e.what() << "\n";
    }
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::cout << "Starting C-ITS SREM simulation...\n";

    std::thread rsu_thread(start_rsu_subscriber);
    pthread_setname_np(rsu_thread.native_handle(), "RSU_Thread");

	// Give the RSU some time to set up before starting the publishers
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread bus_thread(start_bus_publisher);
    pthread_setname_np(bus_thread.native_handle(), "BUS_Thread");

    std::thread car_thread(start_car_publisher);
    pthread_setname_np(car_thread.native_handle(), "CAR_Thread");

	std::thread emergency_thread(start_emergency_publisher);
    pthread_setname_np(emergency_thread.native_handle(), "EMERGENCY_Thread");

    std::cout << "Press Enter to stop simulation...\n";
    std::cin.get();

    running = false;

    if (rsu_thread.joinable()) {
        rsu_thread.join();
    }

    if (bus_thread.joinable()) {
        bus_thread.join();
    }

    if (car_thread.joinable()) {
        car_thread.join();
	}

    if (emergency_thread.joinable()) {
        emergency_thread.join();
	}

    google::protobuf::ShutdownProtobufLibrary();

    std::cout << "Simulation stopped.\n";
    return 0;
}
