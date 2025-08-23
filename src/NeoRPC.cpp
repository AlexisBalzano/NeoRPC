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

    DisplayMessage("Version " + std::string(PLUGIN_VERSION) + " loaded.", "Initialisation");
    
    try
    {
        this->RegisterCommand();

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
        LOG_DEBUG(Logger::LogLevel::Info, "NeoRPC shutdown complete");
    }

    this->m_stop = true;
    this->m_worker.join();

    this->unegisterCommand();
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

void NeoRPC::runScopeUpdate() {
	LOG_DEBUG(Logger::LogLevel::Info, "Running scope update.");
}

void NeoRPC::OnTimer(int Counter) {
    if (Counter % 5 == 0) this->runScopeUpdate();
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
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (true == this->m_stop) return;
        
        this->OnTimer(counter);
    }
    return;
}

PluginSDK::PluginMetadata NeoRPC::GetMetadata() const
{
    return {"NeoRPC", PLUGIN_VERSION, "French VACC"};
}