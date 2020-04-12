#include "mumble_pch.hpp"
#include "RTPAudio.h"
#include "RTPNetworkTransceiver.h"

#include "Global.h"

#define RTPNetworkTransceiverInit {bRunning = m_pRTPNetworkTransceiver->init(g.s.RTPAudio_u16LocalPort, g.s.RTPAudio_qsServerIP, g.s.RTPAudio_u16ServerPort,\
 g.s.RTPAudio_SampleRate, sizeof(short), g.s.RTPAudio_PayLoadType, g.s.RTPAudio_SamplesPerPacket);\
 if(!bRunning) qWarning("RTPNetworkTransceiver::init() failed! Aborting!");}

// Class for registering an Audio Input Device Type:
class RTPAudioInputRegistrar : public AudioInputRegistrar {
public:
  RTPAudioInputRegistrar(RTPNetworkTransceiver *pRTPNetworkTransceiver);
  virtual AudioInput *create();
  virtual const QList<audioDevice> getDeviceChoices();
  virtual void setDeviceChoice(const QVariant &, Settings &);
  virtual bool canEcho(const QString &) const;

protected:
  RTPNetworkTransceiver *m_pRTPNetworkTransceiver;
};

// Class for registering an Audio Output Device Type:
class RTPAudioOutputRegistrar : public AudioOutputRegistrar {
public:
  RTPAudioOutputRegistrar(RTPNetworkTransceiver *pRTPNetworkTransceiver);
  virtual AudioOutput *create();
  virtual const QList<audioDevice> getDeviceChoices();
  virtual void setDeviceChoice(const QVariant &, Settings &);

protected:
  RTPNetworkTransceiver *m_pRTPNetworkTransceiver;
};

// Class to create Registrar-instances at the right time:
class RTPAudioInit : public DeferInit {
public:
  void initialize();
  void destroy();

protected:
  RTPNetworkTransceiver *pRTPNetworkTransceiver;
  RTPAudioInputRegistrar *pRTPAudioInputRegistrar;
  RTPAudioOutputRegistrar *pRTPAudioOutputRegistrar;
};

static RTPAudioInit raInit;
QMutex qmRTPAudio;

void RTPAudioInit::initialize() {
  pRTPNetworkTransceiver = new RTPNetworkTransceiver();
  pRTPAudioInputRegistrar = new RTPAudioInputRegistrar(pRTPNetworkTransceiver);
  pRTPAudioOutputRegistrar = new RTPAudioOutputRegistrar(pRTPNetworkTransceiver);
}

void RTPAudioInit::destroy() {
  QMutexLocker qml(&qmRTPAudio);
  delete pRTPAudioOutputRegistrar;
  delete pRTPAudioInputRegistrar;
  delete pRTPNetworkTransceiver;
}

RTPAudioInputRegistrar::RTPAudioInputRegistrar(RTPNetworkTransceiver *pRTPNetworkTransceiver) : AudioInputRegistrar(QLatin1String("RTP-Network-Audio")) {
  m_pRTPNetworkTransceiver = pRTPNetworkTransceiver;
}

AudioInput *RTPAudioInputRegistrar::create() {
  return new RTPAudioInput(m_pRTPNetworkTransceiver);
}

const QList<audioDevice> RTPAudioInputRegistrar::getDeviceChoices() {
  QList<audioDevice> qlReturn;
  qlReturn << audioDevice(QString::fromUtf8("Incoming RTP network connection"), QString::fromUtf8("RTP1"));
  return qlReturn;
}

void RTPAudioInputRegistrar::setDeviceChoice(const QVariant &choice, Settings &s) {
}

bool RTPAudioInputRegistrar::canEcho(const QString &) const {
  return false;
}

RTPAudioOutputRegistrar::RTPAudioOutputRegistrar(RTPNetworkTransceiver *pRTPNetworkTransceiver) : AudioOutputRegistrar(QLatin1String("RTP-Network-Audio")) {
  m_pRTPNetworkTransceiver = pRTPNetworkTransceiver;
}

AudioOutput *RTPAudioOutputRegistrar::create() {
  return new RTPAudioOutput(m_pRTPNetworkTransceiver);
}

const QList<audioDevice> RTPAudioOutputRegistrar::getDeviceChoices() {
  QList<audioDevice> qlReturn;
  qlReturn << audioDevice(QString::fromUtf8("Outgoing RTP network connection"), QString::fromUtf8("RTP1"));
  return qlReturn;
}

void RTPAudioOutputRegistrar::setDeviceChoice(const QVariant &choice, Settings &s) {
}

RTPAudioInput::RTPAudioInput(RTPNetworkTransceiver *pRTPNetworkTransceiver) : AudioInput() {
  qWarning("RTPAudioInput: Initialized");
  m_pRTPNetworkTransceiver = pRTPNetworkTransceiver;
  m_bPTT = false;
  bRunning = true;
}

RTPAudioInput::~RTPAudioInput() {
  bRunning = false;
  wait();
  qWarning("RTPAudioInput: Destroyed");
}

void RTPAudioInput::run() {
  QMutexLocker qml(&qmRTPAudio);

  RTPNetworkTransceiverInit;

  iMicFreq = m_pRTPNetworkTransceiver->getSampleRate();
  eMicFormat = SampleShort;
  iMicChannels = 1;
  initializeMixer();

  qml.unlock();

  const int ciMaxAllowedSamplesPerPacket = SAMPLE_RATE;
  char inbuf[ciMaxAllowedSamplesPerPacket * m_pRTPNetworkTransceiver->getSampleSizeInBytes()];

  bool bLastPTT = false;
  while(bRunning) {
    int iSamplesReceived = m_pRTPNetworkTransceiver->receive(inbuf, sizeof inbuf, m_bPTT);
    if(m_bPTT != bLastPTT) {
      printf("PTT state changed: %d\n", m_bPTT);
      bLastPTT = m_bPTT;
    }
    //printf("RTPAudioInput::run(): %d samples received! PTT=%d\n", iSamplesReceived, m_bPTT);
    if(iSamplesReceived > 0) {
      //printf("RTPAudioInput::run(): %d samples received! PTT=%d\n", iSamplesReceived, m_bPTT);
      if(g.s.RTPAudio_UseBigEndian) {
        short *wbuf = (short *)inbuf;
        for(int i = 0; i < iSamplesReceived; ++i) {
          *wbuf = qToLittleEndian(*wbuf);
          ++wbuf;
        }
      }
      addMic(inbuf, iSamplesReceived);
    }
    else if(iSamplesReceived < 0) printf("RTPNetworkTransceiver::receive() failed with error code %d!\n", iSamplesReceived);
  }
}

RTPAudioOutput::RTPAudioOutput(RTPNetworkTransceiver *pRTPNetworkTransceiver) : AudioOutput() {
  qWarning("RTPAudioOutput: Initialized");
  m_pRTPNetworkTransceiver = pRTPNetworkTransceiver;
  m_bSQL = false;
  bRunning = true;
}

RTPAudioOutput::~RTPAudioOutput() {
  bRunning = false;
  wipe();
  wait();
  qWarning("RTPAudioOutput: Destroyed");
}

void RTPAudioOutput::run() {
  QMutexLocker qml(&qmRTPAudio);

  RTPNetworkTransceiverInit;

  iMixerFreq = m_pRTPNetworkTransceiver->getSampleRate();
  eSampleFormat = SampleShort;
  iChannels = 1;
  unsigned int chanmasks[iChannels] = {SPEAKER_FRONT_CENTER};
  initializeMixer(chanmasks);
  printf("RTPAudioOutput::run(): iMixerFreq=%d iChannels=%d iFrameSize=%d iSampleSize=%d\n", iMixerFreq, iChannels, iFrameSize, iSampleSize);

  qml.unlock();

  char outbuf[m_pRTPNetworkTransceiver->getPacketSizeInBytes()];

  while(bRunning) {
    bool bDataAvail = mix(outbuf, g.s.RTPAudio_SamplesPerPacket);
    //printf("RTPAudioOutput::run(): Sending %d samples! (bDataAvail=%d g.s.fVolume=%f)\n", g.s.RTPAudio_SamplesPerPacket, bDataAvail, g.s.fVolume);
    if(bDataAvail) {
      if(g.s.RTPAudio_UseBigEndian) {
        short *wbuf = (short *)outbuf;
        for(int i = 0; i < g.s.RTPAudio_SamplesPerPacket; ++i) {
          *wbuf = qToBigEndian(*wbuf);
          ++wbuf;
        }
      }
    }
    else memset(outbuf, 0, sizeof outbuf);
    m_pRTPNetworkTransceiver->transmit(outbuf, sizeof outbuf, m_bSQL);
    msleep(m_pRTPNetworkTransceiver->getPacketInterval_ms() * 0.99); // TODO: Use real timer as timing is critical here!
  }
}
