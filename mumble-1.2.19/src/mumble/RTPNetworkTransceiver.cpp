#include "mumble_pch.hpp"
#include "RTPNetworkTransceiver.h"

QMutex qmRTPNetworkTransceiver;

RTPNetworkTransceiver::RTPNetworkTransceiver() {
  qWarning("RTPNetworkTransceiver: Initialized");
  m_bIsInitialized = false;
  m_pSocket = new QUdpSocket();
  m_uSeq = 0;
  m_uTimestamp = 0;
  m_uSSRC = rand();
}

RTPNetworkTransceiver::~RTPNetworkTransceiver() {
  delete m_pSocket;
  qWarning("RTPNetworkTransceiver: Destroyed");
}

void RTPNetworkTransceiver::init(quint16 u16LocalPort, const QString &qsServerIP, quint16 u16ServerPort, int iSampleRate, int iSampleSizeInBytes, int iPayLoadType, int iSamplesPerPacket) {
  if(m_bIsInitialized) return;
  QMutexLocker qml(&qmRTPNetworkTransceiver);
  m_Host.setAddress(qsServerIP);
  m_u16LocalPort = u16LocalPort;
  m_u16ServerPort = u16ServerPort;
  m_iSampleRate = iSampleRate;
  m_iSampleSizeInBytes = iSampleSizeInBytes;
  m_iPayLoadType = iPayLoadType;
  m_iSamplesPerPacket = iSamplesPerPacket;
  m_bIsInitialized = true;
}

int RTPNetworkTransceiver::receive(void *pDataBuf, int iMaxBytes) {
  if(!m_bIsInitialized || iMaxBytes < 0) return 0;
  // TODO: Receive and parse packet.
  return 0;
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
  bool res = m_pSocket->writeDatagram(pPacket, iPacketSize, m_Host, m_u16ServerPort) == iPacketSize;
  delete pPacket;
  return res;
}
