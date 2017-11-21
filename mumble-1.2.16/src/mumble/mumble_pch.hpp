#ifndef Q_MOC_RUN
#ifndef _MUMBLE_PCH_H
#define _MUMBLE_PCH_H

#define QT_NO_DEBUG_STREAM
#define QT_NO_CAST_TO_ASCII
#define QT_NO_CAST_FROM_ASCII
#define QT_USE_FAST_CONCATENATION
#define QT_USE_FAST_OPERATOR_PLUS

#define NOMINMAX
#define _WINSOCKAPI_

#if defined(__INTEL_COMPILER)
#define _MSC_EXTENSIONS
#include <mathimf.h>
#else
#define BOOST_TYPEOF_SUPPRESS_UNNAMED_NAMESPACE
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#undef nil
#undef check
#undef TYPE_BOOL
#endif

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtSvg/QtSvg>
#ifdef USE_DBUS
#include <QtDBus/QtDBus>
#endif
#include <QtNetwork/QtNetwork>
#include <QtSql/QtSql>
#include <QtXml/QtXml>

#ifdef Q_OS_WIN
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#endif

#define __int64_t __int64
#include <sndfile.h>
#undef __int64_t
#include <celt.h>
#ifdef USE_SBCELT
#include <sbcelt.h>
#endif
#include <speex/speex.h>
#include <speex/speex_jitter.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_echo.h>
#include <speex/speex_resampler.h>

#include <boost/array.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/typeof/typeof.hpp>
#include <boost/weak_ptr.hpp>

#ifdef Q_OS_WIN
#include <shellapi.h>
#include <winsock2.h>
#include <qos2.h>
#include <wintrust.h>
#include <Softpub.h>
#include <Dbt.h>
#include <delayimp.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <psapi.h>
#ifndef Q_CC_INTEL
#include <math.h>
#define lroundf(x) ( static_cast<long int>( (x) + ((x) >= 0.0f ? 0.5f : -0.5f) ) )
#define lround(x) ( static_cast<long int>( (x) + ((x) >= 0.0 ? 0.5 : -0.5) ) )
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define STACKVAR(type, varname, count) type *varname=reinterpret_cast<type *>(_alloca(sizeof(type) * (count)))
#else
#include <math.h>
#define STACKVAR(type, varname, count) type varname[count]
#define CopyMemory(dst,ptr,len) memcpy(dst,ptr,len)
#define ZeroMemory(ptr,len) memset(ptr, 0, len)
#define __cdecl
typedef WId HWND;
#include <arpa/inet.h>
#endif

#if defined(__MMX__) || defined(Q_OS_WIN)
#include <mmintrin.h>
#endif

#define iroundf(x) ( static_cast<int>(x) )

#ifdef USE_BONJOUR
#include <dns_sd.h>
#endif

#ifdef __OBJC__
 #define nil 0
#endif

#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/ssl.h>
/* OpenSSL defines set_key. This breaks our protobuf-generated setters. */
#undef set_key

#endif
#endif
