 
 /**
 * The MIT License (MIT)
 * Modifications (c) 2022 by peufeu
 * Original NTP client by Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Arduino.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <WiFi.h>
#include <TimeLib.h>

#include "config.h"
#include "timeout.h"

#include "GrugNTPClient.h"

GrugNTPClient::GrugNTPClient( const char* poolServerName, int32_t timeOffset, 
								uint32_t updateInterval, uint32_t retryInterval,
								uint32_t errorInterval ) {
	_serverName 	= poolServerName;
	_timeOffset     = timeOffset;
	_updateInterval = updateInterval;
	_retryInterval 	= retryInterval;
	_errorInterval  = errorInterval;
}

void GrugNTPClient::begin( unsigned port ) {
	_port = port;
}

// log error and clean everythign up
bool GrugNTPClient::err( const char* s )  {
	log("NTP: %s %d %s", s, errno, strerror(errno) ); 
	closeSocket();
	return false; 
}

bool GrugNTPClient::receiveNTPPacket() {
	if( _sockfd<0 ) return false;
	struct sockaddr_in source_addr;
	socklen_t source_addr_len = sizeof( source_addr );
	int len = recvfrom( _sockfd, _packetBuffer, sizeof(_packetBuffer), 0, 
						(struct sockaddr *)&source_addr, &source_addr_len );
	return len == NTP_PACKET_SIZE;
}

bool GrugNTPClient::forceUpdate() {

	if( WiFi.status()!= WL_CONNECTED )		// No point if we don't have internet
		return false;

	// got server IP address?
	if( !_serverIP.sin_addr.s_addr  ) {
	    struct hostent *hostnm = gethostbyname( _serverName );
	    if( !hostnm ) {
	    	if( _errorCounter < MAX_DNS_ERRORS ) {		// If we can't resolve the server name,
	    		_errorCounter++;						// it could mean the config is wrong or has a typo,
	    		timeout.set( _retryInterval );			// or it could mean we have wifi but no DNS.
	    	} else 										// We don't want to block here at every call
	    		timeout.set( _errorInterval );			// thus after MAX_DNS_ERRORS we give up and
	    	return err("gethostbyname");				// use a much higher timeout value.
	    }
		_errorCounter = 0;
		_serverIP.sin_family      = AF_INET;
	    _serverIP.sin_port        = htons( 123 );
	    _serverIP.sin_addr.s_addr = *((uint32_t*)hostnm->h_addr);

	    byte *p = (byte*) &_serverIP.sin_addr.s_addr;
		log("NTP: server %d.%d.%d.%d", p[0],p[1],p[2],p[3] );
	}
	
	// got socket?
	if( _sockfd < 0 ) {
		_sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    	if( _sockfd < 0 ) 
    		return err( "socket" );

        struct sockaddr_in listen_addr;
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        listen_addr.sin_port = htons( _port );

        if( bind( _sockfd, (const struct sockaddr *)&listen_addr, sizeof(listen_addr) ) < 0 ) 
        	return err("bind");

	    // set to non blocking
		int flags = fcntl( _sockfd, F_GETFL );
		if( flags<0 || fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK)<0 ) 
			return err("fcntl");
	}

	// flush any previously received packets
	while( receiveNTPPacket() ) ;

	// set all bytes in the buffer to 0
	memset( _packetBuffer, 0, NTP_PACKET_SIZE );
	_packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	_packetBuffer[1] = 0;     // Stratum, or type of clock
	_packetBuffer[2] = 6;     // Polling Interval
	_packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	_packetBuffer[12]  = 49;
	_packetBuffer[13]  = 0x4E;
	_packetBuffer[14]  = 49;
	_packetBuffer[15]  = 52;

    int r = sendto( _sockfd, (const char *)_packetBuffer, sizeof(_packetBuffer), 0, 
    				(const struct sockaddr *) &_serverIP, sizeof(_serverIP));
    if( r != NTP_PACKET_SIZE ) 
    	return err( "sendto" );

    log("NTP: sent");
	timeout.set( _retryInterval );
    return true;
}

bool GrugNTPClient::startUpdate() {
	if( !timeout.expired() )
		return false;
	return forceUpdate();
}

bool GrugNTPClient::receiveUpdate() {
	if( !receiveNTPPacket() )
		return false;

	_lastUpdate = fastmillis();

	unsigned long highWord = word(_packetBuffer[40], _packetBuffer[41]);	// combine the four bytes (two words) into a long integer
	unsigned long lowWord  = word(_packetBuffer[42], _packetBuffer[43]);	// this is NTP time (seconds since Jan 1 1900):
	unsigned long secsSince1900 = highWord << 16 | lowWord;
	_currentEpoc = secsSince1900 - SEVENZYYEARS;

	closeSocket();	// conserve memory

	time_t t = getEpochTime();		// set time
	setTime( t );
	char buf[20];
	struct tm *lt = localtime(&t);
	strftime( buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", lt ); 
	log( "NTP: set %s", buf );
	if( !boot_timestamp_iso[0] ) {	// set boot_timestamp
		strcpy( boot_timestamp_iso, buf ); 
		boot_timestamp = t;
	}

	timeout.set( _updateInterval );	// Success. Switch to longer update interval.
	return true;
}

bool GrugNTPClient::isTimeSet() const {
	return _currentEpoc; // returns true if the time has been set, else false
}

uint32_t GrugNTPClient::getEpochTime() const {
	return 	  _timeOffset  // User offset
			+ _currentEpoc // Epoch returned by the NTP server
			+ ((fastmillis()-_lastUpdate) / 1000); // Time since last update
}

void GrugNTPClient::closeSocket() {
	if( _sockfd>=0 ) {
		close( _sockfd );
		_sockfd = -1;
	}	
}

void GrugNTPClient::end() {
	closeSocket();
	_serverIP.sin_addr.s_addr  = 0;
}

void GrugNTPClient::setTimeOffset(int timeOffset) {
	_timeOffset     = timeOffset;
}

void GrugNTPClient::setUpdateInterval( uint32_t updateInterval, uint32_t retryInterval, uint32_t errorInterval ) {
	_updateInterval = updateInterval;
	_retryInterval = retryInterval;
	_errorInterval = errorInterval;
}

void GrugNTPClient::setPoolServerName(const char* poolServerName) {
	_serverName = poolServerName;
	_serverIP.sin_addr.s_addr  = 0;
}

