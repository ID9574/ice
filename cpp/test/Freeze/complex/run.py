#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2001
# MutableRealms, Inc.
# Huntsville, AL, USA
#
# All Rights Reserved
#
# **********************************************************************

import os, sys

for toplevel in [".", "..", "../..", "../../..", "../../../.."]:
    toplevel = os.path.normpath(toplevel)
    if os.path.exists(os.path.join(toplevel, "config", "TestUtil.py")):
        break
else:
    raise "can't find toplevel directory!"

sys.path.append(os.path.join(toplevel, "config"))
import TestUtil

name = os.path.join("Freeze", "complex")

testdir = os.path.join(toplevel, "test", name)

#
# Clean the contents of the database directory.
#
dbdir = os.path.join(testdir, "db")
TestUtil.cleanDbDir(dbdir)

client = os.path.join(testdir, "client")
clientOptions = " --dbdir " + testdir;

print "starting populate...",
populatePipe = os.popen(client + clientOptions + " populate")
output = populatePipe.read().strip()
if not output:
    print "failed!"
    sys.exit(1)
print "ok"
print output

populateStatus = populatePipe.close()

if populateStatus:
    sys.exit(1)

print "starting verification client...",
clientPipe = os.popen(client + clientOptions + " validate")
output = clientPipe.read().strip()
if not output:
    print "failed!"
    sys.exit(1)
print "ok"
print output

clientStatus = clientPipe.close()

if clientStatus:
    sys.exit(1)

sys.exit(0)
