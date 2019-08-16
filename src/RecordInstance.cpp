#include <stdio.h>
#include "RecordInstance.h"
RecordInstance *RecordInstance::m_instance;
pthread_mutex_t RecordInstance::m_mutex;

class RtcVideoEncoder :
	public VideoEncoderWorker {
public:
	RtcVideoEncoder();
	virtual ~RtcVideoEncoder();

	int Init(VideoInput *input);
	int SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod,const Properties & properties);
	int End();

	int  SetMediaListener(MediaFrame::Listener *listener);

public:
	int Start();
	int Stop();

	virtual void FlushRTXPackets();
	virtual void SmoothFrame(const VideoFrame *videoFrame,DWORD sendingTime);
};

RtcVideoEncoder::RtcVideoEncoder() : VideoEncoderWorker()
{
}

RtcVideoEncoder::~RtcVideoEncoder()
{
}

int RtcVideoEncoder::Init(VideoInput *input)
{
	//Init encoder
	return VideoEncoderWorker::Init(input);
}

int RtcVideoEncoder::SetCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int intraPeriod, const Properties& properties)
{
	//Set codec properties
	if (!VideoEncoderWorker::SetCodec(codec,mode,fps,bitrate,intraPeriod,properties))
		//Error
		return 0;

	//Exit
	return 1;
}

int RtcVideoEncoder::Start()
{
	//Start encoder
	VideoEncoderWorker::Start();

	return 1;
}

int RtcVideoEncoder::Stop()
{
	//Stop encoder
	VideoEncoderWorker::Stop();

	return 1;
}

int RtcVideoEncoder::End()
{
	//Stop
	return Stop();
}

void RtcVideoEncoder::FlushRTXPackets()
{
}

void RtcVideoEncoder::SmoothFrame(const VideoFrame *videoFrame,DWORD sendingTime)
{
	//RTPMultiplexerSmoother::SmoothFrame(videoFrame,sendingTime);
}

int  RtcVideoEncoder::SetMediaListener(MediaFrame::Listener *listener)
{
	//Set codec properties
	if (!VideoEncoderWorker::SetMediaListener(listener))
		//Error
		return 0;

	//Exit
	return 1;
}

RecordInstance::RecordInstance() {
}

RecordInstance::~RecordInstance() {
	videoencoder_->End();
	audioencoder_->StopEncoding();
}
RecordInstance* RecordInstance::GetInstance() {
	do {
		if (NULL == m_instance) {
			pthread_mutex_lock(&m_mutex);
			if (NULL != m_instance) {
				pthread_mutex_unlock(&m_mutex);
				break;
			}
			m_instance = new RecordInstance;
			if (m_instance->Init() != true) {
				pthread_mutex_unlock(&m_mutex);
				break;
			}
			pthread_mutex_unlock(&m_mutex);
		}
	} while (0);
	return m_instance;
}

//音视频编码线程一直存在，伴随着程序的整个生命周期！！！
bool RecordInstance::Init() {
	properties_.SetProperty("streaming", "true");
	properties_.SetProperty("mosaics.default.compType", (int)Mosaic::mosaic1p1);
	properties_.SetProperty("mosaics.default.size"		, HD720P);

	properties_.SetProperty("rate"		, 8000);

	mp4record_ = new MP4Recorder();

	audiomixer_ = new AudioMixer();
	audiomixer_->CreateMixer(1);

	audioencoder_ = new AudioEncoderWorker();
	audioencoder_->SetAudioCodec(AudioCodec::Type::PCMU);
	audioencoder_->AddListener(mp4record_);
	audioencoder_->StartEncoding();
	audioencoder_->Init(audiomixer_->GetInput(1));

	videomixer_ = new VideoMixer(L"RSU");
	videomixer_->CreateMixer(1, L"chn1");

	videoencoder_ = new RtcVideoEncoder();
	videoencoder_->SetCodec(VideoCodec::Type::H264, HD720P, 25, 1024*1024, 30, properties_);\
	videoencoder_->SetMediaListener(mp4record_);
	videoencoder_->Init(videomixer_->GetInput(1));
	videoencoder_->Start();

	sidebarid_ = audiomixer_->CreateSidebar();
	mosaicid_ = videomixer_->CreateMosaic(Mosaic::Type::mosaic1p1, HD720P);
	return true;
}

//音视频混操作线程伴随着整个录像操作的整个过程！！！
int RecordInstance::StartRecord(const std::string filename) {
	mp4record_->Create(filename.c_str());
	mp4record_->Record();

	audiomixer_->Init(properties_);
	videomixer_->Init(properties_);


	return 0;
}

int RecordInstance::StopRecord() {
	mp4record_->Close();
	return 0;
}

int RecordInstance::AddParticipant(int id) {


	if (id != 1 ) {
		std::wstring tag = L"" + id;
		videomixer_->CreateMixer(id, tag);
		audiomixer_->CreateMixer(id);

	}

	audiomixer_->InitMixer(id, sidebarid_);
	videomixer_->InitMixer(id, mosaicid_);
	videomixer_->GetOutput(id)->SetVideoSize(1280, 720);

	audiomixer_->AddSidebarParticipant(sidebarid_, id);
	videomixer_->AddMosaicParticipant(mosaicid_, id);
	return 0;
}

int RecordInstance::SetCompositionType() {
	videomixer_->SetCompositionType(mosaicid_, Mosaic::Type::mosaic1x1, HD720P);
	return 0;
}
int RecordInstance::RemoveParticipant(int id) {
	audiomixer_->EndMixer(id);
	videomixer_->EndMixer(id);
	audiomixer_->RemoveSidebarParticipant(sidebarid_, id);
	videomixer_->RemoveMosaicParticipant(mosaicid_, id);
	return 0;
}

int RecordInstance::OutputToRec(int id, bool video, char *buf, int len) {
	if (video) {
		BYTE*  frameBuffer = (BYTE*)buf;
		videomixer_->GetOutput(id)->NextFrame(frameBuffer);
	} else {
		SWORD*  frameBuffer = (SWORD*)buf;
		audiomixer_->GetOutput(id)->PlayBuffer(frameBuffer, 480, 0);
	}
	return 0;
}



//int main(){
//	RecordInstance *instance = RecordInstance::GetInstance();
//	instance->StartRecord("abc.mp4");
//	instance->AddParticipant(1);
//	FILE *fp = fopen("abc.yuv", "r+");
//	int frameSize = 1280*720*3/2;
//	BYTE  frameBuffer[frameSize];
//	int count = 0;
//	while (1) {
//		int r  = fread(frameBuffer, 1, frameSize, fp);
//		if (r <= 0) {
//			break;
//		}
//		if(++count == 25) {
//			instance->SetCompositionType();
//		}
//		instance->OutputToRec(1, true, (char *)frameBuffer, 1280*720*3/2);
//		usleep(60*1000);
//	}
//	instance->RemoveParticipant(1);
//	printf("STOPRECORD BEGIN!\n");
//	instance->StopRecord();
//	printf("STOPRECORD END!\n");
//	return 0;
//}
