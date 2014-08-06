#pragma once

#include "OBSApi.h"
#include <vector>
#include <fstream>

#define CONFIG_FILENAME TEXT("\\noisereduction.ini")
#define LOG_FILENAME TEXT("\\noisereduction.log")

void nr_log(const char * msg1, const char * msg2);

class NoiseReductionFilter;

class NoiseReductionSettings : public SettingsPane
{

};

class NoiseReduction
{
    public:
        static HINSTANCE hinstDll;

    private:
        static NoiseReduction *     instance;

        AudioSource *               micSource;
        NoiseReductionFilter *      filter;
        NoiseReductionSettings *    settings;
        ConfigFile                  config;

        // configuration
        bool isEnabled;

    public:
        ~NoiseReduction();

        static void             DropInstance();
        static NoiseReduction * GetInstance();

        void OnStartStream();
        void OnStopStream();

        bool IsEnabled() const {
            return isEnabled;
        }

        const AudioSource * GetMicSource() const {
            return micSource;
        }

    private:
        // prevent construction outside of class
        NoiseReduction();
};

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) void OnStartStream();
extern "C" __declspec(dllexport) void OnStopStream();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();
