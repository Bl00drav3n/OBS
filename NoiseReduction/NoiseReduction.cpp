#include "NoiseReduction.h"
#include "NoiseReductionFilter.h"

// assuming 44100Hz sample-rate
static const int DEFAULT_BUFFER_SIZE = 1 << 16;

NoiseReduction* NoiseReduction::instance = NULL;
HINSTANCE NoiseReduction::hinstDll = NULL;

#define g_pInstance NoiseReduction::GetInstance()

std::ofstream log_out;

void nr_log(const char * msg1, const char * msg2)
{
    log_out << msg1 << ": " << msg2 << std::endl;
}

NoiseReduction::NoiseReduction()
: isEnabled(false)
, micSource(NULL)
, filter(NULL)
, settings(NULL)
, config()
{
}

NoiseReduction::~NoiseReduction()
{
    if (micSource)
    {
        micSource->RemoveAudioFilter(filter);
    }

    if (filter)
    {
        delete filter;
    }

    if (log_out.is_open())
        log_out.close();
}

NoiseReduction* NoiseReduction::GetInstance()
{
    if (!instance)
        instance = new NoiseReduction();

    return instance;
}

void NoiseReduction::DropInstance()
{
    if (instance)
    {
        delete instance;
        instance = NULL;
    }
}

void NoiseReduction::OnStartStream()
{
    isEnabled = true;
    //if (!isEnabled)
        //return;

    log_out.open(OBSGetPluginDataPath() + LOG_FILENAME);

    nr_log("OnStartStream", "Initializing");

    micSource = OBSGetMicAudioSource();
    if (micSource == NULL)
        return; // No microphone

    filter = new NoiseReductionFilter(this);
    if(filter->Initialize(DEFAULT_BUFFER_SIZE))
        micSource->AddAudioFilter(filter);
}

void NoiseReduction::OnStopStream()
{
    isEnabled = false;

    if (filter) {
        if (micSource)
            micSource->RemoveAudioFilter(filter);
        delete filter;
        filter = NULL;
    }
    micSource = NULL;

    nr_log("OnStopStream", "Freeing data");

    log_out.close();
}

bool LoadPlugin()
{
    return NoiseReduction::GetInstance() != NULL;
}

void UnloadPlugin()
{
    NoiseReduction::DropInstance();
}

void OnStartStream()
{
    if (!g_pInstance)
        return;

    g_pInstance->OnStartStream();
}

void OnStopStream()
{
    if (!g_pInstance)
        return;

    g_pInstance->OnStopStream();
}

CTSTR GetPluginName()
{
    return Str("Plugins.NoiseReduction.PluginName");
}

CTSTR GetPluginDescription()
{
    return Str("Plugins.NoiseReduction.PluginDescription");
}


BOOL CALLBACK DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        NoiseReduction::hinstDll = hinstDLL;
    }
    else if (fdwReason == DLL_PROCESS_ATTACH)
    {
        NoiseReduction::DropInstance();
        log_out.close();
    }

    return TRUE;
}
