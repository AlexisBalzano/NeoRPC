#include <algorithm>
#include <string>

#include "NeoRPC.h"

using namespace rpc;

namespace rpc {
void NeoRPC::RegisterCommand() {
    try
    {
        CommandProvider_ = std::make_shared<NeoRPCCommandProvider>(this, logger_, chatAPI_, fsdAPI_);

        PluginSDK::Chat::CommandDefinition definition;
        definition.name = "rpc version";
        definition.description = "return NeoRPC version";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        versionCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "rpc help";
        definition.description = "display all the available NeoRPC commands";
        definition.lastParameterHasSpaces = false;
        definition.parameters.clear();

        helpCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);

        definition.name = "rpc toggle";
        definition.description = "Toggle Discord Rich Presence";
        definition.lastParameterHasSpaces = false;
        definition.parameters.clear();

        presenceCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
    }
    catch (const std::exception& ex)
    {
        logger_->error("Error registering command: " + std::string(ex.what()));
    }
}

inline void NeoRPC::unegisterCommand()
{
    if (CommandProvider_)
    {
        chatAPI_->unregisterCommand(versionCommandId_);
        chatAPI_->unregisterCommand(helpCommandId_);
        CommandProvider_.reset();
	}
}

Chat::CommandResult NeoRPCCommandProvider::Execute( const std::string &commandId, const std::vector<std::string> &args)
{
    if (commandId == neoRPC_->versionCommandId_)
    {
        std::string message = "Version " + std::string(PLUGIN_VERSION);
        neoRPC_->DisplayMessage(message);
        return { true, std::nullopt };
	}
    else if (commandId == neoRPC_->helpCommandId_)
    {
        neoRPC_->DisplayMessage(".rpc version");
        neoRPC_->DisplayMessage(".rpc toggle");
    }
    else if (commandId == neoRPC_->presenceCommandId_)
    {
		bool currentPresence = neoRPC_->getPresence();
		neoRPC_->setPresence(!currentPresence);
		std::string status = (!currentPresence) ? "enabled" : "disabled";
        neoRPC_->DisplayMessage("Discord Rich Presence " + status);
        return { true, std::nullopt };
	}
    else {
        std::string error = "Invalid command. Use .rpc <command> <param>";
        neoRPC_->DisplayMessage(error);
        return { false, error };
    }

	return { true, std::nullopt };
}
}  // namespace rpc