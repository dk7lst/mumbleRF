#!/usr/bin/env python

import sys
import json
try:
	from numpy import array
except:
	print 'Stat crunching requires numpy - skipping.'
	sys.exit(0)

def load_results(kind):
	results = []
	for i in range(0, 10):
		f = open('results/%s.%i' % (kind, i), 'r')
		s = f.read()
		results.append(json.loads(s)['elapsed_usec'])
	return results

if __name__ == '__main__':
	celt = array(load_results('celt'))
	sbcelt_futex = array(load_results('sbcelt-futex'))
	sbcelt_rw = array(load_results('sbcelt-rw'))

	print '# results (niter=10000)'
	print 'sbcelt-futex  %.2f usec (%.2f stddev)' % (sbcelt_futex.mean(), sbcelt_futex.std())
	print 'sbcelt-rw     %.2f usec (%.2f stddev)' % (sbcelt_rw.mean(), sbcelt_rw.std())
	print 'celt          %.2f usec (%.2f stddev)' % (celt.mean(), celt.std())
	print ''
	print 'sbcelt-futex delta: %.2f usec' % (celt.mean() - sbcelt_futex.mean())
	print 'sbcelt-rw detlta:   %.2f usec' % (celt.mean() - sbcelt_rw.mean())
