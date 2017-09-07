'use strict';

var net = require('net');
Buffer = require('buffer').Buffer;

var addr = 'XXXX';
var port = XXXX;

/**
 * Utility functions
 */

function log(title, msg) {
    console.log(`[${title}] ${msg}`);
}

function forwardToHub(request, callback) {
    var str = JSON.stringify(request);
    console.log(`forwardToHub: ${str}`);
    
    var client = new net.Socket();
    client.connect(port, addr, function() {
        log('DEBUG', 'TCP Connected');
        client.write(str + "\r\n\r\n");
    })
    .on("data", function(data) {
        var str = data.toString();
        var response = JSON.parse(str);
        log('DEBUG', `Hub response: ${response}`);
        client.destroy();
        
        callback(null, response);
    });
}

/**
 * Main entry point.
 * Incoming events from Alexa service through Smart Home API are all handled by this function.
 *
 * It is recommended to validate the request and response with Alexa Smart Home Skill API Validation package.
 *  https://github.com/alexa/alexa-smarthome-validation
 */
exports.handler = (request, context, callback) => {
    forwardToHub(request, callback);
};

