#include "NeoRPC.h"
#include <numeric>
#include <chrono>

#include "Version.h"
#include "core/CompileCommands.h"

#ifdef DEV
#define LOG_DEBUG(loglevel, message) logger_->log(loglevel, message)
#else
#define LOG_DEBUG(loglevel, message) void(0)
#endif

using namespace rpc;

NeoRPC::NeoRPC() : m_stop(false), controllerDataAPI_(nullptr) {};
NeoRPC::~NeoRPC() = default;

void NeoRPC::Initialize(const PluginMetadata &metadata, CoreAPI *coreAPI, ClientInformation info)
{
    metadata_ = metadata;
    clientInfo_ = info;
    CoreAPI *lcoreAPI = coreAPI;
    aircraftAPI_ = &lcoreAPI->aircraft();
    airportAPI_ = &lcoreAPI->airport();
    chatAPI_ = &lcoreAPI->chat();
    flightplanAPI_ = &lcoreAPI->flightplan();
    fsdAPI_ = &lcoreAPI->fsd();
    controllerDataAPI_ = &lcoreAPI->controllerData();
    logger_ = &lcoreAPI->logger();
    tagInterface_ = lcoreAPI->tag().getInterface();
	StartTime = time(nullptr);

    DisplayMessage("Version " + std::string(PLUGIN_VERSION) + " loaded.", "Initialisation");
    
    try
    {
        this->RegisterCommand();
        discordSetup();
        discord::RPCManager::get().initialize();

        initialized_ = true;
    }
    catch (const std::exception &e)
    {
        logger_->error("Failed to initialize NeoRPC: " + std::string(e.what()));
    }

    this->m_stop = false;
    this->m_worker = std::thread(&NeoRPC::run, this);
}

void NeoRPC::Shutdown()
{
    if (initialized_)
    {
        initialized_ = false;
		discord::RPCManager::get().shutdown();
        this->unegisterCommand();
        LOG_DEBUG(Logger::LogLevel::Info, "NeoRPC shutdown complete");
    }

    this->m_stop = true;
	if (m_worker.joinable()) this->m_worker.join();
}

void rpc::NeoRPC::Reset()
{
}

void NeoRPC::DisplayMessage(const std::string &message, const std::string &sender) {
    Chat::ClientTextMessageEvent textMessage;
    textMessage.sentFrom = "NeoRPC";
    (sender.empty()) ? textMessage.message = ": " + message : textMessage.message = sender + ": " + message;
    textMessage.useDedicatedChannel = true;

    chatAPI_->sendClientMessage(textMessage);
}

void rpc::NeoRPC::discordSetup()
{
    discord::RPCManager::get()
        .setClientID(APPLICATION_ID)
        .onReady([this](discord::User const& user) {
            DisplayMessage("Discord: connected to user " + user.username + "#" + user.discriminator + " - " + user.id);
        })
        .onDisconnected([this](int errcode, std::string_view message) {
            DisplayMessage("Discord: disconnected with error code " + std::to_string(errcode) + " - " + std::string(message));
        })
        .onErrored([this](int errcode, std::string_view message) {
            DisplayMessage("Discord: error with code " + std::to_string(errcode) + " - " + std::string(message));
        })
        .onJoinGame([this](std::string_view joinSecret) {
            DisplayMessage("Discord: join game - " + std::string(joinSecret));
        })
        .onSpectateGame([this](std::string_view spectateSecret) {
            DisplayMessage("Discord: spectate game - " + std::string(spectateSecret));
        })
        .onJoinRequest([this](discord::User const& user) {
            DisplayMessage("Discord: join request from " + user.username + "#" + user.discriminator + " - " + user.id);
        });
}

void rpc::NeoRPC::updatePresence()
{
    auto& rpc = discord::RPCManager::get();
    if (!m_presence) {
        rpc.clearPresence();
        return;
    }

    rpc.getPresence()
        .setState("Idling")
        .setActivityType(discord::ActivityType::Game)
        .setStatusDisplayType(discord::StatusDisplayType::State)
        .setDetails("Testing")
        .setStartTimestamp(StartTime)
        .setEndTimestamp(time(nullptr) + 5 * 60)
        .setLargeImageKey("logo")
        .setSmallImageKey("radarlogo")
        .setPartyID("party1234")
        .setPartySize(1)
        .setPartyMax(6)
        .setPartyPrivacy(discord::PartyPrivacy::Public)
        .setButton1("Click me!", "https://google.com/")
        .setButton2("Dont click me!", "https://www.youtube.com/watch?v=dQw4w9WgXcQ")
        .setInstance(false)
        .refresh();
}

void NeoRPC::runScopeUpdate() {
	LOG_DEBUG(Logger::LogLevel::Info, "Running scope update.");
	this->updatePresence();
}

void NeoRPC::OnTimer(int Counter) {
    this->runScopeUpdate();
}

void rpc::NeoRPC::OnControllerDataUpdated(const ControllerData::ControllerDataUpdatedEvent* event)
{   
    if (!event || event->callsign.empty())
        return;
    std::optional<ControllerData::ControllerDataModel> controllerDataBlock = controllerDataAPI_->getByCallsign(event->callsign);
    if (!controllerDataBlock.has_value()) return;
    if (controllerDataBlock->groundStatus == ControllerData::GroundStatus::Dep) {
        return;
    }
    else {

    }
}

void NeoRPC::OnAirportConfigurationsUpdated(const Airport::AirportConfigurationsUpdatedEvent* event) {
	LOG_DEBUG(Logger::LogLevel::Info, "Airport configurations updated.");
}

void rpc::NeoRPC::OnAircraftTemporaryAltitudeChanged(const ControllerData::AircraftTemporaryAltitudeChangedEvent* event)
{
    if (!event || event->callsign.empty())
        return;

	LOG_DEBUG(Logger::LogLevel::Info, "Temporary altitude changed for callsign: " + event->callsign);

	std::optional<double> distanceFromOrigin = aircraftAPI_->getDistanceFromOrigin(event->callsign);
	if (!distanceFromOrigin.has_value()) {
		logger_->error("Failed to retrieve distance from origin for callsign: " + event->callsign);
		return;
	}
}

void rpc::NeoRPC::OnPositionUpdate(const Aircraft::PositionUpdateEvent* event)
{
    for (const auto& aircraft : event->aircrafts) {
        if (aircraft.callsign.empty())
            continue;
	}
}

void rpc::NeoRPC::OnFlightplanUpdated(const Flightplan::FlightplanUpdatedEvent* event)
{
    if (!event || event->callsign.empty())
        return;

	LOG_DEBUG(Logger::LogLevel::Info, "Flightplan updated for callsign: " + event->callsign);

	std::optional<double> distanceFromOrigin = aircraftAPI_->getDistanceFromOrigin(event->callsign);
	if (!distanceFromOrigin.has_value()) {
		logger_->error("Failed to retrieve distance from origin for callsign: " + event->callsign);
		return;
	}
}


void rpc::NeoRPC::OnFlightplanRemoved(const Flightplan::FlightplanRemovedEvent* event)
{
    if (!event || event->callsign.empty())
        return;
}

void NeoRPC::run() {
    int counter = 1;
    while (true) {
        counter += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (true == this->m_stop) return;
        
        this->OnTimer(counter);
    }
    return;
}

PluginSDK::PluginMetadata NeoRPC::GetMetadata() const
{
    return {"NeoRPC", PLUGIN_VERSION, "French VACC"};
}