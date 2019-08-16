#ifndef __RECORD_INSTANCE_HPP__
#define __RECORD_INSTANCE_HPP__
#include <pthread.h>
#include <list>
#include "videomixer.h"
#include "audiomixer.h"
#include "audioencoder.h"
#include "pipevideoinput.h"
#include "core/VideoEncoderWorker.h"
#include "mp4recorder.h"

class RecordInstance {
public:
	virtual ~RecordInstance();
    static RecordInstance* GetInstance();
    int StartRecord(const std::string filename);
    int StopRecord();

    int AddParticipant(int id);
    int RemoveParticipant(int id);
    int OutputToRec(int id, bool video, char *buf, int len);
    int SetCompositionType();
private:

	Properties properties_;
    MP4Recorder *mp4record_;

    AudioMixer *audiomixer_;
    AudioEncoderWorker *audioencoder_;

    VideoMixer *videomixer_;
    VideoEncoderWorker *videoencoder_;

    int sidebarid_;
    int mosaicid_;
private:
	bool Init();
    static pthread_mutex_t m_mutex;
    static RecordInstance* m_instance;
    RecordInstance();
};


#endif//__RECORD_INSTANCE_HPP__
