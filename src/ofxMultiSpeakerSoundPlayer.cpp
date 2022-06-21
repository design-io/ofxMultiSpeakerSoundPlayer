#include "ofxMultiSpeakerSoundPlayer.h"
#include "ofUtils.h"

using namespace std;

bool bFmodSysInited = false;
bool bFmodInitialized_ = false;
//bool bUseSpectrum_ = false;
//float fftValues_[8192];			//
float fftInterpValues_[8192];			//
float fftSpectrum_[8192];		// maximum #ofxMultiSpeakerSoundPlayer is 8192, in fmodex....
static unsigned int buffersize = 1024;
static FMOD_DSP* fftDSP = NULL;

// ---------------------  static vars
static FMOD_CHANNELGROUP * channelgroup = nullptr;
static FMOD_SYSTEM       * sys = nullptr;

ofxMultiSpeakerSoundPlayer::FmodSettings ofxMultiSpeakerSoundPlayer::sFmodSettings;

// these are global functions, that affect every sound / channel:
// ------------------------------------------------------------
// ------------------------------------------------------------

//--------------------
void fmodStopAll() {
    ofxMultiSpeakerSoundPlayer::initializeFmod();
    FMOD_ChannelGroup_Stop(channelgroup);
}

//--------------------
void fmodSetVolume(float vol) {
    ofxMultiSpeakerSoundPlayer::initializeFmod();
    FMOD_ChannelGroup_SetVolume(channelgroup, vol);
}

//--------------------
void fmodSoundUpdate() {
    if (bFmodInitialized_) {
        FMOD_System_Update(sys);
    }
}

//--------------------
float * fmodSoundGetSpectrum(int nBands) {

    ofxMultiSpeakerSoundPlayer::initializeFmod();

    // 	set to 0
    for (int i = 0; i < 8192; i++){
        fftInterpValues_[i] = 0;
        fftSpectrum_[i] = 0;
    }

    // 	check what the user wants vs. what we can do:
    if (nBands > 8192){
        ofLogWarning("ofxMultiSpeakerSoundPlayer") << "fmodSoundGetSpectrum(): requested number of bands " << nBands << ", using maximum of 8192";
        nBands = 8192;
    } else if (nBands <= 0){
        ofLogWarning("ofxMultiSpeakerSoundPlayer") << "fmodSoundGetSpectrum(): requested number of bands " << nBands << ", using minimum of 1";
        nBands = 1;
        return fftInterpValues_;
    }

    //  get the fft
    //  useful info here: https://www.parallelcube.com/2018/03/10/frequency-spectrum-using-fmod-and-ue4/
    if( fftDSP == NULL ){
        FMOD_System_CreateDSPByType(sys, FMOD_DSP_TYPE_FFT,&fftDSP);
        FMOD_ChannelGroup_AddDSP(channelgroup,0,fftDSP);
        FMOD_DSP_SetParameterInt(fftDSP, FMOD_DSP_FFT_WINDOWTYPE, FMOD_DSP_FFT_WINDOW_HANNING);
    }

    if( fftDSP != NULL ){
        FMOD_DSP_PARAMETER_FFT *fft;
        auto result = FMOD_DSP_GetParameterData(fftDSP, FMOD_DSP_FFT_SPECTRUMDATA, (void **)&fft, 0, 0, 0);
        if( result == 0 ){

            // Only read / display half of the buffer typically for analysis
            // as the 2nd half is usually the same data reversed due to the nature of the way FFT works. ( comment from link above )
            int length = fft->length/2;
            if( length > 0 ){

                std::vector <float> avgValCount;
                avgValCount.assign(nBands, 0.0); 

                float normalizedBand = 0;
                float normStep = 1.0 / (float)length;

                for (int bin = 0; bin < length; bin++){
                    //should map 0 to nBands but accounting for lower frequency bands being more important
                    int logIndexBand = log10(1.0 + normalizedBand*9.0) * nBands;

                    //get both channels as that is what the old FMOD call did
                    for (int channel = 0; channel < fft->numchannels; channel++){
                        fftSpectrum_[logIndexBand] += fft->spectrum[channel][bin];
                        avgValCount[logIndexBand] += 1.0;
                    }

                    normalizedBand += normStep;
                }

                //average the remapped bands based on how many times we added to each bin
                for(int i = 0; i < nBands; i++){
                    if( avgValCount[i] > 1.0 ){
                        fftSpectrum_[i] /= avgValCount[i];
                    }
                }
            }
        }
    }

    // 	convert to db scale
    for(int i = 0; i < nBands; i++){
        fftInterpValues_[i] = 10.0f * (float)log10(1 + fftSpectrum_[i]) * 2.0f;
    }

    return fftInterpValues_;
}

//--------------------
bool ofxMultiSpeakerSoundPlayer::setFmodSettings( FmodSettings aFmodSettings ) {
    if( bFmodInitialized_ ) {
        ofLogWarning("ofxMultiSpeakerSoundPlayer :: fmod has already been inited, try closing fmod and then calling this function");
        return false;
    }
    sFmodSettings = aFmodSettings;
    return true;
}

//--------------------
std::string ofxMultiSpeakerSoundPlayer::getSpeakerName(FMOD_SPEAKER aspeaker) {

	if (aspeaker == FMOD_SPEAKER_FRONT_LEFT) {
		return "FrontLeft";
	} else if (aspeaker == FMOD_SPEAKER_FRONT_RIGHT) {
		return "FrontRight";
	} else if (aspeaker == FMOD_SPEAKER_FRONT_CENTER) {
		return "FrontCenter";
    } else if (aspeaker == FMOD_SPEAKER_LOW_FREQUENCY) {
        return "LowFrequency";
    } else if( aspeaker == FMOD_SPEAKER_SURROUND_LEFT ) {
        return "SpeakerSurroundLeft";
    } else if (aspeaker == FMOD_SPEAKER_SURROUND_RIGHT) {
        return "SpeakerSurroundRight";
    } else if (aspeaker == FMOD_SPEAKER_BACK_LEFT) {
		return "BackLeft";
	} else if (aspeaker == FMOD_SPEAKER_BACK_RIGHT) {
		return "BackRight";
	} else if (aspeaker == FMOD_SPEAKER_TOP_FRONT_LEFT) {
		return "TopFrontLeft";
	} else if (aspeaker == FMOD_SPEAKER_TOP_FRONT_RIGHT) {
		return "TopFrontRight";
    } else if (aspeaker == FMOD_SPEAKER_TOP_BACK_LEFT) {
        return "TopBackLeft";
    } else if (aspeaker == FMOD_SPEAKER_TOP_BACK_RIGHT) {
        return "TopBackRight";
    }
	ofLogError("ofxMultiSpeakerSoundPlayer::getSpeakerName : unable to find name for speaker: ") << aspeaker;
	return "Unknown";
}

//--------------------
FMOD_SPEAKER ofxMultiSpeakerSoundPlayer::getSpeakerForName(std::string aname) {
	if (aname == "FrontLeft") {
		return FMOD_SPEAKER_FRONT_LEFT;
	} else if (aname == "FrontRight") {
		return FMOD_SPEAKER_FRONT_RIGHT;
	} else if (aname == "FrontCenter") {
		return FMOD_SPEAKER_FRONT_CENTER;
    } else if (aname == "LowFrequency") {
        return FMOD_SPEAKER_LOW_FREQUENCY;
    } else if (aname == "SpeakerSurroundLeft") {
        return FMOD_SPEAKER_SURROUND_LEFT;
    } else if( aname == "SpeakerSurroundRight" ) {
        return FMOD_SPEAKER_SURROUND_RIGHT;
	} else if (aname == "BackLeft") {
		return FMOD_SPEAKER_BACK_LEFT;
	} else if (aname == "BackRight") {
		return FMOD_SPEAKER_BACK_RIGHT;
	} else if (aname == "TopFrontLeft") {
		return FMOD_SPEAKER_TOP_FRONT_LEFT;
	} else if (aname == "TopFrontRight") {
		return FMOD_SPEAKER_TOP_FRONT_RIGHT;
    } else if (aname == "TopBackLeft") {
        return FMOD_SPEAKER_TOP_BACK_LEFT;
    } else if (aname == "TopBackRight") {
        return FMOD_SPEAKER_TOP_BACK_RIGHT;
    }
	ofLogError("ofxMultiSpeakerSoundPlayer::getSpeakerForName : unable to find speaker for name: ") << aname;
	return FMOD_SPEAKER_FRONT_LEFT;
}

//--------------------
std::vector<FMOD_SPEAKERMODE> ofxMultiSpeakerSoundPlayer::getSpeakerModes() {
	std::vector<FMOD_SPEAKERMODE> speaks = {
		FMOD_SPEAKERMODE_MONO,
		FMOD_SPEAKERMODE_STEREO,
		FMOD_SPEAKERMODE_SURROUND,
		FMOD_SPEAKERMODE_5POINT1,
		FMOD_SPEAKERMODE_7POINT1
	};
	return speaks;
}

//--------------------
std::string ofxMultiSpeakerSoundPlayer::getSpeakerModeName(FMOD_SPEAKERMODE amode) {
	if (amode == FMOD_SPEAKERMODE_MONO) {
		return "Mono";
	} else if (amode == FMOD_SPEAKERMODE_STEREO) {
		return "Stereo";
	} else if (amode == FMOD_SPEAKERMODE_SURROUND) {
		return "Surround";
	} else if (amode == FMOD_SPEAKERMODE_5POINT1) {
		return "FivePoint1";
	} else if (amode == FMOD_SPEAKERMODE_7POINT1) {
		return "SevenPoint1";
	}
	ofLogError("ofxMultiSpeakerSoundPlayer :: getSpeakerModeName : unable to get name for mode: ") << amode;
	return "Stereo";
}

//--------------------
FMOD_SPEAKERMODE ofxMultiSpeakerSoundPlayer::getSpeakerModeForName(std::string aname) {
	if (aname == "Mono") {
		return FMOD_SPEAKERMODE_MONO;
	} else if (aname == "Stereo") {
		return FMOD_SPEAKERMODE_STEREO;
	} else if (aname == "Surround") {
		return FMOD_SPEAKERMODE_SURROUND;
	} else if (aname == "FivePoint1") {
		return FMOD_SPEAKERMODE_5POINT1;
	} else if (aname == "SevenPoint1") {
		return FMOD_SPEAKERMODE_7POINT1;
	}
	ofLogError("ofxMultiSpeakerSoundPlayer::getSpeakerModeForName : could not find mode for name: ") << aname;
	return FMOD_SPEAKERMODE_STEREO;
}

//--------------------------------------------------
std::vector<std::string> ofxMultiSpeakerSoundPlayer::getSpeakerNameList() {
    vector<std::string> tSpeakerNames;
    for (int i = 0; i < FMOD_SPEAKER_MAX; i++) {
        tSpeakerNames.push_back( ofxMultiSpeakerSoundPlayer::getSpeakerName((FMOD_SPEAKER)i ));
    }
    return tSpeakerNames;
}

// call this every frame //
//--------------------
void ofxMultiSpeakerSoundPlayer::updateSound() {
	fmodSoundUpdate();
}

//--------------------
int ofxMultiSpeakerSoundPlayer::getNumberOfDrivers() {
	if( !bFmodSysInited ) {
        bFmodSysInited=true;
        FMOD_System_Create(&sys);
    }
	int numDrivers;
	FMOD_System_GetNumDrivers(sys, &numDrivers);
	return numDrivers;
}

//--------------------
void ofxMultiSpeakerSoundPlayer::printDriverList() {
    auto ds = getDriverList();
    cout << " -- ofxMultiSpeakerSoundPlayer :: Driver List -- " << endl;
    for( int i = 0; i < ds.size(); i++ ) {
        cout << ds[i].index << " - " << ds[i].name << endl;
    }
}

//--------------------
vector<ofxMultiSpeakerSoundPlayer::Driver> ofxMultiSpeakerSoundPlayer::getDriverList() {
    int numDrivers = getNumberOfDrivers();
    vector<ofxMultiSpeakerSoundPlayer::Driver> rdrivers;
    for (int i = 0; i < numDrivers; i++) {
        char* name = new char[64];
        // FMOD_RESULT F_API FMOD_System_GetDriverInfo (FMOD_SYSTEM *system, int id, char *name, int namelen, FMOD_GUID *guid);
        // FMOD_System_GetDriverInfo(sys, i, name, 64, NULL );
        // FMOD_RESULT F_API FMOD_System_GetDriverInfo             (FMOD_SYSTEM *system, int id, char *name, int namelen, FMOD_GUID *guid, int *systemrate, FMOD_SPEAKERMODE *speakermode, int *speakermodechannels);
        //int systemRate = 0;
        //FMOD_SPEAKERMODE smode;
        //int speakerModeChannels = 0;

        ofxMultiSpeakerSoundPlayer::Driver tdriver;
        //tdriver.name = sname;
        tdriver.index = i;

        FMOD_System_GetDriverInfo(
            sys, 
            i, 
            name, 
            64, 
            NULL,
            &tdriver.systemRate,
            &tdriver.speakerMode,
            &tdriver.speakerModeChannels
        );

        string sname(name);
        //ofxMultiSpeakerSoundPlayer::Driver tdriver;
        tdriver.name = sname;
        //tdriver.index = i;
       // tdriver.speakerMode = smode;
        rdrivers.push_back( tdriver );
//        cout << i << " - driver " << tdriver.name << endl;
        delete[] name;
    }
    return rdrivers;
}

// ------------------------------------------------------------
// ------------------------------------------------------------


// now, the individual sound player:
//------------------------------------------------------------
ofxMultiSpeakerSoundPlayer::ofxMultiSpeakerSoundPlayer() {
    bLoop 			= false;
    bLoadedOk 		= false;
    pan 			= 0.0f; // range for oF is -1 to 1
    volume 			= 1.0f;
    internalFreq 	= 44100;
    speed 			= 1;
    bPaused 		= false;
    isStreaming		= false;
}

//---------------------------------------
ofxMultiSpeakerSoundPlayer::~ofxMultiSpeakerSoundPlayer() {
    unload();
}

//---------------------------------------
// this should only be called once
void ofxMultiSpeakerSoundPlayer::initializeFmod() {

    if(!bFmodInitialized_) {
        
        int numDrivers = getNumberOfDrivers();
        // try to find the driver based on the name //
        if( sFmodSettings.driverName != "" ) {
            auto drivers = getDriverList();
            string sdname = ofToLower(sFmodSettings.driverName);
            for( auto driver : drivers ) {
                if( ofIsStringInString( ofToLower(driver.name), sFmodSettings.driverName)) {
                    ofLogNotice("ofxMultiSpeakerSoundPlayer :: found driver containing name: ") << driver.index << " - " << sFmodSettings.driverName << " driver: " << driver.name;
                    sFmodSettings.driverIndex = driver.index;
                    break;
                }
            }
		} else {
			if (sFmodSettings.driverIndex < numDrivers && sFmodSettings.driverIndex > -1) {
				sFmodSettings.driverName = getDriverList()[sFmodSettings.driverIndex].name;
			}
		}
        
        if( sFmodSettings.driverIndex >= numDrivers || sFmodSettings.driverIndex < 0 ) {
             ofLogError("ofxMultiSpeakerSoundPlayer :: initializeFmod device: ") << sFmodSettings.driverIndex << " out of range! number of drivers: " << numDrivers << " | " << ofGetFrameNum();
            ofLogWarning("ofxMultiSpeakerSoundPlayer :: setting driver index to 0 ");
            sFmodSettings.driverIndex = 0;
        }
        
        ofLogNotice("ofxMultiSpeakerSoundPlayer :: initializeFmod with device: ") << sFmodSettings.driverIndex << " | " << ofGetFrameNum();
        
        FMOD_System_SetDriver(sys, sFmodSettings.driverIndex);
        //FMOD_System_SetSpeakerMode(sys, FMOD_SPEAKERMODE_7POINT1);
//		FMOD_System_SetSpeakerMode(sys, FMOD_SPEAKERMODE_5POINT1);
        //FMOD_RESULT FMOD_System_SetSpeakerMode(
        //    FMOD_SYSTEM *  system,
        //    FMOD_SPEAKERMODE  speakermode
        //);
        //FMOD_System_SetSpeakerMode(sys, sFmodSettings.speakerMode);
        /*FMOD_RESULT F_API FMOD_System_SetAdvancedSettings       (FMOD_SYSTEM *system, FMOD_ADVANCEDSETTINGS *settings);*/
        //FMOD_ADVANCEDSETTINGS fmodApiSettings;
        //fmodAP
        //FMOD_RESULT FMOD_System_SetSoftwareFormat(
        //    FMOD_SYSTEM *system,
        //    int samplerate,
        //    FMOD_SPEAKERMODE speakermode,
        //    int numrawspeakers
        //);
        auto setSoftRes = FMOD_System_SetSoftwareFormat(
            sys,
            sFmodSettings.sampleRate,
            sFmodSettings.speakerMode,
            0
        );
        
        // set buffersize, keep number of buffers
        unsigned int bsTmp;
        int nbTmp;
        FMOD_System_GetDSPBufferSize(sys, &bsTmp, &nbTmp);
        FMOD_System_SetDSPBufferSize(sys, sFmodSettings.bufferSize, nbTmp);

#ifdef TARGET_LINUX
        FMOD_System_SetOutput(sys,FMOD_OUTPUTTYPE_ALSA);
#endif

        FMOD_System_Init(sys, sFmodSettings.numChannels, FMOD_INIT_NORMAL, NULL);  
        FMOD_System_GetMasterChannelGroup(sys, &channelgroup);
        bFmodInitialized_ = true;
    }
}

// should probably call this on exit()
//---------------------------------------
void ofxMultiSpeakerSoundPlayer::closeFmod() {
    if(bFmodInitialized_) {
        FMOD_System_Close(sys);
        bFmodInitialized_ = false;
    }
}

//struct Settings {
//    bool bLoops = false;
//    SpeakerPair speakerPair = SPEAKERS_DEFAULT;
//    bool multiPlay = false;
//    float pan = 0.0f;
//    std::string filePath = "";
//    float volume = 1.0f;
//};

//------------------------------------------------------------
bool ofxMultiSpeakerSoundPlayer::load( Settings asettings ) {
    if( asettings.filePath == "" ) return false;
    bool bok = load( asettings.filePath, false );
    if( bok ) {
        setLoop( asettings.bLoops );
        setMultiPlay( asettings.multiPlay );
        setVolume( asettings.volume );
        setSpeakers( asettings.speakers );
        setPan( asettings.pan );
    }
    return bLoadedOk;
}

//------------------------------------------------------------
bool ofxMultiSpeakerSoundPlayer::load(const std::filesystem::path& fileName, bool stream) {
    string fileNameStr;
	currentLoaded = fileName.string();

    fileNameStr = ofToDataPath(fileName.string());

    // fmod uses IO posix internally, might have trouble
    // with unicode paths...
    // says this code:
    // http://66.102.9.104/search?q=cache:LM47mq8hytwJ:www.cleeker.com/doxygen/audioengine__fmod_8cpp-source.html+FSOUND_Sample_Load+cpp&hl=en&ct=clnk&cd=18&client=firefox-a
    // for now we use FMODs way, but we could switch if
    // there are problems:

    bMultiPlay = false;

    // [1] init fmod, if necessary

    initializeFmod();

    // [2] try to unload any previously loaded sounds
    // & prevent user-created memory leaks
    // if they call "loadSound" repeatedly, for example

    unload();

    // [3] load sound

    //choose if we want streaming
    int fmodFlags =  FMOD_DEFAULT;
    if(stream)fmodFlags =  FMOD_DEFAULT | FMOD_CREATESTREAM;

    result = FMOD_System_CreateSound(sys, fileNameStr.data(), fmodFlags, NULL, &sound);

    if (result != FMOD_OK) {
        bLoadedOk = false;
        ofLogError("ofxMultiSpeakerSoundPlayer") << "loadSound(): could not load \"" << fileNameStr << "\"";
    } else {
        bLoadedOk = true;
        FMOD_Sound_GetLength(sound, &length, FMOD_TIMEUNIT_PCM);
        isStreaming = stream;
        
        if( sFmodSettings.speakers.size() > 0 ) {
            setSpeakers(sFmodSettings.speakers);
        }
        
    }

    return bLoadedOk;
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::unload() {
    if (bLoadedOk) {
        stop();						// try to stop the sound
        if(!isStreaming) FMOD_Sound_Release(sound);
        bLoadedOk = false;
    }
}

//------------------------------------------------------------
bool ofxMultiSpeakerSoundPlayer::isPlaying() const {
    if (!bLoadedOk) return false;
    if(channel == NULL) return false;
    int playing = 0;
    FMOD_Channel_IsPlaying(channel, &playing);
    return (playing != 0 ? true : false);
}

//------------------------------------------------------------
float ofxMultiSpeakerSoundPlayer::getSpeed() const {
    return speed;
}

//------------------------------------------------------------
float ofxMultiSpeakerSoundPlayer::getPan() const {
    return pan;
}

//------------------------------------------------------------
float ofxMultiSpeakerSoundPlayer::getVolume() const
{
    return volume;
}

//------------------------------------------------------------
bool ofxMultiSpeakerSoundPlayer::isLoaded() const {
    return bLoadedOk;
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setVolume(float vol) {
    if (isPlaying() == true) {
        FMOD_Channel_SetVolume(channel, vol);
    }
    volume = vol;
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setPosition(float pct) {
    if (isPlaying() == true) {
        int sampleToBeAt = (int)(length * pct);
        FMOD_Channel_SetPosition(channel, sampleToBeAt, FMOD_TIMEUNIT_PCM);
    }
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setPositionMS(int ms) {
    if (isPlaying() == true) {
        FMOD_Channel_SetPosition(channel, ms, FMOD_TIMEUNIT_MS);
    }
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setSpeakers( std::vector<FMOD_SPEAKER> aspeakers ) {
    mSpeakers = aspeakers;
}

//------------------------------------------------------------
float ofxMultiSpeakerSoundPlayer::getPosition() const {
    if (isPlaying() == true) {
        unsigned int sampleImAt;

        FMOD_Channel_GetPosition(channel, &sampleImAt, FMOD_TIMEUNIT_PCM);

        float pct = 0.0f;
        if (length > 0) {
            pct = sampleImAt / (float)length;
        }
        return pct;
    } 
    return 0.f;
}

//------------------------------------------------------------
int ofxMultiSpeakerSoundPlayer::getPositionMS() const {
    if (isPlaying() == true) {
        unsigned int sampleImAt;

        FMOD_Channel_GetPosition(channel, &sampleImAt, FMOD_TIMEUNIT_MS);

        return sampleImAt;
    } else {
        return 0;
    }
}

//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setPan(float p) {

    pan = p;
    p = ofClamp(p, -1, 1);

    if (channel == NULL) {
        ofLogWarning("ofxMultiSpeakerSoundPlayer :: setPan : channel is NULL");
        return;
    }

    if (isPlaying() == true) {
        if( mSpeakers.size() < 3 || sFmodSettings.speakerMode == FMOD_SPEAKERMODE_MONO || sFmodSettings.speakerMode == FMOD_SPEAKERMODE_STEREO ) {
            FMOD_RESULT result = FMOD_Channel_SetPan(channel,p);
            if (result != FMOD_OK) {
                ofLogError("ofxMultiSpeakerSoundPlayer :: setPan : FMOD_Channel_SetPan - ERROR");
            }
        } else {
            // linear map to play
//            FMOD_Channel_SetSpeakerMix(channel, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            /*
            FMOD_RESULT Channel::setSpeakerMix(
              float  frontleft,
              float  frontright,
              float  center,
              float  lfe,
              float  backleft,
              float  backright,
              float  sideleft,
              float  sideright
            );
            */

            //ChannelControl::setMixLevelsOutput
            //    Sets the speaker volume levels for each speaker individually, this is a helper to avoid calling ChannelControl::setMixMatrix.

            //FMOD_RESULT F_API FMOD_Channel_SetMixLevelsOutput (
            //    FMOD_CHANNEL *channel, 
            //    float frontleft, 
            //    float frontright, 
            //    float center, 
            //    float lfe, --> Volume level for the subwoofer speaker.
            //    float surroundleft, 
            //    float surroundright, 
            //    float backleft, 
            //    float backright
            //);

            vector<float> tvols;
            for( int i = 0; i < FMOD_SPEAKER_MAX; i++ ) {
                tvols.push_back(0.0f);
            }
            if (isPanningToAllSpeakers()) {
                if (mSpeakers.size() > 0) {
                    p = 1.f / (float)mSpeakers.size();
                    for (int i = 0; i < mSpeakers.size(); i++) {
                        tvols[mSpeakers[i]] = p;
                    }
                }
            } else {
                p = ofMap(p, -1, 1, 0, 1, true);
                for (int i = 0; i < mSpeakers.size(); i++) {
                    float spct = (float)i / ((float)mSpeakers.size() - 1.f);
                    float sdiff = 1.0f - fabs(spct - p);
                    tvols[mSpeakers[i]] = sdiff;
                }
            }

            //FMOD_SPEAKER_NONE = -1,
            //    FMOD_SPEAKER_FRONT_LEFT = 0,
            //    FMOD_SPEAKER_FRONT_RIGHT,
            //    FMOD_SPEAKER_FRONT_CENTER,
            //    FMOD_SPEAKER_LOW_FREQUENCY,
            //    FMOD_SPEAKER_SURROUND_LEFT,
            //    FMOD_SPEAKER_SURROUND_RIGHT,
            //    FMOD_SPEAKER_BACK_LEFT,
            //    FMOD_SPEAKER_BACK_RIGHT,
            //    FMOD_SPEAKER_TOP_FRONT_LEFT,
            //    FMOD_SPEAKER_TOP_FRONT_RIGHT,
            //    FMOD_SPEAKER_TOP_BACK_LEFT,
            //    FMOD_SPEAKER_TOP_BACK_RIGHT,

            //FMOD_Channel_SetSpeakerMix(channel, tvols[0], tvols[1], tvols[2], tvols[3], tvols[4], tvols[5], tvols[6], tvols[7] );
            FMOD_RESULT result = FMOD_Channel_SetMixLevelsOutput(
                channel, 
                tvols[0], 
                tvols[1], 
                tvols[2], 
                tvols[3], 
                tvols[4], 
                tvols[5], 
                tvols[6], 
                tvols[7]
            );

            if (result != FMOD_OK) {
                ofLogError("ofxMultiSpeakerSoundPlayer :: setPan : FMOD_Channel_SetMixLevelsOutput - ERROR");
            }
        }
    }
}


//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setPaused(bool bP) {
    if (isPlaying() == true) {
        FMOD_Channel_SetPaused(channel,bP);
        bPaused = bP;
    }
}


//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setSpeed(float spd) {
    if (isPlaying() == true) {
        FMOD_Channel_SetFrequency(channel, internalFreq * spd);
    }
    speed = spd;
}


//------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setLoop(bool bLp) {
    if (isPlaying() == true) {
        FMOD_Channel_SetMode(channel,  (bLp == true) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    }
    bLoop = bLp;
}

// ----------------------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::setMultiPlay(bool bMp) {
    bMultiPlay = bMp;		// be careful with this...
}

// ----------------------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::play() {

    // if it's a looping sound, we should try to kill it, no?
    // or else people will have orphan channels that are looping
    if (bLoop == true) {
        FMOD_Channel_Stop(channel);
    }

    // if the sound is not set to multiplay, then stop the current,
    // before we start another
    if (!bMultiPlay) {
        FMOD_Channel_Stop(channel);
    }

    FMOD_System_PlaySound(sys, sound, channelgroup, bPaused, &channel);

    FMOD_Channel_GetFrequency(channel, &internalFreq);
    FMOD_Channel_SetVolume(channel,volume);
    setPan(pan);
    FMOD_Channel_SetFrequency(channel, internalFreq * speed);
    FMOD_Channel_SetMode(channel, (bLoop == true) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

    //fmod update() should be called every frame - according to the docs.
    //we have been using fmod without calling it at all which resulted in channels not being able
    //to be reused.  we should have some sort of global update function but putting it here
    //solves the channel bug
    FMOD_System_Update(sys);

}
//
//// ----------------------------------------------------------------------------
//void ofxMultiSpeakerSoundPlayer::playTo(SpeakerPair aSpeakerPair) {
//
//	// if it's a looping sound, we should try to kill it, no?
//	// or else people will have orphan channels that are looping
//	if (bLoop == true) {
//		FMOD_Channel_Stop(channel);
//	}
//
//	// if the sound is not set to multiplay, then stop the current,
//	// before we start another
//	if (!bMultiPlay) {
//		FMOD_Channel_Stop(channel);
//	}
//
//	FMOD_System_PlaySound(sys, FMOD_CHANNEL_FREE, sound, 1, &channel);
//
//	/*
//	FMOD_RESULT Channel::setSpeakerMix(
//	  float  frontleft,
//	  float  frontright,
//	  float  center,
//	  float  lfe,
//	  float  backleft,
//	  float  backright,
//	  float  sideleft,
//	  float  sideright
//	);
//	*/
//
//    if( aSpeakerPair == SPEAKERS_FRONT ) {
//		FMOD_Channel_SetSpeakerMix(channel, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
//    } else if( aSpeakerPair == SPEAKERS_BACK ) {
//		FMOD_Channel_SetSpeakerMix(channel, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
//    } else if( aSpeakerPair == SPEAKERS_SIDE ) {
//        FMOD_Channel_SetSpeakerMix(channel, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0);
//    }
//
//
//    //switch (speaker)
//    //{
//    //case 0 : // send to back output
//    //    FMOD_Channel_SetSpeakerMix(channel, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
//    //    break;
//    //case 1 : // send to side output
//    //    FMOD_Channel_SetSpeakerMix(channel, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0);
//    //    break;
//    //case 2 : // send to front output
//    //    FMOD_Channel_SetSpeakerMix(channel, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
//    //    break;
//    //case 3 : // send to center output
//    //    FMOD_Channel_SetSpeakerMix(channel, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0);
//    //    break;
//    //}
//
//    FMOD_Channel_SetPaused(channel, 0);
//
//    FMOD_Channel_GetFrequency(channel, &internalFreq);
//    FMOD_Channel_SetVolume(channel,volume);
//    FMOD_Channel_SetFrequency(channel, internalFreq * speed);
//    FMOD_Channel_SetMode(channel, (bLoop == true) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
//
//    //fmod update() should be called every frame - according to the docs.
//    //we have been using fmod without calling it at all which resulted in channels not being able
//    //to be reused.  we should have some sort of global update function but putting it here
//    //solves the channel bug
//    //FMOD_System_Update(sys);
//
//}

// ----------------------------------------------------------------------------
void ofxMultiSpeakerSoundPlayer::stop() {
    FMOD_Channel_Stop(channel);
}
