/*
socket.io-arduino-client: a Socket.IO client for the Arduino
Based on the Kevin Rohling WebSocketClient & Bill Roy Socket.io Lbrary
Copyright 2015 Florent Vidal
Supports Socket.io v1.x
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Modified by RoboJay
 */

#include <SocketIOClient.h>

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#else
#define ECHO(m)
#endif

#define ORIGIN F("Origin: ArduinoSocketIOClient\r\n")

SocketIOClient::SocketIOClient() {
    for (int i = 0; i < MAX_ON_HANDLERS; i++) {
        onId[i] = "";
    }
}

void SocketIOClient::setChannel(String newChannel) {
    channel = newChannel;
}

bool SocketIOClient::connect(String thehostname, int theport) {
    thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
    port = theport;

    if (handshake() && joinChannel() && connectViaSocket()) {
        return true;
    }

    return false;
}

bool SocketIOClient::handshake() {
    if (!beginConnect()) {
        return false;
    }

    ECHO(F("[handshake] Begin send Handshake"));
    ECHO("[handshake] Server: " + String(hostname));
    sendHandshake();

    if (!waitForInput()) {
        ECHO(F("[handshake] Time out"));
        return false;
    }

    ECHO(F("[handshake] Read Handshake"));
    return readHandshake();
}

bool SocketIOClient::readHandshake() {
    // check for happy "HTTP/1.1 200" response
    if (!checkResponseStatus(200)) {
        return false;
    }

    eatHeader();
    if (!readSid()) {
        return false;
    }

    stopConnect();
}

bool SocketIOClient::joinChannel() {
    ECHO(channel);
    if (channel.length() == 0) {
        return true;
    }

    if (!beginConnect()) {
        return false;
    }

    sendRequestJoinChannel();

    if (!waitForInput()) {
        ECHO(F("[joinChannel] Time out"));
        return false;
    }

    if (!checkResponseStatus(200)) {
        return false;
    }

    stopConnect();
}

void SocketIOClient::sendRequestJoinChannel() {
    ECHO("[sendRequestJoinChannel] Join channel: " + channel);
    String body = String(channel.length() + 30);
    body += ":42[\"subscribe\",{\"channel\":\"";
    body += channel;
    body += "\"}]";

    String request = "";
    request += F("POST /socket.io/?EIO=3&transport=polling&sid=");
    request += sid;
    request += F(" HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }

    request += ORIGIN;
    request += F("Content-Type: text/plain;charset=UTF-8\r\n");
    request += F("Content-Length: ");
    request += body.length();
    request += F("\r\n");
    request += F("Connexion: keep-alive\r\n\r\n");
    request += body;
    request += "\r\n\r\n";

    ECHO(F("\r\n[sendRequestJoinChannel] Send request........................."));
    ECHO(request);
    ECHO(F("[sendRequestJoinChannel] .........................send request done\r\n"));
    internets.print(request);

    // request += "43:42[\"subscribe\",{\"channel\":\"feed-the-fish\"}]\r\n\r\n";
}

bool SocketIOClient::connectViaSocket() {
    if (!beginConnect()) {
        return false;
    }

    ECHO(F("[connectViaSocket] Send connect to Websocket"));

    sendConnectToSocket();
    if (!waitForInput()) {
        return false;
    }

    if (!checkResponseStatus(101)) {
        ECHO(F("[connectViaSocket] Updrage to Websocket failed"));
        return false;
    }

    ECHO(F("[connectViaSocket] Updrage to Websocket successful"));
    readLine();
    readLine();
    readLine();
    for (int i = 0; i < 28; i++) {
        key[i] = databuffer[i + 22]; //key contains the Sec-WebSocket-Accept, could be used for verification
    }

    ECHO(F("[connectViaSocket] Read Sec-WebSocket-Accept key successful"));
    ECHO("[connectViaSocket] Key: " + String(key));

    eatHeader();

    /*
    Generating a 32 bits mask requiered for newer version
    Client has to send "52" for the upgrade to websocket
     */
    randomSeed(analogRead(0));
    String mask = "";
    String masked = "52";
    String message = "52";
    for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < message.length(); i++) {
        masked[i] = message[i] ^ mask[i % 4]; //apply the "mask" to the message ("52")
    }

    internets.print((char) 0x81); //has to be sent for proper communication
    internets.print((char) 130); //size of the message (2) + 128 because message has to be masked
    internets.print(mask);
    internets.print(masked);

    monitor(); // treat the response as input
    return true;
}

bool SocketIOClient::checkResponseStatus(int status) {
    // check for happy "HTTP/1.1 200" response
    readLine();
    if (atoi(&databuffer[9]) != status) {
        while (internets.available()) readLine();
        internets.stop();
        return false;
    }

    return true;
}

bool SocketIOClient::readSid() {
    readLine();
    String tmp = databuffer;

    int sidindex = tmp.indexOf("sid");
    if (sidindex < 0) {
        ECHO(F("[readSid] SID not exist"));
        return false;
    }

    int sidendindex = tmp.indexOf("\"", sidindex + 6);
    int count = sidendindex - sidindex - 6;

    for (int i = 0; i < count; i++) {
        sid[i] = databuffer[i + sidindex + 6];
    }

    ECHO("[readSid] SID: " + String(sid));
    return true;
}

bool SocketIOClient::beginConnect() {
    if (!internets.connect(hostname, port)) {
        ECHO(F("[beginConnect] Connect failed"));
        return false;
    }

    ECHO(F("[beginConnect] Connect successful"));
    return true;
}

bool SocketIOClient::stopConnect() {
    while (internets.available()) {
        readLine();
    }

    internets.stop();
    delay(100);
    ECHO(F("[stopConnect] Connect was stopped"));
    return true;
}

void SocketIOClient::sendConnectToSocket() {
    String request = "";
    request += F("GET /socket.io/1/websocket/?transport=websocket&b64=true&sid=");
    request += sid;
    request += F(" HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }

    request += ORIGIN;
    request += F("Sec-WebSocket-Key: ");
    request += sid;
    request += F("\r\n");
    request += F("Sec-WebSocket-Version: 13\r\n");
    request += F("Upgrade: websocket\r\n");
    request += F("Connection: Upgrade\r\n\r\n");

    ECHO(F("\r\n[sendConnectToSocket] Send request........................."));
    ECHO(request);
    ECHO(F("[sendConnectToSocket] .........................send request done\r\n"));
    internets.print(request);
}

bool SocketIOClient::connectHTTP(String thehostname, int theport) {
    thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
    port = theport;
    if (!internets.connect(hostname, theport)) {
        return false;
    }
    return true;
}

bool SocketIOClient::reconnect(String thehostname, int theport) {
    thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
    port = theport;

    if (!internets.connect(hostname, theport)) {
        return false;
    }

    sendHandshake();
    return readHandshake();
}

bool SocketIOClient::connected() {
    return internets.connected();
}

void SocketIOClient::disconnect() {
    internets.stop();
}

void SocketIOClient::eventHandler(int index) {
    String id = "";
    String data = "";
    String rcvdmsg = "";
    int sizemsg = databuffer[index + 1]; // 0-125 byte, index ok Fix provide by Galilei11. Thanks
    if (databuffer[index + 1] > 125) {
        sizemsg = databuffer[index + 2]; // 126-255 byte
        index += 1; // index correction to start
    }

    ECHO("[eventHandler] Message size = " + String(sizemsg));

    for (int i = index + 2; i < index + sizemsg + 2; i++) {
        rcvdmsg += (char) databuffer[i];
    }

    ECHO("[eventHandler] Received message = " + String(rcvdmsg));

    switch (rcvdmsg[0]) {
        case '2':
            ECHO("[eventHandler] Ping received - Sending Pong");
            heartbeat(1);
            break;
        case '3':
            ECHO("[eventHandler] Pong received - All good");
            break;
        case '4':
            switch (rcvdmsg[1]) {
                case '0':
                    ECHO("[eventHandler] Upgrade to WebSocket confirmed");
                    break;
                case '2':
                    rcvdmsg.replace("\\\\", "\\");
                    id = rcvdmsg.substring(4, rcvdmsg.indexOf("\","));

                    ECHO("[eventHandler] id = " + id);

                    data = rcvdmsg.substring(2);
                    ECHO("[eventHandler] data = " + data);

                    ECHO(onIndex);
                    for (uint8_t i = 0; i < onIndex; i++) {
                        ECHO(onId[i]);
                        if (id == onId[i]) {
                            ECHO("[eventHandler] Found handler = " + String(i));
                            (*onFunction[i])(data);
                        }
                    }
                    break;
            }
    }
}

bool SocketIOClient::monitor() {
    int index = -1;
    int index2 = -1;
    String tmp = "";
    *databuffer = 0;
    static unsigned long pingTimer = 0;

    if (!internets.connected()) {
        ECHO("[monitor] Client not connected.");
        if (connect(hostname, port)) {
            ECHO("[monitor] Connected.");
            return true;
        } else {
            ECHO("[monitor] Can't connect. Aborting.");
        }
    }

    // the PING_INTERVAL from the negotiation phase should be used
    // this is a temporary hack
    if (internets.connected() && millis() >= pingTimer) {
        heartbeat(0);
        pingTimer = millis() + PING_INTERVAL;
    }

    if (!internets.available()) {
        return false;
    }

    while (internets.available()) { // read availible internets
        readLine();
        tmp = databuffer;
        dataptr = databuffer;
        index = tmp.indexOf((char) 129); //129 DEC = 0x81 HEX = sent for proper communication
        index2 = tmp.indexOf((char) 129, index + 1);
        if (index != -1) {
            eventHandler(index);
        }
        if (index2 != -1) {
            eventHandler(index2);
        }
    }
    return false;
}

void SocketIOClient::sendHandshake() {
    ECHO(hostname);
    String request = "";
    request += F("GET /socket.io/?transport=polling&b64=true HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }
    request += F("Origin: Arduino\r\n");
    request += F("Connexion: keep-alive\r\n\r\n");
    ECHO(F("\r\n[sendHandshake] Send request........................."));
    ECHO(request);
    ECHO(F("[sendHandshake] .........................send request done\r\n"));
    internets.print(request);
}

bool SocketIOClient::waitForInput(void) {
    unsigned long now = millis();
    while (!internets.available() && ((millis() - now) < 30000UL)) {
        ;
    }
    return internets.available();
}

void SocketIOClient::eatHeader(void) {
    while (internets.available()) { // consume the header
        readLine();
        if (strlen(databuffer) == 0) {
            break;
        }
    }
}

void SocketIOClient::getREST(String host, String path) {
    String request = "";
    request += F("GET /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Connection: close\r\n\r\n");

    ECHO(F("\r\n[getREST] Send request........................."));
    ECHO(request);
    ECHO(F("[getREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::postREST(String host, String path, String type, String data) {
    String request = "";
    request += F("POST /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Content-Length: ");
    request += data.length();
    request += F("\r\n");
    request += F("Content-Type: ");
    request += type;
    request += F("\r\n");
    request += F("Connection: close\r\n\r\n");
    request += data;
    request += F("\r\n");

    ECHO(F("\r\n[postREST] Send request........................."));
    ECHO(request);
    ECHO(F("[postREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::putREST(String host, String path, String type, String data) {
    String request = "";
    request += F("PUT /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Content-Length: ");
    request += data.length();
    request += F("\r\n");
    request += F("Content-Type: ");
    request += type;
    request += F("\r\n");
    request += F("Connection: close\r\n\r\n");
    request += data;
    request += F("\r\n");

    ECHO(F("\r\n[putREST] Send request........................."));
    ECHO(request);
    ECHO(F("[putREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::deleteREST(String host, String path) {
    String request = "";
    request += F("DELETE /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += hostname;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Connection: close\r\n\r\n");

    ECHO(F("\r\n[deleteREST] Send request........................."));
    ECHO(request);
    ECHO(F("[deleteREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::readLine() {
    for (int i = 0; i < DATA_BUFFER_LEN; i++) {
        databuffer[i] = ' ';
    }
    dataptr = databuffer;
    while (internets.available() && (dataptr < &databuffer[DATA_BUFFER_LEN - 2])) {
        char c = internets.read();
        if (c == 0) {
            ECHO("");
        } else if (c == 255) {
            ECHO("");
        } else if (c == '\r') {
            ;
        } else if (c == '\n') break;
        else *dataptr++ = c;
    }
    *dataptr = 0;
}

void SocketIOClient::emit(String id, String data) {
    String message = "42[\"" + id + "\"," + data + "]";
    ECHO("[emit] " + message);
    int header[10];
    header[0] = 0x81;
    int msglength = message.length();
    ECHO("[emit] " + String(msglength));
    randomSeed(analogRead(0));
    String mask = "";
    String masked = message;
    for (int i = 0; i < 4; i++) {
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < msglength; i++) {
        masked[i] = message[i] ^ mask[i % 4];
    }

    String request = "";
//    internets.print((char) header[0]); // has to be sent for proper communication
    request += String((char) header[0]); // has to be sent for proper communication
    // Depending on the size of the message
    if (msglength <= 125) {
        header[1] = msglength + 128;
        request += String((char) header[1]); //size of the message + 128 because message has to be masked
    } else if (msglength >= 126 && msglength <= 65535) {
        header[1] = 126 + 128;
        request += String((char) header[1]);
        header[2] = (msglength >> 8) & 255;
        request += String((char) header[2]);
        header[3] = (msglength)& 255;
        request += String((char) header[3]);
    } else {
        header[1] = 127 + 128;
        request += String((char) header[1]);
        header[2] = (msglength >> 56) & 255;
        request += String((char) header[2]);
        header[3] = (msglength >> 48) & 255;
        request += String((char) header[4]);
        header[4] = (msglength >> 40) & 255;
        request += String((char) header[4]);
        header[5] = (msglength >> 32) & 255;
        request += String((char) header[5]);
        header[6] = (msglength >> 24) & 255;
        request += String((char) header[6]);
        header[7] = (msglength >> 16) & 255;
        request += String((char) header[7]);
        header[8] = (msglength >> 8) & 255;
        request += String((char) header[8]);
        header[9] = (msglength)& 255;
        request += String((char) header[9]);
    }
    request += String(mask);
    request += String(masked);
    internets.print(request);
}

void SocketIOClient::heartbeat(int select) {
    randomSeed(analogRead(0));
    String mask = "";
    String masked = "";
    String message = "";
    if (select == 0) {
        masked = "2";
        message = "2";
    } else {
        masked = "3";
        message = "3";
    }
    for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < message.length(); i++) {
        masked[i] = message[i] ^ mask[i % 4]; //apply the "mask" to the message ("2" : ping or "3" : pong)
    }
    
    String request = "";
    request += String((char) 0x81); //has to be sent for proper communication
    request += String((char) 129); //size of the message (1) + 128 because message has to be masked
    request += String(mask);
    request += String(masked);
    internets.print(request);
}

void SocketIOClient::on(String id, functionPointer function) {
    if (onIndex == MAX_ON_HANDLERS) {
        return;
    } // oops... to many...
    onFunction[onIndex] = function;
    onId[onIndex] = id;
    onIndex++;
}

