const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const PORT = 3000;

// Track connected clients: { address, port, lastUpdate, x, y, z, rot }
const clients = new Map();

server.on('error', (err) => {
    console.error(`Server error:\n${err.stack}`);
    server.close();
});

server.on('message', (msg, rinfo) => {
    const clientKey = `${rinfo.address}:${rinfo.port}`;
    
    // Process incoming message (Assuming JSON for simplicity, though binary is better for games)
    try {
        const data = JSON.parse(msg.toString());
        
        // Update client state
        clients.set(clientKey, {
            address: rinfo.address,
            port: rinfo.port,
            lastUpdate: Date.now(),
            x: data.x || 0,
            y: data.y || 0,
            z: data.z || 0,
            rot: data.rot || 0
        });

        // Broadcast this player's data to everyone else
        const broadcastData = JSON.stringify({
            id: clientKey, // simple identifier
            x: data.x,
            y: data.y,
            z: data.z,
            rot: data.rot
        });

        clients.forEach((client, key) => {
            if (key !== clientKey) { // Don't send data back to the sender
                server.send(broadcastData, client.port, client.address);
            }
        });

    } catch (e) {
        console.error('Failed to parse message:', msg.toString());
    }
});

server.on('listening', () => {
    const address = server.address();
    console.log(`Skyrim Multiplayer Server listening on ${address.address}:${address.port}`);
});

// Clean up disconnected clients (timeout after 5 seconds)
setInterval(() => {
    const now = Date.now();
    for (const [key, client] of clients.entries()) {
        if (now - client.lastUpdate > 5000) {
            console.log(`Client disconnected: ${key}`);
            clients.delete(key);
        }
    }
}, 1000);

server.bind(PORT);
