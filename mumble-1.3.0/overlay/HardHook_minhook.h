// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_HARDHOOK_MINHOOK_H_
#define MUMBLE_HARDHOOK_MINHOOK_H_

#define _UNICODE
#ifndef _WIN32_WINNT
#define  _WIN32_WINNT 0x0501
#endif
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <windows.h>
#include <math.h>
#include <map>
#include <vector>
#include <string>

typedef void *(*voidFunc)();

struct HardHook {
	voidFunc  m_func;
	voidFunc  m_replacement;
	voidFunc  call;

	HardHook();
	HardHook(voidFunc func, voidFunc replacement);
	void setup(voidFunc func, voidFunc replacement);
	void setupInterface(IUnknown *intf, LONG funcoffset, voidFunc replacement);
	void reset();
	void inject(bool force = false);
	void restore(bool force = false);
	void print();
	void check();
};

#endif
