#pragma once
#include <memory>
#include <thread>
#include <vector>
#include <unordered_set>

#include <discord-rpc.hpp>
#include <NeoRadarSDK/SDK.h>
#include "core/NeoRPCCommandProvider.h"

using namespace PluginSDK;


namespace rpc {
    constexpr auto APPLICATION_ID = "1408567135428673546";
    static int64_t StartTime;
    static bool SendPresence = true;

	constexpr uint32_t ONFIRE_THRESHOLD = 10;
	constexpr uint32_t HOUR_THRESHOLD = 7200; // 2 hour

    class NeoRPCCommandProvider;

    enum State {
        IDLE = 0,
        CONTROLLING,
        OBSERVING
    };

    enum Tier {
        NONE = 0,
        SILVER,
        GOLD
    };

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
        void OnTimer(int Counter);

        // Command handling
        void RegisterCommand();
        void unegisterCommand();

		// API Accessors
        PluginSDK::Logger::LoggerAPI* GetLogger() const { return logger_; }
        Aircraft::AircraftAPI* GetAircraftAPI() const { return aircraftAPI_; }
        Chat::ChatAPI* GetChatAPI() const { return chatAPI_; }
        Fsd::FsdAPI* GetFsdAPI() const { return fsdAPI_; }
        PluginSDK::ControllerData::ControllerDataAPI* GetControllerDataAPI() const { return controllerDataAPI_; }

        // Getters
		bool getPresence() const { return m_presence; }

		// Setters
		void setPresence(bool presence) { m_presence = presence; }

    private:
        void discordSetup();
        void changeIdlingText();
		void updatePresence();
		void updateData();
        void runUpdate();
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

		int connectionType_ = State::IDLE;
		int tier_ = Tier::NONE;
        bool isOnFire_ = false;
        std::string currentController_ = "";
        std::string currentFrequency_ = "";
		std::string idlingText_ = "Watching the skies";
		int onlineTime_ = 0; // in hours
		std::unordered_set<std::string> trackedCallsigns_;
        
		uint32_t totalTracks_ = 0;
		uint32_t totalAircrafts_ = 0;
		uint32_t aircraftTracked_ = 0;

        // APIs
        PluginMetadata metadata_;
        ClientInformation clientInfo_;
        Aircraft::AircraftAPI* aircraftAPI_ = nullptr;
        Chat::ChatAPI* chatAPI_ = nullptr;
        Fsd::FsdAPI* fsdAPI_ = nullptr;
        PluginSDK::Logger::LoggerAPI* logger_ = nullptr;
        PluginSDK::ControllerData::ControllerDataAPI* controllerDataAPI_ = nullptr;
        std::shared_ptr<NeoRPCCommandProvider> CommandProvider_;
    };
} // namespace rpc