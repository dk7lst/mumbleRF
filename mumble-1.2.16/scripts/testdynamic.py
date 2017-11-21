#!/usr/bin/env python
# -*- coding: utf-8
import Ice, IcePy, sys, tempfile, os

ice = Ice.initialize(sys.argv)

proxy = ice.stringToProxy('Meta:tcp -h 127.0.0.1 -p 6502')

try:
  slice = IcePy.Operation('getSlice', Ice.OperationMode.Idempotent, Ice.OperationMode.Idempotent, True, (), (), (), IcePy._t_string, ()).invoke(proxy, ((), None))

  (slicefiledesc, slicefilepath)  = tempfile.mkstemp(suffix = '.ice')
  slicefile = os.fdopen(slicefiledesc, 'w')
  slicefile.write(slice)
  slicefile.flush()
  Ice.loadSlice('', ['-I' + Ice.getSliceDir(), slicefilepath])
  slicefile.close()
  os.remove(slicefilepath)
  
  print 'Using dynamic slice'
except Exception:
  Ice.loadSlice('', ['-I' + Ice.getSliceDir(), 'Murmur.ice'])
  print 'Using bundled slice'

import Murmur

meta = Murmur.MetaPrx.checkedCast(proxy)

print meta.getVersion()
