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
    chatAPI_ = &lcoreAPI->chat();
    fsdAPI_ = &lcoreAPI->fsd();
    controllerDataAPI_ = &lcoreAPI->controllerData();
    logger_ = &lcoreAPI->logger();
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
        LOG_DEBUG(Logger::LogLevel::Info, "Discord: connected to user " + user.username + "#" + user.discriminator + " - " + user.id);
            })
        .onDisconnected([this](int errcode, std::string_view message) {
        LOG_DEBUG(Logger::LogLevel::Info, "Discord: disconnected with error code " + std::to_string(errcode) + " - " + std::string(message));
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
        "Clearing ISS for a low pass",
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

    switch (connectionType_) {
    case State::CONTROLLING:
        controller = "Controlling " + currentController_ + " " + currentFrequency_;
        state = "Aircraft tracked: " + std::to_string(aircraftTracked_) + " of " + std::to_string(totalAircrafts_);
        rpc.getPresence().setSmallImageKey("radarlogo");
        break;
    case State::OBSERVING:
        controller = "Observing as " + currentController_;
        state = "Aircraft in range: " + std::to_string(totalAircrafts_);
        rpc.getPresence().setSmallImageKey("");
        break;
    default:
        rpc.getPresence().setSmallImageKey("");
        break;
    }

    std::string imageKey = "";
	std::string imageText = "";
    
    switch (tier_) {
    case Tier::SILVER:
        imageKey = "silver";
        imageText = "On a " + std::to_string(onlineTime_) + " hour streak";
        break;
    case Tier::GOLD:
        if (imageKey.empty()) imageKey = "gold";
        imageText = "On a " + std::to_string(onlineTime_) + " hour streak";
        break;
    case Tier::NONE:
    default:
        imageKey = "main";
        imageText = "French VACC";
        break;
    }

    if (isOnFire_) {
        imageKey += "fire";
        if (!imageText.empty()) imageText += " ";
        imageText += "On Fire!";
    }

    if (imageKey.empty()) imageKey = "main";
	if (imageText.empty()) imageText = "French VACC";


    rpc.getPresence()
        .setState(state)
		.setLargeImageKey(imageKey)
		.setLargeImageText(imageText)
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
    currentController_ = "";
    currentFrequency_ = "";
	connectionType_ = State::IDLE;

    auto connectionData = fsdAPI_->getConnection();
    if (connectionData) {
        if (connectionData->facility != Fsd::NetworkFacility::OBS) {
			connectionType_ = State::CONTROLLING;
        }
        else {
			connectionType_ = State::OBSERVING;
        }

        currentController_ = connectionData->callsign;
		std::transform(currentController_.begin(), currentController_.end(), currentController_.begin(), ::toupper);
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

	if (std::time(nullptr) - StartTime > 2 * HOUR_THRESHOLD) tier_ = Tier::GOLD;
	else if (std::time(nullptr) - StartTime > HOUR_THRESHOLD) tier_ = Tier::SILVER;
	else tier_ = Tier::NONE;

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
