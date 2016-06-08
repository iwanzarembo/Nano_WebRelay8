/*
	Nano_WebRelay8_OpenHab.ino - Sketch for Arduino Nano with ATmega328 
	implementation to control relays via HTTP requests.
	Copyright (c) 2015 Iwan Zarembo <iwan@zarembo.de>
	All rights reserved.

	It is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	
	Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
	der GNU General Public License, wie von der Free Software Foundation,
	Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
	veröffentlichten Version, weiterverbreiten und/oder modifizieren.
	
	Dieses Programm wird in der Hoffnung, dass es nützlich sein wird, aber
	OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
	Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
	Siehe die GNU General Public License für weitere Details.
	
	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.
  */

/**
 * This sketch provides a small webserver to control releays connected to your Arduino Nano with ATmega328.
 * 
 * What the sketch does:
 * It is a small web server which listens to http requests to (de-)activate a certain relay but the result
 * is openHab compatible.
 * 
 * Features:
 * - Initial GET request returns a JSON array with the current status of the relay.
 *   e.g.  {"r":["OFF","ON","OFF","OFF","OFF","OFF","OFF","OFF"]}
 *   Due to performance reasons only the status is returned. But you can see that the first  entry is for
 *   the first relay, the second for the second and so on.
 * - Only POST requests can change the status of the relays. The request data must follow the 
 *   pattern: relay number = 0 or 1 or 2 => e.g. 0=1&1=2&2=1&3=0&4=1&5=1&6=1&7=1
 *   -> The relay number can only be an integer between 0 and 7
 *   -> 0 turns the relay off
 *   -> 1 turns the relay on
 *   -> 2 changes the status of the relay. So a turned off relay would be switched on and the other way around
 * - A GET request to /about will show a JSON with the version of the sketch running on the board.
 *   e.g.  {"version" : "0.2.1" }
 *   
 * Additionally it is implementing a little bit of security like the maximum request size (default 
 * set to 480 bytes).
 * 
 * Hardware and Libraries used:
 * - An Ethernet Shield with the ENC28J60 chip, which requires the library
 *   See https://github.com/ntruchsess/arduino_uip or http://playground.arduino.cc/Hardware/ArduinoEthernet
 * - An 8 Kanal 5V Relay Module 
 * 
 * Error messages:
 * The application only returns error codes and a HTTP 500 error to save the already very limited memory. 
 * - 1 = The request is too big, please send one with less bytes or check the maxSize constant to increase the value.
 * - 2 = Only GET and POST is supported, but something else has been received.
 * - 3 = The command must be at least 3 chars long! e.g. 0=1
 * - 4 = The application only supports relay numbers between 0 and the value in numRelays. Change it if you have more relays.
 * - 5 = The application only supports the values 0 for off, 1 for on and 2 to invert the status, everything else will not work!
 * - 6 = The POST data does not follow the required pattern, see the description in the features section.
 *
 * Found a bug? Please create an issue at https://github.com/iwanzarembo/Nano_WebRelay8
 * 
 * The sketch was developed and build at codebender.cc and can be accessed at https://codebender.cc/sketch:321260
 * 
 * Known issues
 * 
 * - My Requst never returns
 *   Sometimes the Webserver just does not stop the client, I do not know why. But if you resend the request, then it will 
 *   work again, which means you should always call with a timeout!
 * 
 * created 04 Jan 2015 - by Iwan Zarembo
 * 
 * Version History:
 * - 0.2 2015/01/04
 *   * First version with usage of Strings
 * - 0.3 2015/01/21
 *   * Enhanced code without usage of strings
 *   * Additional error handling
 *   * Reduced the maximum request size
 * - 0.3.OPENHAB 2016/06/08
 *   * Return a JSON String which can be directly used by OpenHab
 *   * Read query string also in POST request (in that case the real post data is ignored!)
 */

// the debug flag during development
// de-comment to enable debugging statements
//#define DEBUGGING

#include <Arduino.h>
#include <UIPEthernet.h>

#ifdef DEBUGGING
#include <MemoryFree.h>
#endif

/*** The configuration of the application ***/
// Change the configuration for your needs
const uint8_t mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const IPAddress myIP(192, 168, 178, 21);

// Number of relays on board, max 9 are supported!
const int numRelays = 8;
// Output ports for relays, change it if you connected other pins, must be adjusted if number of relays changed
const int outputPorts[] = { 2, 3, 4, 5, 6, 7, 8, 9 }; 
// this is for performance, must be adjusted if number of relays changed
boolean portStatus[] = { false, false, false, false, false, false, false, false };

/****** a little bit of security *****/
// max request size, arduino cannot handle big requests so set the max you really plan to process
const int maxSize = 480; // that is enough for 8 relays + a bigger header

/****** General constants required by the application. DO NOT CHANGE ******/
const char* VERSION = "{\"version\":\"0.3.OPENHAB\"}";

// the get requests the application is listening to
const char* REQ_ABOUT = "/about";

// Request parameters
const char* GET  = "GET";
const char* POST = "POST";

// http codes
const int RC_ERR = 500;
const int RC_OK  = 200;

// supported relay status
const int R_OFF = 0;
const int R_ON  = 1;
const int R_INV = 2;

/* ************ internal constants *************/
// post data tokenizer
const int TOK_AND = '&';
const int TOK_EQU = '=';

// the max length of a relay command e.g. 1=2 or (if more than 9 relais) 11=2
const int RCL = 2 + (numRelays > 9 ? 2 : 1);

// new line
const char NL = '\n';
// carriage return
const char CR = '\r';
// JSON string start
const char JSS = '"';

// open hab states
const char* OH_ON = "ON";
const char* OH_OFF = "OFF";

// response of relay status, json start
const char* RS_START = "{\"r\":[";
// response of relay status, json end
const char* RS_END = "]}";
// response of relay status, json array separator;
const char RS_SEP = ',';

// response of the error JSON start
const char* RS_ERR_START = "{\"e\":";
// response of the error JSON end
const char RS_ERR_END = '}';

const char* HD_START = "HTTP/1.1 ";
const char* HD_END = " \nContent-Type: application/json\nConnection: close\n\n";

// start the server on port 80
EthernetServer server = EthernetServer(80);

void setup() {
	#ifdef DEBUGGING
		Serial.begin(115200);
	#endif
	// all pins will be turned off
	for (int i = 0; i < numRelays; i = i + 1) {
		pinMode(outputPorts[i], OUTPUT);    
		digitalWrite(outputPorts[i], HIGH); 
	}

	Ethernet.begin(mac, myIP);
	#ifdef DEBUGGING
		Serial.print("Local IP: ");
		Serial.println(Ethernet.localIP());
	#endif
	server.begin();
}


void loop() {
	size_t size;
	if (EthernetClient client = server.available()) {
		if (client) {
			if((size = client.available()) > 0) {
				#ifdef DEBUGGING
					Serial.print("Mem bef: "); Serial.println(freeMemory());
				#endif
				// check the the data is not too big
				if(size > maxSize) {
					returnHeader(client, RC_ERR);
					returnErr(client, 1);
				} else {
					// read all data from the client
					uint8_t* msgConv = (uint8_t*) malloc(size+1);
					size = client.read(msgConv, size);
					char* msg = (char*) msgConv;
					// set the string terminator
					*(msg+size) = 0;
					
					#ifdef DEBUGGING
						Serial.write(msgConv, size);
						Serial.print("\nSize is: "); Serial.println(size);
					#endif
	
					// check the HTTP option used by client
					if(strncmp((char*)msg, GET, 3) == 0) {
						char *data = getGetData(msg, 4);
						#ifdef DEBUGGING
							Serial.print("GET req\nData:"); Serial.println(data);
						#endif
						
						returnHeader(client, RC_OK);
						// check if the client wants to know the application version.
						if(strcmp(data, REQ_ABOUT) == 0) {
							client.println(VERSION);
						} else {
							// return the status of all relays
							printRelayStatus(client);
						}
					} else if(strncmp(msg, POST, 4) == 0) {
						#ifdef DEBUGGING
							Serial.println("POST req");
						#endif
						
						//extract the post data from the client message
						char *lnl = NULL; // last line
						char *src = msg-1; // first call begins with address msg
						char *end = msg+size; // the end of the message
						
						// the data in openhab is usually send as a query string
						// the first line must look like: POST /?0=2 HTTP/1.1
						// the question mark is at 6
						if(msg[6] == '?') {
							// ok, now search for the next space, which ends the query string
							char * spaceLocation = (char*) memchr(src+7, ' ', end-src);
							if (spaceLocation!=NULL) {
								int pos = spaceLocation-src;
								#ifdef DEBUGGING
									Serial.print("Cal pos: "); Serial.println(pos);
								#endif
								end = src+pos;
								src[pos] = 0;
								lnl = src+7;
							} else {
								returnHeader(client, RC_ERR);
								returnErr(client, 6);
							}
						} else {
							do {
								lnl = src;
								// read to the end of the next line, but max to the end of the msg.
								src = (char*) memchr(src+1, '\n', end-src);
								#ifdef DEBUGGING
									Serial.print("Msg left:"); Serial.println(src);
								#endif
							}while(src);
						}
						// +1 is to remove the line break before.
						changeRelayStatus(client, lnl+1, end);
					} else {
						// return an error, because only POST and GET are supported
						returnHeader(client, RC_ERR);
						returnErr(client, 2);
					}
					free(msg);
				} // end if size > maxSize
			} // end if size > 0
		} // end if client
		// required for the loop timing
		delay(2);
		client.stop();
		#ifdef DEBUGGING
			Serial.print("Memory after processing: "); Serial.println(freeMemory());
		#endif
	} // end if server available
}

/**
 * Returns an array pointer which contains the data from the given char array including the
 * check where the array should start and how many chars before it should end.
 */
char *getGetData(char *charArr, int start) {
    char* beg = charArr+start;
    char* end = (char*)memchr(beg, ' ', 10);
    if(end)
	    *end = 0;
    return beg;
}

/**
 * Performs the status change on the relais. The POST data must follow the 
 * pattern: relay number = 0 or 1 or 2 => e.g. 0=1&1=2&2=1&3=0&4=1&5=1&6=1&7=1
 *   -> The relay number can only be an integer between 0 and the constant numRelays-1
 *   -> 0 turns the relay off
 *   -> 1 turns the relay on
 *   -> 2 changes the status of the relay. So a turned off relay would be switched on and the other way around.
 * The result of this method can be:
 * 	1. A HTTP 500 error if something went wrong processing the command.
 * 	2. A JSON String as you would get when sending a simple GET request.
 */
void changeRelayStatus(EthernetClient &client, char *command, char *msgEnd) {
	#ifdef DEBUGGING
		Serial.print("data: '"); Serial.print(command); Serial.println("'");
	#endif
	// the command string needs to be terminated with the null byte.
	*(command+(size_t)msgEnd) = 0;
	
	// check the length of the whole command
  	int length = msgEnd - command;
  	#ifdef DEBUGGING
  		Serial.print("len:"); Serial.println(length);
  		Serial.print("cmd after null:"); Serial.println(command);
  	#endif
  	if(length < 3) {
  		returnHeader(client, RC_ERR);
		returnErr(client, 3);
	  	return;
  	}

  	// extract the post data from the client message
	char *lastSrc = NULL; // what is left from the command
	char *src = command; // first call starts with address of the command

	do {
		int srcNum = msgEnd-src;
		lastSrc = src;
		// read to the end of the next line, but max to the end of the msg.
		src = (char*) memchr(src+1, TOK_EQU, srcNum);
		if(!src) {
			// if there is no = char in the command left, then something is wrong!
			returnHeader(client, RC_ERR);
			returnErr(client, 6);
		  	return;
		}
		int relLen = src - lastSrc;
		// check the lenght of the relay number
		if( !(relLen <= RCL) ) {
  			returnHeader(client, RC_ERR);
			returnErr(client, 4);
		  	return;
  		}
  		// convert the value into a number
  		char* relNumChar;
  		memcpy(relNumChar, lastSrc, relLen);
  		int relNum = atoi( relNumChar );
  		#ifdef DEBUGGING
  			Serial.print("rel#:"); Serial.println(relNum);
  		#endif
  		// check if the relay number is in valid range
  		if( !(relNum < numRelays && relNum >= 0) ) {
  			// error stop processing!!!
			returnHeader(client, RC_ERR);
			returnErr(client, 4);
			return;
  		}
  		
  		// reassign last search and remove the = char
  		lastSrc = src + 1;
  		// search for the next & sign and convert the value
  		// -1 to remove the last finding
  		srcNum = msgEnd-src-1;
  		src = (char*) memchr(src+2, TOK_AND, srcNum);
  		// if there is no & then it means the command left is the status for the relay.
  		int statLen = src ? (src - lastSrc) : srcNum;
		// check the lenght of the relay number
		if( statLen != 1 ) {
  			returnHeader(client, RC_ERR);
			returnErr(client, 5);
		  	return;
  		}
  		
  		// convert the status;
		char* statChar;
  		memcpy(statChar, lastSrc, statLen);
  		int stat = atoi( statChar );
  		#ifdef DEBUGGING
  			Serial.print("Status:"); Serial.println(stat);
  		#endif
  		
  		// Only change the status if it has the correct value
  		switch(stat) {
  			case R_ON:
  				// only turn something of if it is turn on, otherwise ignore it.
				if(portStatus[relNum] != true) {
					// turn it off
					digitalWrite(outputPorts[relNum], LOW);
					portStatus[relNum] = true;
				}
  				break;
  			case R_OFF:
  				// only turn something of if it is turn on, otherwise ignore it.
				if(portStatus[relNum] != false) {
					// turn it off
					digitalWrite(outputPorts[relNum], HIGH);
					portStatus[relNum] = false;
				}
  				break;
  			case R_INV:
				digitalWrite(outputPorts[relNum], !portStatus[relNum] ? LOW : HIGH);
				portStatus[relNum] = !portStatus[relNum];
  				break;
  			default:
	  			// error stop processing!!!
				returnHeader(client, RC_ERR);
				returnErr(client, 5);
				return;
  		}
  		
  		
	} while(src);
	returnHeader(client, RC_OK);
  	// return the final status of all relays
	printRelayStatus(client);
}

/**
 * Returns a JSON with the current values of the relays to the client. The 
 * JSON will look like: {"r":["OFF","OFF","OFF","OFF","OFF","OFF","OFF","OFF"]}
 * This one means all releays are turned off.
 */
void printRelayStatus(EthernetClient &client) {
	client.print(RS_START);
	int lastRelay = numRelays-1;
	// dump openHab only supports strings!
	for (int i = 0; i < numRelays; i++) {
		client.print(JSS);
		client.print(portStatus[i] ? OH_ON : OH_OFF);
		client.print(JSS);
		if(i < lastRelay) {
		  client.print(RS_SEP);
		}
	}
	client.println(RS_END);
}

/**
 * Returns a message to the client.
 */
void returnErr(EthernetClient &client, int rc) {
	#ifdef DEBUGGING
		Serial.print("Returning error: ");
		Serial.println(rc);
	#endif
	client.print(RS_ERR_START);
	client.print(rc);
	client.println(RS_ERR_END);
}

/**
 * Returns a header with the given http code to the client.
 */
void returnHeader(EthernetClient &client, int httpCode) {
	client.print(HD_START);
	client.print(httpCode);
	client.print(HD_END);
}
