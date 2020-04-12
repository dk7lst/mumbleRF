#ifdef USE_EXTPTT
#ifndef EXTERNALPTT_H_
#define EXTERNALPTT_H_
#include <termios.h>
#include <unistd.h>

class ExternalPTT {
public:
  ExternalPTT();
  ~ExternalPTT();

  bool init();
  bool getPTT();
  bool setSQL(bool state);

protected:
  bool openCOM(const char *pszDevice);
  bool closeCOM();
  bool setRTS(bool state);
  bool getCTS() const;

  bool m_bInitialized;
  bool m_bKeyed;
  int m_hSerialPort;
  struct termios m_termios;
};
#endif // EXTERNALPTT_H_
#endif // USE_EXTPTT
