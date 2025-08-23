#include "NeoRPC.h"

extern "C" PLUGIN_API PluginSDK::BasePlugin *CreatePluginInstance()
{
    try
    {
        return new rpc::NeoRPC();
    }
    catch (const std::exception &e)
    {
        return nullptr;
    }
}