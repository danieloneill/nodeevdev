# nodeevdev
Basic libevdev binding for node.js

### Example

```javascript
#!/usr/bin/env node

var Evdev = require('nodeevdev');

var dev = Evdev.create();
dev.open( '/dev/input/event1', function(obj, err) {
	if( err ) {
		console.log("Evdev: Failed to open device: "+err);
		return false;
	}
	dev['data'] = function(data) {
		console.log("Evdev event: "+JSON.stringify(data, null, 2));
	};
	dev['closed'] = function() {
		console.log("Evdev device closed.");
	};
});

// Hack to prevent Node.js from exiting, sorry.
var http = require('http');
var httpServer = http.createServer(function(){});
httpServer.listen(8084);
console.log("Listening for keyboard input...");
```

# Methods

## Factory

### evdev::create()
`Returns an initialised evdev object.`

* Returns: An initialised evdev object.

---

## evdev object

### evdevObject::open( path, callback( evdevObject, error ) )
`Open an evdev device, associate it to this object, and fire the provided callback upon completion.`

The parameters passed to the provided callback function are **evdevObject** which is a reference to the evdev object, and **error** which will be an error string in the case of an error, or undefined on success.

* Returns: A reference to itself.

### evdevObject::data( evdevEvent )
`A virtual method of the evdevObject. Reimplement it to handle evdevEvents. It can be ignored, but if you ignore it, you might as well not use this module at all.`

### evdevObject::closed( evdevObject )
`A virtual method of the evdevObject. Reimplement it to react to the device being closed. Otherwise it can be ignored.`

