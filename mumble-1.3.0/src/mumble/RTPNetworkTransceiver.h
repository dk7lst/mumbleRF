#ifndef RTPNETWORKTRANSCEIVER_H_
#define RTPNETWORKTRANSCEIVER_H_

class UDPSocket;
class IPAddr;

// Class for sending+receiving of RTP network protocol:
class RTPNetworkTransceiver {
public:
  RTPNetworkTransceiver();
  ~RTPNetworkTransceiver();

  bool init(quint16 u16LocalPort, const QString &qsServerIP, quint16 u16ServerPort, int iSampleRate, int iSampleSizeInBytes, int iPayLoadType, int iSamplesPerPacket);

  bool getIsInitialized() const {
    return m_bIsInitialized;
  }

  int getSampleRate() const {
    return m_iSampleRate;
  }

  int getSampleSizeInBytes() const {
    return m_iSampleSizeInBytes;
  }

  int getSamplesPerPacket() const {
    return m_iSamplesPerPacket;
  }

  int getPacketSizeInBytes() const {
    return m_iSamplesPerPacket * m_iSampleSizeInBytes;
  }

  int getPacketInterval_ms() const {
    return 1000 * m_iSamplesPerPacket / m_iSampleRate;
  }

  int receive(void *pDataBuf, int iMaxBytes, bool &bIsMarked);
  bool transmit(const void *pDataBuf, int iBytes, bool bSetMarker = false);

protected:
  bool m_bIsInitialized;
  UDPSocket *m_pSocket;
  IPAddr *m_pHost;
  int m_iSampleRate;
  int m_iSampleSizeInBytes;
  int m_iPayLoadType;
  int m_iSamplesPerPacket;
  unsigned short m_uSeq;
  unsigned int m_uTimestamp, m_uSSRC;
};

#endif // RTPNETWORKTRANSCEIVER_H_
