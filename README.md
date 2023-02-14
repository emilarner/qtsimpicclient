# qtsimpicclient
A GUI interface to Simpic servers made in Qt C++, ideally for Linux.

Version: 1.1

![Picture of qtsimpicclient](https://i.ibb.co/3MTFmnD/2023-02-13-235428-1366x768-scrot.png)

This program either connects to an existing Simpic server or it creates one on the local machine that it is running on, but only if that same local machine is not currently running a Simpic server, due to synchronization issues between two simultaneous Simpic instances running at once. It allows one to scan the server--and by extension, the machine that the server is running on--for duplicate images/media files in a specified directory. This program is made in Qt and the source code is--well--quite unappealing, but it works.

To select images for removal once you have them in a set, just click on them and a red border around them will indicate they will be removed (moved to Simpic's recycling bin, not actually deleted). Then, click next for the process to be done and for the next set of images to come through. If clicking next yields a blank screen, then there are no more image/media files that Simpic had found. You may also view characteristics of the image/media files by hovering over them, revealing tooltip information. If you are generally confused about something that this program has, the tooltip information yielded on hovering over something should usually be of great assistance. 

If you want to open images in a browser or in your main image viewer, then right-clicking on them will execute `xdg-open` on them, where they will be stored as random files in /tmp/. This will open them in the default image viewer for your system. 

Dependencies: 
- Qt5 C++ libraries. 
- libsimpicclient, can be found [here](https://github.com/emilarner/simpic_client).
- libsimpicserver, can be found [here](https://github.com/emilarner/simpic-server).
- pHash (impossible to compile for Windows, hard to compile for Mac OS)
- OpenSSL: *libcrypto* and *libssl*
- libjpeg
- libffmpeg
- libtiff
- libpng

To build and install, do the following:
 - When you clone this directory, cd into it and run `cmake .`
 - Then run `make`
 - To install (only if desired), then run `sudo make install`