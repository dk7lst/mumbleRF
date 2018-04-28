#include "mumble_pch.hpp"
#include "socketlib.h"
#include "RTPNetworkTransceiver.h"

QMutex qmRTPNetworkTransceiver;

RTPNetworkTransceiver::RTPNetworkTransceiver() {
  qWarning("RTPNetworkTransceiver: Initialized");
  m_bIsInitialized = false;
  m_pSocket = new UDPSocket();
  m_pHost = new IPAddr();
  m_uSeq = 0;
  m_uTimestamp = 0;
  m_uSSRC = rand();
}

RTPNetworkTransceiver::~RTPNetworkTransceiver() {
  if(m_pSocket) m_pSocket->close();
  delete m_pHost;
  delete m_pSocket;
  qWarning("RTPNetworkTransceiver: Destroyed");
}

bool RTPNetworkTransceiver::init(quint16 u16LocalPort, const QString &qsServerIP, quint16 u16ServerPort, int iSampleRate, int iSampleSizeInBytes, int iPayLoadType, int iSamplesPerPacket) {
  if(m_bIsInitialized || !m_pSocket) return m_bIsInitialized;
  QMutexLocker qml(&qmRTPNetworkTransceiver);
  m_pHost->setbyhostname(qsServerIP.toLatin1().data());
  m_pHost->setport(u16ServerPort);
  m_iSampleRate = iSampleRate;
  m_iSampleSizeInBytes = iSampleSizeInBytes;
  m_iPayLoadType = iPayLoadType;
  m_iSamplesPerPacket = iSamplesPerPacket;
  m_bIsInitialized = m_pSocket->open() != -1
    && m_pSocket->bind(u16LocalPort) == 0;
  return m_bIsInitialized;
}

int RTPNetworkTransceiver::receive(void *pDataBuf, int iMaxBytes, bool &bIsMarked) {
  bIsMarked = false;
  if(!m_bIsInitialized || iMaxBytes < 0) return -1;

  const int ciMaxHeaderSize = 256;
  const int ciMaxPacketSize = ciMaxHeaderSize + iMaxBytes;
  unsigned char *const pPacket = new unsigned char[ciMaxPacketSize];
  if(!pPacket) return -2;

  int iResult = -3;
  IPAddr fromAddr;
  unsigned fromAddrLen = 0;
  int iBytes = m_pSocket->recvfrom(pPacket, ciMaxPacketSize, &fromAddr, &fromAddrLen);
  //printf("iBytes=%d pPacket[0]=%X\n", iBytes, pPacket[0]);
  if(iBytes >= 12 && (pPacket[0] >> 6) == 2) {
    const bool bUsesPadding = (pPacket[0] & 0x20) != 0;
    const bool bUsesExtHdr = (pPacket[0] & 0x10) != 0;
    const int iCSRC_Cnt = pPacket[0] & 0xF;
    const int iPayLoadType = pPacket[1] & 0x7F;
    const int iExtHdrOffset = 12 + 4 * iCSRC_Cnt;
    if(iPayLoadType == m_iPayLoadType && iExtHdrOffset + 4 < iBytes) {
      int iDataOffset = iExtHdrOffset;
      if(bUsesExtHdr) {
        //const int iExtHdrId = qToLittleEndian(*(short *)(pPacket + iExtHdrOffset));
        const int iExtHdrSize = qToLittleEndian(*(short *)(pPacket + iExtHdrOffset + 2)) * 4;
        iDataOffset += 4 + iExtHdrSize;
      }
      int iDataSize = iBytes - iDataOffset;
      if(bUsesPadding) iDataSize -= pPacket[iBytes - 1];
      if(iDataSize >= 0 && iDataSize % m_iSampleSizeInBytes == 0) {
        bIsMarked = (pPacket[1] & 0x80) != 0;
        iResult = qMin(iMaxBytes, iDataSize);
        memcpy(pDataBuf, pPacket + iDataOffset, iResult);
        iResult /= m_iSampleSizeInBytes;
      }
      else iResult = -5;
    }
    else iResult = -4;
  }

  delete pPacket;
  return iResult;
}

bool RTPNetworkTransceiver::transmit(const void *pDataBuf, int iBytes, bool bSetMarker) {
  if(!m_bIsInitialized || iBytes < 0) return false;
  const int ciHeaderSize = 12, iPacketSize = ciHeaderSize + iBytes;
  char *pPacket = new char[iPacketSize];
  pPacket[0] = 0x80; // https://en.wikipedia.org/wiki/Real-time_Transport_Protocol#Packet_header
  pPacket[1] = (m_iPayLoadType & 0x7F) | (bSetMarker ? 0x80 : 0);
  *(short *)(pPacket + 2) = qToBigEndian(m_uSeq++);
  *(int *)(pPacket + 4) = qToBigEndian(m_uTimestamp);
  *(int *)(pPacket + 8) = qToBigEndian(m_uSSRC);
  m_uTimestamp += iBytes / m_iSampleSizeInBytes;
  memcpy(pPacket + ciHeaderSize, pDataBuf, iBytes);
  bool res = m_pSocket->sendto(m_pHost, pPacket, iPacketSize) == iPacketSize;
  delete pPacket;
  return res;
}
