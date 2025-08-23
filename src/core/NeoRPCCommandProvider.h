#pragma once
#include "SDK.h"
#include "NeoRPC.h"

using namespace PluginSDK;

namespace rpc {

class NeoRPC;

class NeoRPCCommandProvider : public PluginSDK::Chat::CommandProvider
{
public:
    NeoRPCCommandProvider(rpc::NeoRPC *neoRPC, PluginSDK::Logger::LoggerAPI *logger, Chat::ChatAPI *chatAPI, Fsd::FsdAPI *fsdAPI)
            : neoRPC_(neoRPC), logger_(logger), chatAPI_(chatAPI), fsdAPI_(fsdAPI) {}
		
	Chat::CommandResult Execute(const std::string &commandId, const std::vector<std::string> &args) override;

private:
    Logger::LoggerAPI *logger_ = nullptr;
    Chat::ChatAPI *chatAPI_ = nullptr;
    Fsd::FsdAPI *fsdAPI_ = nullptr;
    NeoRPC *neoRPC_ = nullptr;
};
}  // namespace vsid