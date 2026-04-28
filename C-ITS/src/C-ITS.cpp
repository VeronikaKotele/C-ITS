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
#include "Interfaces/SpawnLocation.h"

std::atomic<bool> running{ true };

void startRoadsideUnitSimulation(SpawnLocation location, WebSocketBridge& wsBridge) {
    RoadsideUnit rsu(123, wsBridge);
    try {
		rsu.connect();
        rsu.subscribeToListenCam();
		rsu.subscribeToListenSrem();
        rsu.startProcessingPriorityRequests();
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            /*
            * const phases: SignalPhase[] = ["RED", "GREEN", "YELLOW"];

            onEvent({
              type: "spatem",
              intersectionId: 1,
              phase: phases[Math.floor(tick / 6) % phases.length],
              remainingSeconds: 20 - (tick % 20),
              timestampMs: now,
            });
            */
		}
        rsu.stopProcessingRequests();
		rsu.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "RoadsideUnit thread error: " << e.what() << "\n";
    }
}

void startVehicleSimulation(
    VehicleType type,
    int messagePauseMs,
    VehicleState state,
    SpawnLocation destinationLocation,
    WebSocketBridge& wsBridge)
{
    state.station_id = static_cast<uint32_t>(rand() % 100);
    Vehicle vehicle(type, state);
	vehicle.setDestination(destinationLocation);

    try {
        vehicle.connect();
        vehicle.subscribeToListenSsem();
        for (int tick = 0; running; ++tick)
        {
            vehicle.move();

            vehicle.sendCAM();

			if (type == VehicleType::BUS && tick % 10 < 2) { // BUS requests priority for 20% of the time
                vehicle.requestPriority();
            }
            else if (type == VehicleType::EMERGENCY && tick % 10 < 5) {// EMERGENCY requests priority for 50% of the time
                vehicle.requestPriority();
			}
            
            std::this_thread::sleep_for(std::chrono::milliseconds(messagePauseMs));
        }
        vehicle.disconnect();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Vehicle thread error: " << e.what() << "\n";
    }
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::cout << "Starting WebSocket bridge\n";
    WebSocketBridge wsBridge(8080);

    std::thread rsu_thread;
    SpawnLocation rsuSpawnLocation{ 48.776, 9.183 };

    std::vector<std::thread> vehicleThreads;
	std::vector<std::string> vehicleThreadNames = { "BUS_Thread", "CAR_Thread", "EMERGENCY_Thread" };
	std::vector<VehicleType> vehicleTypes = { VehicleType::BUS, VehicleType::CAR, VehicleType::EMERGENCY };
	std::vector<int> vehicleMessagePauseSeconds = { 200, 300, 100 }; // BUS sends every 2s, CAR every 3s, EMERGENCY every 1s
    std::vector<VehicleState> vehicleStates;
    vehicleStates.push_back(VehicleState{ // BUS
        .latitude = 48.7758,
        .longitude = 9.1829,
        .speed = 30,
    });
    vehicleStates.push_back(VehicleState{ // CAR
        .latitude = 48.7765,
        .longitude = 9.1840,
        .speed = 50,
    });
    vehicleStates.push_back(VehicleState{ // EMERGENCY
        .latitude = 48.7748,
        .longitude = 9.1810,
        .speed = 120,
    });

    try {
        wsBridge.start();

        std::cout << "Starting C-ITS simulation...\n";
        rsu_thread = std::thread([&wsBridge, &rsuSpawnLocation]() {
            startRoadsideUnitSimulation(rsuSpawnLocation, wsBridge);
            });
        pthread_setname_np(rsu_thread.native_handle(), "RSU_Thread");

	    // Give the RSU some time to set up before starting the publishers
        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (size_t vehicleNo = 0; vehicleNo < vehicleTypes.size(); ++vehicleNo) {
            vehicleThreads.emplace_back([&, vehicleNo]() {
                startVehicleSimulation(vehicleTypes[vehicleNo], vehicleMessagePauseSeconds[vehicleNo], vehicleStates[vehicleNo], rsuSpawnLocation, wsBridge);
            });
            pthread_setname_np(vehicleThreads.back().native_handle(), vehicleThreadNames[vehicleNo].c_str());
        }

        std::cout << "Press Enter to stop simulation...\n";
        std::cin.get();

        running = false;
    }
    catch (const std::exception& ex) {
        std::cerr << "[MAIN] Fatal error: " << ex.what() << "\n";
        running = false;
    }

    if (rsu_thread.joinable()) {
        rsu_thread.join();
    }

    for (auto& thread : vehicleThreads) {
        if (thread.joinable()) {
            thread.join();
        }
	}

    wsBridge.stop();
    google::protobuf::ShutdownProtobufLibrary();

    std::cout << "Simulation stopped.\n";
    return 0;
}
