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
    
    try
    {
        this->RegisterCommand();
        initialized_ = true;
    }
    catch (const std::exception &e)
    {
        logger_->error("Failed to initialize NeoRPC: " + std::string(e.what()));
    }
	m_stop = false;
	m_thread = std::thread(&NeoRPC::run, this);
}

void NeoRPC::Shutdown()
{
    if (initialized_)
    {
        initialized_ = false;
        this->unegisterCommand();
		trackedCallsigns_.clear();
        LOG_DEBUG(Logger::LogLevel::Info, "NeoRPC shutdown complete");
    }
	m_stop = true;
    if (m_thread.joinable())
		m_thread.join();
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
        LOG_DEBUG(Logger::LogLevel::info, "Discord: connected to user " + user.username + "#" + user.discriminator + " - " + user.id);
            })
        .onDisconnected([this](int errcode, std::string_view message) {
        LOG_DEBUG(Logger::LogLevel::info, "Discord: disconnected with error code " + std::to_string(errcode) + " - " + std::string(message));
            })
        .onErrored([this](int errcode, std::string_view message) {
        LOG_DEBUG(Logger::LogLevel::Error, "Discord: error with code " + std::to_string(errcode) + " - " + std::string(message));
            });
}

void rpc::NeoRPC::changeIdlingText()
{
    static int counter;
	counter++;
    static constexpr std::array<std::string_view, 21> idlingTexts = {
        "Waiting for traffic",
        "Monitoring frequencies",
        "Checking FL5000 for conflicts",
        "Watching the skies",
        "Searching for binoculars",
        "Listening to ATC chatter",
        "Scanning for aircraft",
        "Awaiting calls",
        "Tracking airspace",
        "Possible pilot deviation, I have a number...",
        "Clearing ILS 22R",
        "Observing traffic flow",
        "Monitoring silence",
        "Awaiting handoffs",
        "Recording ATIS",
        "Radar scope screensaver",
		"Checking NOTAMs",
		"Deleting SIDs from Flight Plans",
        "Answering radio check",
        "Trying to contact UNICOM",
        "Arguing that France is not on strike"
    };

    idlingText_ = std::string(idlingTexts[counter % idlingTexts.size()]);
}

void rpc::NeoRPC::updatePresence()
{
    auto& rpc = discord::RPCManager::get();
    if (!m_presence) {
        rpc.clearPresence();
        return;
    }

    std::string controller = idlingText_;
	std::string state = "Idling";

    if (isControllerATC_) {
        state = "Aircraft tracked: (" + std::to_string(aircraftTracked_) + " of " + std::to_string(totalAircrafts_) + ")";
        controller = "Controlling " + currentController_ + " " + currentFrequency_;
        rpc.getPresence().setSmallImageKey("radarlogo");
    }
    else if (isObserver_) {
        state = "Aircraft in range: " + std::to_string(totalAircrafts_);
        controller = "Observing as " + currentController_;
    }
    else {
        rpc.getPresence().setSmallImageKey("");
    }

    if (isGolden_ && isOnFire_) {
        rpc.getPresence()
            .setLargeImageKey("both")
            .setLargeImageText(std::to_string(onlineTime_) + " hour streak, On Fire!");
    }
    else if (isGolden_) {
        rpc.getPresence()
            .setLargeImageKey("gold1")
            .setLargeImageText("On a " + std::to_string(onlineTime_) + " hour streak");
    }
    else if (isOnFire_) {
        rpc.getPresence()
            .setLargeImageKey("fire1")
            .setLargeImageText("On Fire! (Tracking " + std::to_string(aircraftTracked_) + " aircrafts)");
	}
    else {
        rpc.getPresence()
            .setLargeImageKey("main")
            .setLargeImageText("French vACC");
    }


    rpc.getPresence()
        .setState(state)
        .setActivityType(discord::ActivityType::Game)
        .setStatusDisplayType(discord::StatusDisplayType::Name)
        .setDetails(controller)
        .setStartTimestamp(StartTime)
        .setSmallImageText("Total Tracks: " + std::to_string(totalTracks_))
        .setInstance(true)
        .refresh();
}

void rpc::NeoRPC::updateData()
{
    isControllerATC_ = false;
	isObserver_ = false;
    currentController_ = "";
    currentFrequency_ = "";

    auto connectionData = fsdAPI_->getConnection();
    if (connectionData) {
        if (connectionData->facility != Fsd::NetworkFacility::OBS) {
            isControllerATC_ = true;
            isObserver_ = false;
        }
        else {
            isControllerATC_ = false;
            isObserver_ = true;
        }

        currentController_ = connectionData->callsign;
        if (connectionData->frequencies.empty()) currentFrequency_ = "";
        else {
			std::string freq = std::to_string(connectionData->frequencies[0]);
			currentFrequency_ = freq.substr(0, freq.length() - 6) + "." + freq.substr(freq.length() - 6, 3);
        }
    }

    totalAircrafts_ = static_cast<uint32_t>(aircraftAPI_->getAll().size());

    aircraftTracked_ = 0;
    std::vector<ControllerData::ControllerDataModel> controllerDatas = controllerDataAPI_->getAll();
    for (const auto& controllerData : controllerDatas) {
        if (controllerData.ownedByMe) {
            ++aircraftTracked_;
            if (trackedCallsigns_.insert(controllerData.callsign).second) {
                ++totalTracks_;
            }
        }
    }

	isGolden_ = (std::time(nullptr) - StartTime > GOLDEN_THRESHOLD); // golden after 1 hour of uptime
	onlineTime_ = static_cast<int>((std::time(nullptr) - StartTime) / 3600); // in hours
    isOnFire_ = (aircraftTracked_ >= ONFIRE_THRESHOLD);
}

void NeoRPC::runUpdate() {
	this->updatePresence();
}

void NeoRPC::OnTimer(int Counter) {
    if (Counter % 5 == 0) // Every 5 seconds
        updateData();
    if (Counter % 15 == 0) // Every 15 seconds
        changeIdlingText();
    this->runUpdate();
}

void NeoRPC::run() {
    int counter = 1;
    discordSetup();
    discord::RPCManager::get().initialize();
    auto& rpc = discord::RPCManager::get();

    while (true) {
        counter += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (true == this->m_stop) {
            discord::RPCManager::get().shutdown();
            return;
        }
        
        this->OnTimer(counter);
    }
    return;
}

PluginSDK::PluginMetadata NeoRPC::GetMetadata() const
{
    return {"NeoRPC", PLUGIN_VERSION, "French VACC"};
}
