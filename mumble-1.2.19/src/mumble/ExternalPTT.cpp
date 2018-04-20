#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef USE_EXTPTT_WIRINGPI
  #include <wiringPi.h>
#endif
#include "mumble_pch.hpp"
#include "Global.h"
#include "Settings.h"
#include "ExternalPTT.h"

ExternalPTT::ExternalPTT() {
  puts("ExternalPTT::ExternalPTT() called!");
  m_bInitialized = false;
  m_bKeyed = false;
  m_hSerialPort = -1;
}

ExternalPTT::~ExternalPTT() {
  puts("ExternalPTT::~ExternalPTT() called!");
  closeCOM();
}

bool ExternalPTT::init() {
  if(m_bInitialized) return false; // Init only once!
  puts("ExternalPTT::init() called!");
  switch(g.s.ExtPTT_Mode) {
    case 0:
#ifdef USE_EXTPTT_WIRINGPI
      wiringPiSetupSys(); // GPIO-ports need to be exported as root, see http://wiringpi.com/reference/setup/
#else
      fputs("ExtPTT_Mode 0: WiringPi-support not compiled! Recompile with correct settings!\n", stderr);
#endif
      break;
    case 1:
#ifdef USE_EXTPTT_WIRINGPI
      wiringPiSetupGpio(); // requires root!
      printf("ExternalPTT::init(): Setting PTT pin %d to input mode with pull-up; setting SQL pin %d to output mode!\n", g.s.ExtPTT_PinPTT, g.s.ExtPTT_PinSQL);
      pinMode(g.s.ExtPTT_PinPTT, INPUT);
      pullUpDnControl(g.s.ExtPTT_PinPTT, PUD_UP); // Activate pull-up so that you can simply pull pin to GND without having to deal with 3.3V vs 5V
      pinMode(g.s.ExtPTT_PinSQL, OUTPUT);
#else
      fputs("ExtPTT_Mode 1: WiringPi-support not compiled! Recompile with correct settings!\n", stderr);
#endif
      break;
    case 2:
      { // avoid "error: jump to case label [-fpermissive]" message
        const char *pszDev = g.s.ExtPTT_SerialDevice.toLatin1().constData();
        printf("ExternalPTT::init(): Opening serial port \"%s\"...\n", pszDev);
        if(!openCOM(pszDev)) {
          perror(pszDev);
          return false;
        }
        setRTS(g.s.ExtPTT_InvertSQL);
      }
      break;
    default:
      printf("ExternalPTT::init(): unkown mode %d! ExternalPTT disabled!\n", g.s.ExtPTT_Mode);
      return false;
  }
#ifdef USE_EXTPTT_WIRINGPI
  if(g.s.ExtPTT_Mode == 0 || g.s.ExtPTT_Mode == 1) digitalWrite(g.s.ExtPTT_PinSQL, g.s.ExtPTT_InvertSQL ? HIGH : LOW);
#endif
  m_bInitialized = true;
  return true;
}

bool ExternalPTT::getPTT() {
  if(!m_bInitialized) return false;

  bool bKeyed = g.s.ExtPTT_InvertPTT;
  if(g.s.ExtPTT_Mode == 2) bKeyed ^= getCTS();
#ifdef USE_EXTPTT_WIRINGPI
  else bKeyed ^= digitalRead(g.s.ExtPTT_PinPTT) == HIGH;
#endif

  //printf("ExternalPTT::getPTT() called! getCTS()=%d inv=%d res=%d\n", getCTS(), g.s.ExtPTT_InvertPTT, bKeyed);

  if(bKeyed != m_bKeyed) {
    printf("ExternalPTT::getPTT() called! Pin=%d%s\n", g.s.ExtPTT_PinPTT, bKeyed ? " ***TX***" : "");
    m_bKeyed = bKeyed;
  }
  return bKeyed;
}

bool ExternalPTT::setSQL(bool state) {
  if(!m_bInitialized) return false;

  printf("ExternalPTT::setSQL(%s) called! Pin=%d\n", state ? "true" : "false", g.s.ExtPTT_PinSQL);
  state = (state ^ g.s.ExtPTT_InvertSQL) && !m_bKeyed;
  if(g.s.ExtPTT_Mode == 2) setRTS(state);
#ifdef USE_EXTPTT_WIRINGPI
  else digitalWrite(g.s.ExtPTT_PinSQL, state ? HIGH : LOW);
#endif
  return true;
}

bool ExternalPTT::openCOM(const char *pszDevice) {
  // Based on http://www.linuxquestions.org/questions/programming-9/manually-controlling-rts-cts-326590/
  closeCOM();
  m_hSerialPort = open(pszDevice, O_RDWR);
  if(m_hSerialPort != -1) {
    if(tcgetattr(m_hSerialPort, &m_termios) != -1) {
      struct termios attr = m_termios;
      attr.c_cflag |= CRTSCTS | CLOCAL;
      attr.c_oflag = 0;
      if(tcflush(m_hSerialPort, TCIOFLUSH) != -1) {
        if(tcsetattr(m_hSerialPort, TCSANOW, &attr) != -1) return true;
      }
    }
    close(m_hSerialPort);
  }
  return false;
}

bool ExternalPTT::closeCOM() {
  if(m_hSerialPort != -1) {
    tcsetattr(m_hSerialPort, TCSANOW, &m_termios);
    close(m_hSerialPort);
    m_hSerialPort = -1;
  }
  return true;
}

bool ExternalPTT::setRTS(bool state) {
  if(m_hSerialPort != -1) {
    int status;
    if(ioctl(m_hSerialPort, TIOCMGET, &status) != -1) {
      if(state) status |= TIOCM_RTS;
      else status &= ~TIOCM_RTS;
      if(ioctl(m_hSerialPort, TIOCMSET, &status) != -1) return true;
    }
  }
  return false;
}

bool ExternalPTT::getCTS() const {
  if(m_hSerialPort != -1) {
    int status;
    if(ioctl(m_hSerialPort, TIOCMGET, &status) != -1
      && (status & TIOCM_CTS) != 0) return true;
  }
  return false;
}
