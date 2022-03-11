# GrugNTPClient
Simple, dumb, effective, asynchronous NTP client for ESP32 or anything with sockets.

This is a rewrite of https://github.com/arduino-libraries/NTPClient

* Dependencies

Timeout class (https://github.com/peufeu2/ESP32FastMillis)

log() function with the same prototype as printf. You probably have one already, otherwise...

    #define log Serial.printf

* Features

GrugNTPClient uses sockets. Arduino UDP library is not necessary, which reduces the footprint. This client can be used on other platforms, by removing the ESP32 specific call to check if WiFi is connected.

GrugNTPClient tries to be as asynchronous as possible to work well with cooperative multitasking (ie, coroutines). Its only blocking action is to resolve the NTP server name. After that the server IP address is cached. Then, a non-blocking socket is used to send and receive UDP packets.

* How to use

Initiaization code:

    #define NTP_OFFSET   1 * 60*60    		// Time zone, In seconds
    #define NTP_INTERVAL 60 * 60000   		// In miliseconds
    #define NTP_INTERVAL_RETRY 5 * 1000  	// In miliseconds
    #define NTP_INTERVAL_NONET 60 * 1000  	// In miliseconds
    #define NTP_ADDRESS  "fr.pool.ntp.org"

Global variable:

    GrugNTPClient timeClient( NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL, NTP_INTERVAL_RETRY );

setup():

    timeClient.begin();

In a loop() or coroutine:

    timeClient.startUpdate();
	  timeClient.receiveUpdate() 

startUpdate() resolves the NTP pool server name into its IP address, which is then cached.
If WiFi is not connected, this will wait and timeout, so it is better to wait until WiFi
is connected to call it.

startUpdate() sends a packet to the server.

receiveUpdate() looks for a received packet and processes it. Il will also call setTime()
and keep the first received time as boot_timestamp.

If the NTP packet is successfully received, startUpdate() will send the next
packet after NTP_INTERVAL ms. If it failed, the next packet will be sent
after a shorter NTP_INTERVAL_RETRY ms instead.

