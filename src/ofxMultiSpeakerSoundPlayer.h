#pragma once

//using namespace std;

#include "ofConstants.h"
#include "ofMath.h"
#include "ofLog.h"

#include "ofSoundBaseTypes.h"

extern "C" {
#include "fmod.h"
#include "fmod_errors.h"
}


#ifndef OFX_MULTI_SPEAKER_PLAYER 
#define OFX_MULTI_SPEAKER_PLAYER
#endif


class ofxMultiSpeakerSoundPlayer : public ofBaseSoundPlayer {
public:
    
//    enum SpeakerPair {
//        SPEAKERS_DEFAULT=0,
//        SPEAKERS_FRONT,
//        SPEAKERS_BACK,
//        SPEAKERS_SIDE
//    };
    
    struct Driver {
        int index = 0;
        std::string name = "";
        FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
        int speakerModeChannels = 0;
        int systemRate = 0;
    };
    
    // use set fmod settings before initialize fmod is called to configure //
    struct FmodSettings {
        FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_STEREO;
        int driverIndex = 0;
        int numChannels = 64;
        std::string driverName = "";
        unsigned int bufferSize = 1024;
        std::vector<FMOD_SPEAKER> speakers;
        int sampleRate = 44100;
    };
    
    struct Settings {
        bool bLoops = false;
//        SpeakerPair speakerPair = SPEAKERS_DEFAULT;
        bool multiPlay = false;
        float pan = 0.0f;
        std::string filePath = "";
        float volume = 1.0f;
        // array of speakers to pan between
        std::vector<FMOD_SPEAKER> speakers;
    };
    
    static bool setFmodSettings( FmodSettings aFmodSettings );
    static FmodSettings getFmodSettings() { return sFmodSettings; }

	static std::string getSpeakerName(FMOD_SPEAKER aspeaker);
	static FMOD_SPEAKER getSpeakerForName(std::string aname);
	static std::vector<FMOD_SPEAKERMODE> getSpeakerModes();
	static std::string getSpeakerModeName(FMOD_SPEAKERMODE amode);
	static FMOD_SPEAKERMODE getSpeakerModeForName(std::string aname);
    static std::vector<std::string> getSpeakerNameList();

    ofxMultiSpeakerSoundPlayer();
    ~ofxMultiSpeakerSoundPlayer();

    static void updateSound();
    static int getNumberOfDrivers();
    static void printDriverList();
    static std::vector<Driver> getDriverList();
    
    bool load( Settings asettings );
    bool load(const std::filesystem::path& fileName, bool stream = false) override;
    void unload() override;
    void play() override;
//    void playTo(SpeakerPair aSpeakerPair);
    void stop() override;

    void setVolume(float vol) override;
    void setPan(float vol) override;
    void setSpeed(float spd) override;
    void setPaused(bool bP) override;
    void setLoop(bool bLp) override;
    void setMultiPlay(bool bMp) override;
    void setPosition(float pct) override; // 0 = start, 1 = end;
    void setPositionMS(int ms) override;
    void setSpeakers( std::vector<FMOD_SPEAKER> aspeakers );

    float getPosition() const override;
    int getPositionMS() const override;
    bool isPlaying() const override;
    float getSpeed() const override;
    float getPan() const override;
    float getVolume() const override;
    bool isLoaded() const override;

    bool isPanningToAllSpeakers() { return mBPanToAllSpeakers; }
    void setPanToAllSpeakers(bool ab) { mBPanToAllSpeakers = ab; }

    static void initializeFmod();
    static void closeFmod();

protected:
    bool isStreaming = false;
    bool bMultiPlay = false;
    bool bLoop = false;
    bool bLoadedOk = false;
    bool bPaused = false;
    float pan = 0; // -1 to 1
    float volume = 1.0; // 0 - 1
    float internalFreq = 44100; // 44100 ?
    float speed = 1; // -n to n, 1 = normal, -1 backwards
    unsigned int length = 0; // in samples;

    bool mBPanToAllSpeakers = false;

    FMOD_RESULT result;
    FMOD_CHANNEL * channel = nullptr;
    FMOD_SOUND * sound = nullptr;

    std::string currentLoaded = "";
    
    std::vector<FMOD_SPEAKER> mSpeakers;
//    SpeakerPair mSpeakerPair = SPEAKERS_DEFAULT;
    
    static FmodSettings sFmodSettings;
    
};
