#ifdef USE_RTPAUDIO
#ifndef RTPAUDIO_H_
#define RTPAUDIO_H_

#include "AudioInput.h"
#include "AudioOutput.h"

class RTPNetworkTransceiver;

// Virtual soundcard input which receives audio via RTP protocol.
class RTPAudioInput : public AudioInput {
public:
  RTPAudioInput(RTPNetworkTransceiver *pRTPNetworkTransceiver);
  ~RTPAudioInput();

  void run();

  bool getPTT() const {
    return m_bPTT;
  }

protected:
  RTPNetworkTransceiver *m_pRTPNetworkTransceiver;
  bool m_bPTT;

private:
  Q_OBJECT
  Q_DISABLE_COPY(RTPAudioInput)
};

// Virtual soundcard output which transmits audio via RTP protocol.
class RTPAudioOutput : public AudioOutput {
public:
  RTPAudioOutput(RTPNetworkTransceiver *pRTPNetworkTransceiver);
  ~RTPAudioOutput();

  void run();

  void setSQL(bool state) {
    m_bSQL = state;
  }

protected:
  RTPNetworkTransceiver *m_pRTPNetworkTransceiver;
  bool m_bSQL;

private:
  Q_OBJECT
  Q_DISABLE_COPY(RTPAudioOutput)
};

#endif // RTPAUDIO_H_
#endif // USE_RTPAUDIO
