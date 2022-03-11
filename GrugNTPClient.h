/**
  
  		GrugNTPClient is simple, dumb, asynchronous, and effective.

  	It uses ESP32 sockets instead of arduino UDP library.
  
	startUpdate(); sends the NTP packet when needed.

	It does not wait for the NTP UDP reply, because that sort of delay
	would clog up AceRoutine cooperative multitasking. Instead, 

	receiveUpdate() 

	should be called periodically. It checks the UDP 
	socket and processes the response, if it received.

	It will only process the received packet whenever receiveUpdate() is called,
	which adds a small delta t to the NTP update. This isn't a problem for
	most applications.

	If the NTP packet is successfully received, startUpdate() will send the next
	packet after NTP_INTERVAL ms. If it failed, the next packet will be sent
	after a shorter NTP_INTERVAL_RETRY ms instead.


 * Required libraries:
 * 		a log() function, same prototype as printf()
 * 		Timeout class
 * 
 * Just put this in your code:

#define NTP_OFFSET   1 * 60*60    		// Time zone, In seconds
#define NTP_INTERVAL 60 * 60000   		// In miliseconds
#define NTP_INTERVAL_RETRY 5 * 1000  	// In miliseconds
#define NTP_INTERVAL_NONET 60 * 1000  	// In miliseconds
#define NTP_ADDRESS  "fr.pool.ntp.org"

 	global variable:
		GrugNTPClient timeClient( NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL, NTP_INTERVAL_RETRY );

	setup():
		timeClient.begin();

	startUpdate() resolves the NTP pool server name into its IP address, which is then cached.
	If WiFi is not connected, this will wait and timeout, so it is better to wait until WiFi
	is connected to call it. So just put this somewhere in a loop or coroutine:

		timeClient.startUpdate();
		timeClient.receiveUpdate();

 * 
 * The MIT License (MIT)
 * Modifications (c) 2022 by peufeu
 * Original NTP client Copyright (c) 2015 by Fabrice Weinberg
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

#pragma once

class GrugNTPClient {
	protected:
		static const uint32_t SEVENZYYEARS 				= 2208988800UL;
		static const uint32_t NTP_PACKET_SIZE 			= 48;
		static const uint32_t NTP_DEFAULT_LOCAL_PORT 	= 1337;
		static const int	  MAX_DNS_ERRORS			= 10;

		const char*         _serverName = "pool.ntp.org";	// Default time server
		struct sockaddr_in  _serverIP   = {};				// Cache IP address
		unsigned            _port       = NTP_DEFAULT_LOCAL_PORT;
		int                 _sockfd     = -1;

		Timeout		timeout;
		int32_t  _timeOffset     = 0;
		uint32_t _updateInterval = 60000;	// ms
		uint32_t _retryInterval  = 10000;	// ms
		uint32_t _errorInterval  = 3600000;	// ms
		uint32_t _lastUpdate     = 0;		// ms
		uint32_t _currentEpoc    = 0;		// s
		int		 _errorCounter	 = 0;

		byte	_packetBuffer[NTP_PACKET_SIZE];
		bool	receiveNTPPacket();
		bool	err( const char* s );
		void	closeSocket();	// close socket to save preciousss memory

	public:
		/**
		 * Will be initialized at first successful NTP query
		 */
		char	boot_timestamp_iso[20] = "";		// formatted like "2022-03-06T07:58:24"
		time_t	boot_timestamp = 0;					// just a time_t

		GrugNTPClient( const char* poolServerName, int32_t timeOffset, uint32_t updateInterval, uint32_t retryInterval = 10000, uint32_t errorInterval = 3600000 );

		/**
		 * Just sets the port.
		 * UDP socket will be allocated later when needed.
		 */
		void begin( unsigned int port = NTP_DEFAULT_LOCAL_PORT );

		/**
		 * Stops everything, frees socket etc.
		 */
		void end();

		/**
		 * Asynchronous process: call startUpdate() periodically to send NTP packet.
		 * Packet will only be sent at update/retry intervals.
		 * @return true if packet was sent, false if it was not, due to error or interval not expired.
		 */
		bool startUpdate();

		/**
		 * Force send NTP packet. Must call receiveUpdate() to process the response packet.
		 * @return true on success, false on failure
		 */
		bool forceUpdate();

		/**
		 * Receive NTP packet, if any.
		 * @return true if packet was received and processed.
		 */
		bool receiveUpdate();

		/**
		 * This allows to check if the NTPClient successfully received a NTP packet and set the time.
		 * @return true if time has been set, else false
		 */
		bool isTimeSet() const;

		/**
		 * Changes the time offset. Useful for changing timezones dynamically
		 */
		void setTimeOffset( int timeOffset );

		/**
		 * Set the update interval to another frequency. E.g. useful when the
		 * timeOffset should not be set in the constructor
		 */
		void setUpdateInterval( uint32_t updateInterval, uint32_t retryInterval = 5000, uint32_t errorInterval = 3600000 );

		/**
		 * Set the NTP server name, and resets its cached IP address
		 */
		void setPoolServerName(const char* poolServerName);

		/**
		 * @return time in seconds since Jan. 1, 1970
		 */
		uint32_t getEpochTime() const;
};
