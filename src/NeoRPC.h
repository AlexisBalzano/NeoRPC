#pragma once
#include <memory>
#include <thread>
#include <vector>

#include <discord-rpc.hpp>
#include "SDK.h"
#include "core/NeoRPCCommandProvider.h"

using namespace PluginSDK;


namespace rpc {
    constexpr auto APPLICATION_ID = "1408567135428673546";
    static int64_t StartTime;
    static bool SendPresence = true;

    class NeoRPCCommandProvider;

    class NeoRPC : public BasePlugin
    {
    public:
        NeoRPC();
        ~NeoRPC();

		// Plugin lifecycle methods
        void Initialize(const PluginMetadata& metadata, CoreAPI* coreAPI, ClientInformation info) override;
        void Shutdown() override;
        void Reset();
        PluginMetadata GetMetadata() const override;

        // Radar commands
        void DisplayMessage(const std::string& message, const std::string& sender = "");
		
        // Scope events
        void OnControllerDataUpdated(const ControllerData::ControllerDataUpdatedEvent* event) override;
        void OnAirportConfigurationsUpdated(const Airport::AirportConfigurationsUpdatedEvent* event) override;
        void OnAircraftTemporaryAltitudeChanged(const ControllerData::AircraftTemporaryAltitudeChangedEvent* event) override;
        void OnPositionUpdate(const Aircraft::PositionUpdateEvent* event) override;
        void OnFlightplanUpdated(const Flightplan::FlightplanUpdatedEvent* event) override;
        void OnFlightplanRemoved(const Flightplan::FlightplanRemovedEvent* event) override;
        void OnTimer(int Counter);

        // Command handling
        void RegisterCommand();
        void unegisterCommand();

		// API Accessors
        PluginSDK::Logger::LoggerAPI* GetLogger() const { return logger_; }
        Aircraft::AircraftAPI* GetAircraftAPI() const { return aircraftAPI_; }
        Airport::AirportAPI* GetAirportAPI() const { return airportAPI_; }
        Chat::ChatAPI* GetChatAPI() const { return chatAPI_; }
        Flightplan::FlightplanAPI* GetFlightplanAPI() const { return flightplanAPI_; }
        Fsd::FsdAPI* GetFsdAPI() const { return fsdAPI_; }
        PluginSDK::ControllerData::ControllerDataAPI* GetControllerDataAPI() const { return controllerDataAPI_; }
		Tag::TagInterface* GetTagInterface() const { return tagInterface_; }

        // Getters
		bool getPresence() const { return m_presence; }

		// Setters
		void setPresence(bool presence) { m_presence = presence; }

    private:
        void discordSetup();
		void updatePresence();
        void runScopeUpdate();
        void run();

    public:
        // Command IDs
        std::string helpCommandId_;
        std::string versionCommandId_;
        std::string presenceCommandId_;

    private:
        // Plugin state
        bool initialized_ = false;
		bool m_stop;
		bool m_presence = true; // Send presence to Discord
		std::thread m_thread;

		bool isControllerATC_ = false;
        std::string currentController_ = "LFMN APP";
        std::string currentFrequency_ = "134.475";
        
		uint32_t totalTracks_ = 25;
		uint32_t totalFlightplans_ = 8;
		uint32_t aircraftTracked_ = 2;

        // APIs
        PluginMetadata metadata_;
        ClientInformation clientInfo_;
        Aircraft::AircraftAPI* aircraftAPI_ = nullptr;
        Airport::AirportAPI* airportAPI_ = nullptr;
        Chat::ChatAPI* chatAPI_ = nullptr;
        Flightplan::FlightplanAPI* flightplanAPI_ = nullptr;
        Fsd::FsdAPI* fsdAPI_ = nullptr;
        PluginSDK::Logger::LoggerAPI* logger_ = nullptr;
        PluginSDK::ControllerData::ControllerDataAPI* controllerDataAPI_ = nullptr;
        Tag::TagInterface* tagInterface_ = nullptr;
        std::shared_ptr<NeoRPCCommandProvider> CommandProvider_;

    };
} // namespace rpc