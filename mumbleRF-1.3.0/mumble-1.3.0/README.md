Mumble - A voice chat utility for gamers
=======================================

[![Build Status](https://dev.azure.com/Mumble-VoIP/Mumble/_apis/build/status/Mumble-CI?branchName=master)](https://dev.azure.com/Mumble-VoIP/Mumble/_build/latest?definitionId=1?branchName=master)

> *http://mumble.info/*  
> *#mumble on freenode*

Mumble is a voicechat program for gamers written on top of Qt and Opus.

There are two modules in Mumble; the client (mumble) and the server
(murmur). The client works on Win32/64, Linux and Mac OS X, while the
server should work on anything Qt can be installed on.

Note that when we say Win32, we mean Windows XP or newer.

## Running Mumble

On Windows, after installation, you should have a new Mumble folder in your
Start Menu, from which you can start Mumble.

On Mac OS X, to install Mumble, drag the application from the downloaded
disk image into your `/Applications` folder.

Once Mumble is launched, you need a server to connect to. Either create your
own or join a friend's.

## Running Murmur on Unix-like systems

Murmur should be run from the command line, so start a shell (command prompt)
and go to wherever you installed Mumble. Run murmur as

```
murmurd [-supw <password>] [-ini <inifile>] [-fg] [v]

-supw   Set a new password for the user SuperUser, which is hardcoded to
        bypass ACLs. Keep this password safe. Until you set a password,
        the SuperUser is disabled. If you use this option, murmur will
        set the password in the database and then exit.

-ini    Use an inifile other than murmur.ini, use this to run several instances
        of murmur from the same directory. Make sure each instance is using
        a separate database.

-fg     Run in the foreground, logging to standard output.

-v      More verbose logging.
```

## Running Murmur on Mac OS X

Murmur is distributed separately from the Mumble client on Mac OS X.
It is called Static OS X Server and can be downloaded from the main webpage.

Once downloaded it can be run in the same way as on any other Unix-like system.
For more information please see the 'Running Murmur on Unix-like systems' above.

## Running Murmur on Win32

Doubleclick the Murmur icon to start murmur. There will be a small icon on your
taskbar from which you can view the log.

To set the superuser password, run murmur with the parameters `-supw <password>`.

## Bandwidth usage

Mumble will use 10-40 kbit/s outgoing, and the same incoming for each user.
So if there are 10 other users on the server with you, your incoming
bandwidth requirement will be 100-400 kbit/s if they all talk at the same time.
