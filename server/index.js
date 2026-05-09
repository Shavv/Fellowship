const enet = require('enet');

const PORT = 3000;
const HOST = '0.0.0.0';

// Track connected clients
const clients = new Map();

enet.createServer({
    address: { address: HOST, port: PORT },
    peers: 32,
    channels: 2,
    down: 0,
    up: 0
}, function(err, server) {
    if (err) {
        console.error("Failed to create ENet server:", err);
        process.exit(1);
    }

    console.log(`Skyrim Multiplayer Server (ENet) listening on ${HOST}:${PORT}`);

    server.on("connect", function(peer, data) {
        const clientKey = `${peer.address().address}:${peer.address().port}`;
        console.log(`\n***************************************`);
        console.log(`[CONNECT] New Client: ${clientKey}`);
        console.log(`***************************************\n`);

        // Register client
        clients.set(clientKey, {
            peer: peer,
            id: clientKey,
            x: 0, y: 0, z: 0, rot: 0
        });

        peer.on("message", function(packet, channel) {
            try {
                const msgString = packet.data().toString().replace(/\0/g, ''); 
                const parsed = JSON.parse(msgString);
                
                // Only log pos once every few seconds to avoid spamming the console
                if (parsed.type === "pos") {
                    if (!this.lastPosLog || Date.now() - this.lastPosLog > 2000) {
                        console.log(`[Server] Syncing ${clients.size} players... (Last from ${clientKey})`);
                        this.lastPosLog = Date.now();
                    }
                } else {
                    console.log(`[Server] Received ${parsed.type} from ${clientKey}`);
                }

                // Inject the sender's ID into the payload
                parsed.id = clientKey;

                const broadcastData = JSON.stringify(parsed);
                const broadcastPacket = new enet.Packet(Buffer.from(broadcastData), enet.PACKET_FLAG.RELIABLE);
                
                clients.forEach((c, key) => {
                    if (key !== clientKey) {
                        console.log(`[Server] Broadcasting ${parsed.type} from ${clientKey} to ${key}`);
                        c.peer.send(channel, broadcastPacket);
                    }
                });

            } catch (e) {
                console.error("Failed to parse client message:", packet.data().toString());
            }
        });

        peer.on("disconnect", function() {
            console.log(`\n[DISCONNECT] Client left: ${clientKey}`);
            clients.delete(clientKey);
            console.log(`Active Clients: ${clients.size}\n`);
        });
    });

    // Start polling the host for events every 10ms
    server.start(10);
});
